// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Client/VeyraClientFlowSubsystem.h"

#include "Backend/VeyraBackendClient.h"
#include "Backend/VeyraBackendProtocol.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameMapsSettings.h"
#include "GeneralProjectSettings.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Handoff/VeyraHandoff.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectGlobals.h"
#include "VeyraGameState.h"
#include "VeyraLocalPlayer.h"
#include "VeyraServicesSettings.h"
#include "VeyraSettingsSubsystem.h"

bool UVeyraClientFlowSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	FString Channel;
	return !IsRunningDedicatedServer() && FParse::Value(FCommandLine::Get(), VeyraHandoff::LaunchCodeSwitch, Channel);
}

void UVeyraClientFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// The player's settings are ready before sign-in takes the account's.
	Collection.InitializeDependency<UVeyraSettingsSubsystem>();
	// Loaded before any world starts rather than on the first request.
	FModuleManager::Get().LoadModuleChecked(TEXT("HTTP"));

	const UVeyraServicesSettings& Settings = *GetDefault<UVeyraServicesSettings>();
	FString BuildVersion;
	const FString ConfigurationProblem = FindConfigurationProblem(BuildVersion);
	Handshake = MakeUnique<FVeyraPipeLineWriter>(FVeyraPipeLineWriter::ForStandardOutput());
	if (ConfigurationProblem.IsEmpty())
	{
		LaunchCodeReader = MakeUnique<FVeyraPipeLineReader>(FVeyraPipeLineReader::ForStandardInput());
	}
	Backend = MakeUnique<FVeyraBackendClient>(Settings.BackendBaseUrl, Settings.RequestTimeoutSeconds);
	Flow = MakeUnique<FVeyraClientFlow>(*Backend, *this, FVeyraClientFlowConfig::FromSettings(Settings, BuildVersion));
	Flow->SyncAccountSettings(*this);

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVeyraClientFlowSubsystem::Tick));
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UVeyraClientFlowSubsystem::OnPostLoadMap);
	NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UVeyraClientFlowSubsystem::OnNetworkFailure);
	TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UVeyraClientFlowSubsystem::OnTravelFailure);
	Flow->Start(ConfigurationProblem);
}

void UVeyraClientFlowSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	if (AVeyraGameState* GameState = WatchedGameState.Get())
	{
		GameState->OnPhaseChanged.Remove(PhaseHandle);
	}
	// Whoever holds the client lets go while it is still valid.
	ClientEnding.Broadcast();
	ClientEnding.Clear();
	// The flow first: answers still in flight then find it gone.
	Flow.Reset();
	Backend.Reset();
	LaunchCodeReader.Reset();
	Handshake.Reset();
	Super::Deinitialize();
}

FString UVeyraClientFlowSubsystem::FindConfigurationProblem(FString& OutBuildVersion) const
{
	FString Channel;
	FParse::Value(FCommandLine::Get(), VeyraHandoff::LaunchCodeSwitch, Channel);
	if (!Channel.Equals(VeyraHandoff::StandardInput, ESearchCase::CaseSensitive))
	{
		return TEXT("-VeyraLaunchCode accepts only \"stdin\"; a launch code never goes on a command line");
	}
	if (const TArray<FString> Problems = GetDefault<UVeyraServicesSettings>()->Validate(); !Problems.IsEmpty())
	{
		return TEXT("the Veyra Services settings are invalid: ") + FString::Join(Problems, TEXT("; "));
	}
	OutBuildVersion = GetDefault<UGeneralProjectSettings>()->ProjectVersion;
	if (!VeyraBackendProtocol::IsBuildVersion(OutBuildVersion))
	{
		return TEXT("the project version is not a build version the backend accepts");
	}
	return FString();
}

bool UVeyraClientFlowSubsystem::Tick(float /*DeltaSeconds*/)
{
	Flow->Tick();
	WatchGameState();
	return true;
}

void UVeyraClientFlowSubsystem::WatchGameState()
{
	const UWorld* World = GetGameInstance()->GetWorld();
	AVeyraGameState* GameState = World && World->GetNetMode() == NM_Client ? World->GetGameState<AVeyraGameState>() : nullptr;
	if (GameState == WatchedGameState.Get())
	{
		return;
	}
	if (AVeyraGameState* Previous = WatchedGameState.Get())
	{
		Previous->OnPhaseChanged.Remove(PhaseHandle);
	}
	PhaseHandle.Reset();
	WatchedGameState = GameState;
	if (GameState)
	{
		PhaseHandle = GameState->OnPhaseChanged.AddWeakLambda(this, [this](EVeyraMatchPhase Phase) { Flow->NotifyMatchPhase(Phase); });
		// The phase may have replicated before the game state was found.
		Flow->NotifyMatchPhase(GameState->GetPhase());
	}
}

