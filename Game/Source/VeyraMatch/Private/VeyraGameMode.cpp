// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraGameMode.h"

#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "Tuning/VeyraTuning.h"
#include "VeyraCombatVerbs.h"
#include "VeyraGameState.h"
#include "VeyraJoinRules.h"
#include "VeyraMatchLog.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"
#include "VeyraTeamStart.h"
#include "VeyraVanguardCharacter.h"
#include "VeyraVanguardController.h"

namespace
{
	/** The sides that field Vanguards. */
	constexpr EVeyraTeam Sides[] = { EVeyraTeam::A, EVeyraTeam::B };

	// Until the backend supplies a roster (M4), a developer server is told how many humans to wait for.
	TAutoConsoleVariable<int32> CVarExpectedPlayers(
		TEXT("veyra.Match.ExpectedPlayers"), 0,
		TEXT("Humans a developer match waits for during loading when the server URL has no VeyraExpectedPlayers option. ")
		TEXT("0 waits for the loading timeout."),
		ECVF_Default);
}

AVeyraGameMode::AVeyraGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameStateClass = AVeyraGameState::StaticClass();
	PlayerStateClass = AVeyraPlayerState::StaticClass();
	PlayerControllerClass = AVeyraPlayerController::StaticClass();
	VanguardClass = AVeyraVanguardCharacter::StaticClass();

	// Players never get an engine-spawned pawn or spectator: their Vanguard is spawned by this class.
	DefaultPawnClass = nullptr;
	SpectatorClass = nullptr;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AVeyraGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	ExpectedPlayers = UGameplayStatics::GetIntOption(Options, ExpectedPlayersOption, CVarExpectedPlayers.GetValueOnGameThread());
}

void AVeyraGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (ErrorMessage.IsEmpty())
	{
		ErrorMessage = VeyraJoinRules::CheckDirectConnect();
	}
	if (ErrorMessage.IsEmpty())
	{
		ErrorMessage = VeyraJoinRules::CheckTuningHash(Options, VeyraTuning::GetCompositeHash());
	}
	if (ErrorMessage.IsEmpty())
	{
		int32 Participants = 0;
		for (const EVeyraTeam Side : Sides)
		{
			Participants += CountTeamMembers(Side);
		}
		if (Participants >= static_cast<int32>(UE_ARRAY_COUNT(Sides)) * UVeyraMatchTuningSubsystem::Get().Teams.MaxTeamSize)
		{
			ErrorMessage = TEXT("The match is full.");
		}
	}
	if (!ErrorMessage.IsEmpty())
	{
		UE_LOG(LogVeyraMatch, Warning, TEXT("Refused a connection from %s: %s"), *Address, *ErrorMessage);
	}
}

void AVeyraGameMode::StartPlay()
{
	Super::StartPlay();

	GetVeyraGameState().SetPhase(EVeyraMatchPhase::Loading);
	GetWorldTimerManager().SetTimer(LoadingTimeout, this, &AVeyraGameMode::OnLoadingTimedOut,
		static_cast<float>(UVeyraMatchTuningSubsystem::Get().Phases.LoadingTimeoutSeconds));
	SetActorTickEnabled(true);
	// Game/Scripts/Smoke.ps1 waits for this line.
	UE_LOG(LogVeyraMatch, Display, TEXT("Match server ready: loading, waiting for %d player(s)."), ExpectedPlayers);
}

void AVeyraGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Only loading ticks: it ends once the map is ready and every expected human has joined, or the
	// timeout has passed.
	if (GetVeyraGameState().GetPhase() == EVeyraMatchPhase::Loading && IsMapReady() && (bLoadingTimedOut || HaveExpectedPlayersJoined()))
	{
		BeginPreparation();
	}
}

EVeyraOrderRejection AVeyraGameMode::CheckOrdersAllowed() const
{
	if (GetWorld()->IsPaused())
	{
		return EVeyraOrderRejection::Paused;
	}
	// Preparation should allow movement inside the fountain (Match Flow Bible §1, stage 3). Until
	// the map has fountain areas, it refuses orders instead.
	return GetVeyraGameState().GetPhase() == EVeyraMatchPhase::Live ? EVeyraOrderRejection::None : EVeyraOrderRejection::WrongPhase;
}