bool UVeyraClientFlowSubsystem::IsOurs(const UWorld* World) const
{
	return World && World->GetGameInstance() == GetGameInstance();
}

void UVeyraClientFlowSubsystem::OnPostLoadMap(UWorld* World)
{
	if (IsOurs(World))
	{
		Flow->NotifyWorld(World->GetNetMode() == NM_Client ? EVeyraClientWorld::Match : EVeyraClientWorld::FrontEnd);
	}
}

void UVeyraClientFlowSubsystem::OnNetworkFailure(UWorld* World, UNetDriver* /*NetDriver*/, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (!World || IsOurs(World))
	{
		Flow->NotifyConnectionFailed(FString::Printf(TEXT("%s: %s"), ENetworkFailure::ToString(FailureType), *ErrorString));
	}
}

void UVeyraClientFlowSubsystem::OnTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (!World || IsOurs(World))
	{
		Flow->NotifyConnectionFailed(FString::Printf(TEXT("%s: %s"), ETravelFailure::ToString(FailureType), *ErrorString));
	}
}

double UVeyraClientFlowSubsystem::Now() const
{
	return FPlatformTime::Seconds();
}

EVeyraPipeRead UVeyraClientFlowSubsystem::PollLaunchCode(FString& OutLine)
{
	if (!LaunchCodeReader)
	{
		return EVeyraPipeRead::Failed;
	}
	const EVeyraPipeRead Read = LaunchCodeReader->Poll(OutLine);
	if (Read != EVeyraPipeRead::Pending)
	{
		// One line is all standard input carries.
		LaunchCodeReader.Reset();
	}
	return Read;
}

void UVeyraClientFlowSubsystem::WriteHandshake(const FString& Line)
{
	Handshake->WriteLine(Line);
}

bool UVeyraClientFlowSubsystem::TravelToMatch(const FString& Address, const FString& Ticket)
{
	UGameInstance* GameInstance = GetGameInstance();
	UVeyraLocalPlayer* Player = Cast<UVeyraLocalPlayer>(GameInstance->GetFirstGamePlayer());
	if (!Player)
	{
		return false;
	}
	// The engine asks for the ticket only once the server answers, so it never enters the travel URL.
	Player->SetJoinTicket(Ticket);
	GEngine->SetClientTravel(GameInstance->GetWorld(), *Address, TRAVEL_Absolute);
	return true;
}

void UVeyraClientFlowSubsystem::TravelToFrontEnd()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (UVeyraLocalPlayer* Player = Cast<UVeyraLocalPlayer>(GameInstance->GetFirstGamePlayer()))
	{
		Player->ClearJoinTicket();
	}
	GEngine->SetClientTravel(GameInstance->GetWorld(), *UGameMapsSettings::GetGameDefaultMap(), TRAVEL_Absolute);
}

void UVeyraClientFlowSubsystem::QuitGame()
{
	FPlatformMisc::RequestExit(/*bForce*/ false, TEXT("VeyraClientFlow"));
}

UVeyraSettingsSubsystem* UVeyraClientFlowSubsystem::FindSettings() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	UVeyraSettingsSubsystem* Found = GameInstance ? GameInstance->GetSubsystem<UVeyraSettingsSubsystem>() : nullptr;
	return Found && Found->IsReady() ? Found : nullptr;
}

void UVeyraClientFlowSubsystem::UseAccount(const FString& AccountId)
{
	if (UVeyraSettingsSubsystem* Found = FindSettings())
	{
		Found->UseAccount(AccountId);
	}
}

FVeyraAccountSettingsDocument UVeyraClientFlowSubsystem::GetDocument() const
{
	const UVeyraSettingsSubsystem* Found = FindSettings();
	return Found ? Found->GetAccountDocument() : FVeyraAccountSettingsDocument();
}

void UVeyraClientFlowSubsystem::TakeDocument(const FVeyraAccountSettingsDocument& Document)
{
	if (UVeyraSettingsSubsystem* Found = FindSettings())
	{
		Found->TakeAccountDocument(Document);
	}
}

void UVeyraClientFlowSubsystem::MarkSent(int64 Revision, uint32 SentChangeCount)
{
	if (UVeyraSettingsSubsystem* Found = FindSettings())
	{
		Found->MarkAccountSent(Revision, SentChangeCount);
	}
}

bool UVeyraClientFlowSubsystem::HasUnsentChanges() const
{
	const UVeyraSettingsSubsystem* Found = FindSettings();
	return Found && Found->HasUnsentAccountChanges();
}

uint32 UVeyraClientFlowSubsystem::GetChangeCount() const
{
	const UVeyraSettingsSubsystem* Found = FindSettings();
	return Found ? Found->GetAccountChangeCount() : 0;
}