EVeyraOrderRejection AVeyraGameMode::HandleMoveOrder(AVeyraPlayerController& Player, const FVector& Destination)
{
	const EVeyraOrderRejection Allowed = CheckOrdersAllowed();
	if (Allowed != EVeyraOrderRejection::None)
	{
		return Allowed;
	}
	if (Destination.ContainsNaN() || !FMath::IsFinite(Destination.X) || !FMath::IsFinite(Destination.Y) || !FMath::IsFinite(Destination.Z))
	{
		return EVeyraOrderRejection::InvalidOrder;
	}
	const AVeyraPlayerState* PlayerState = Player.GetPlayerState<AVeyraPlayerState>();
	AVeyraVanguardController* Controller = PlayerState ? PlayerState->GetVanguardController() : nullptr;
	return Controller ? Controller->MoveToDestination(Destination) : EVeyraOrderRejection::NoVanguard;
}

bool AVeyraGameMode::PauseMatch(APlayerController& Requester)
{
	// The engine's world pause stops every gameplay clock on the server; the GameState tells clients.
	if (GetVeyraGameState().IsMatchPaused() || !SetPause(&Requester))
	{
		return false;
	}
	GetVeyraGameState().SetMatchPaused(true);
	UE_LOG(LogVeyraMatch, Log, TEXT("Match paused by %s."), *GetNameSafe(Requester.PlayerState));
	return true;
}

bool AVeyraGameMode::ResumeMatch()
{
	if (!GetVeyraGameState().IsMatchPaused() || !ClearPause())
	{
		return false;
	}
	GetVeyraGameState().SetMatchPaused(false);
	UE_LOG(LogVeyraMatch, Log, TEXT("Match resumed."));
	return true;
}

void AVeyraGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Not the engine's version, which would spawn a default pawn for the player.
	AVeyraPlayerState* PlayerState = NewPlayer ? NewPlayer->GetPlayerState<AVeyraPlayerState>() : nullptr;
	if (!PlayerState)
	{
		UE_LOG(LogVeyraMatch, Error, TEXT("%s has no Veyra PlayerState and cannot join the match."), *GetNameSafe(NewPlayer));
		return;
	}
	AssignTeam(*PlayerState);
	if (GetVeyraGameState().GetPhase() != EVeyraMatchPhase::Loading)
	{
		SpawnVanguard(*PlayerState);
	}
}

bool AVeyraGameMode::PlayerCanRestart_Implementation(APlayerController* /*Player*/)
{
	// Respawning belongs to the match, never to a client request.
	return false;
}

bool AVeyraGameMode::CanSpectate_Implementation(APlayerController* /*Viewer*/, APlayerState* /*ViewTarget*/)
{
	return false;
}

AVeyraGameState& AVeyraGameMode::GetVeyraGameState() const
{
	return *CastChecked<AVeyraGameState>(GameState);
}

int32 AVeyraGameMode::CountTeamMembers(EVeyraTeam Team) const
{
	int32 Count = 0;
	for (const APlayerState* Member : GameState->PlayerArray)
	{
		Count += VeyraTeams::TeamOf(Member) == Team ? 1 : 0;
	}
	return Count;
}

void AVeyraGameMode::AssignTeam(AVeyraPlayerState& PlayerState) const
{
	// The smaller side, or Team A on a tie. PreLogin has already refused anyone beyond capacity.
	const EVeyraTeam Team = CountTeamMembers(EVeyraTeam::B) < CountTeamMembers(EVeyraTeam::A) ? EVeyraTeam::B : EVeyraTeam::A;
	PlayerState.SetVeyraTeam(Team);
	UE_LOG(LogVeyraMatch, Log, TEXT("%s joins Team %s."), *PlayerState.GetPlayerName(), Team == EVeyraTeam::A ? TEXT("A") : TEXT("B"));
}

AActor* AVeyraGameMode::FindTeamStart(EVeyraTeam Team) const
{
	for (TActorIterator<AVeyraTeamStart> It(GetWorld()); It; ++It)
	{
		if (It->GetVeyraTeam() == Team)
		{
			return *It;
		}
	}
	return nullptr;
}

bool AVeyraGameMode::IsMapReady() const
{
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Navigation || Navigation->IsNavigationBuildInProgress())
	{
		return false;
	}
	for (const EVeyraTeam Side : Sides)
	{
		const AActor* Start = FindTeamStart(Side);
		FNavLocation Walkable;
		if (!Start || !Navigation->ProjectPointToNavigation(Start->GetActorLocation(), Walkable,
			FVector(UVeyraMatchTuningSubsystem::Get().Orders.DestinationProjectionExtent)))
		{
			return false;
		}
	}
	return true;
}

bool AVeyraGameMode::HaveExpectedPlayersJoined()
{
	return ExpectedPlayers > 0 && GetNumPlayers() >= ExpectedPlayers;
}

void AVeyraGameMode::OnLoadingTimedOut()
{
	bLoadingTimedOut = true;
	UE_CLOG(!IsMapReady(), LogVeyraMatch, Error, TEXT("Loading timed out, but the map is not ready: it needs a start for each side on built navigation."));
}

void AVeyraGameMode::BeginPreparation()
{
	SetActorTickEnabled(false);
	GetWorldTimerManager().ClearTimer(LoadingTimeout);
	GetVeyraGameState().SetPhase(EVeyraMatchPhase::Preparation);
	UE_LOG(LogVeyraMatch, Log, TEXT("Preparation begins with %d player(s)."), GetNumPlayers());

	for (APlayerState* Member : GameState->PlayerArray)
	{
		if (AVeyraPlayerState* PlayerState = Cast<AVeyraPlayerState>(Member))
		{
			SpawnVanguard(*PlayerState);
		}
	}
	GetWorldTimerManager().SetTimer(PreparationTimer, this, &AVeyraGameMode::BeginLive,
		static_cast<float>(UVeyraMatchTuningSubsystem::Get().Phases.PreparationSeconds));
}

void AVeyraGameMode::BeginLive()
{
	GetVeyraGameState().SetPhase(EVeyraMatchPhase::Live);
	UE_LOG(LogVeyraMatch, Log, TEXT("The match is live."));
}

void AVeyraGameMode::SpawnVanguard(AVeyraPlayerState& PlayerState)
{
	AActor* Start = FindTeamStart(PlayerState.GetVeyraTeam());
	UAbilitySystemComponent* AbilitySystem = PlayerState.GetAbilitySystemComponent();
	if (!Start || !AbilitySystem)
	{
		UE_LOG(LogVeyraMatch, Error, TEXT("Cannot spawn a Vanguard for %s: its side has no start."), *PlayerState.GetPlayerName());
		return;
	}

	if (!PlayerState.HasInitializedStats())
	{
		const FVeyraDeveloperLoadoutTuning& Loadout = UVeyraMatchTuningSubsystem::Get().DeveloperLoadout;
		if (!VeyraCombat::InitializeVitals(*AbilitySystem, Loadout.MaxHealth) || !VeyraCombat::InitializeResource(*AbilitySystem, Loadout.MaxResource)
			|| !VeyraCombat::InitializeMoveSpeed(*AbilitySystem, Loadout.MoveSpeed))
		{
			return;
		}
		PlayerState.MarkStatsInitialized();
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AVeyraVanguardCharacter* Vanguard = GetWorld()->SpawnActor<AVeyraVanguardCharacter>(VanguardClass, Start->GetActorTransform(), SpawnParameters);
	if (!Vanguard)
	{
		UE_LOG(LogVeyraMatch, Error, TEXT("Failed to spawn a Vanguard for %s."), *PlayerState.GetPlayerName());
		return;
	}

	AVeyraVanguardController* Controller = PlayerState.GetVanguardController();
	if (!Controller)
	{
		FActorSpawnParameters ControllerParameters;
		ControllerParameters.ObjectFlags |= RF_Transient;
		Controller = GetWorld()->SpawnActor<AVeyraVanguardController>(ControllerParameters);
		PlayerState.SetVanguardController(Controller);
	}
	Controller->Possess(Vanguard);
	// An AI controller has no PlayerState of its own, so possession leaves the pawn without one.
	Vanguard->SetPlayerState(&PlayerState);
}
