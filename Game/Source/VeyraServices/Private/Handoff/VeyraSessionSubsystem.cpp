// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Handoff/VeyraSessionSubsystem.h"

#include "Backend/VeyraBackendProtocol.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GeneralProjectSettings.h"
#include "HAL/PlatformTime.h"
#include "Handoff/VeyraHandoff.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "VeyraLocalPlayer.h"
#include "VeyraServicesLog.h"
#include "VeyraServicesSettings.h"

bool UVeyraSessionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	FString Channel;
	return !IsRunningDedicatedServer() && FParse::Value(FCommandLine::Get(), VeyraHandoff::LaunchCodeSwitch, Channel);
}

void UVeyraSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Loaded before any world starts rather than on the first request.
	FModuleManager::Get().LoadModuleChecked(TEXT("HTTP"));

	FString Channel;
	FParse::Value(FCommandLine::Get(), VeyraHandoff::LaunchCodeSwitch, Channel);
	if (!Channel.Equals(VeyraHandoff::StandardInput, ESearchCase::CaseSensitive))
	{
		Fail(TEXT("-VeyraLaunchCode accepts only \"stdin\"; a launch code never goes on a command line"));
		return;
	}
	const UVeyraServicesSettings& Settings = *GetDefault<UVeyraServicesSettings>();
	if (const TArray<FString> Problems = Settings.Validate(); !Problems.IsEmpty())
	{
		Fail(TEXT("the Veyra Services settings are invalid: ") + FString::Join(Problems, TEXT("; ")));
		return;
	}
	BuildVersion = GetDefault<UGeneralProjectSettings>()->ProjectVersion;
	if (!VeyraBackendProtocol::IsBuildVersion(BuildVersion))
	{
		Fail(TEXT("the project version is not a build version the backend accepts"));
		return;
	}

	Backend = MakeUnique<FVeyraBackendClient>(Settings.BackendBaseUrl, Settings.RequestTimeoutSeconds);
	Reader = MakeUnique<FVeyraPipeLineReader>(FVeyraPipeLineReader::ForStandardInput());
	ReadDeadline = FPlatformTime::Seconds() + Settings.LaunchCodeReadTimeoutSeconds;
	Progress(TEXT("waiting for the launch code on standard input."));
	ReadTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVeyraSessionSubsystem::TickReadLaunchCode));
}

void UVeyraSessionSubsystem::Deinitialize()
{
	StopTickers();
	Super::Deinitialize();
}

bool UVeyraSessionSubsystem::TickReadLaunchCode(float /*DeltaSeconds*/)
{
	FString Line;
	const EVeyraPipeRead Read = Reader->Poll(Line);
	if (Read == EVeyraPipeRead::Pending)
	{
		if (FPlatformTime::Seconds() < ReadDeadline)
		{
			return true;
		}
		ReadTicker.Reset();
		Fail(TEXT("no launch code arrived on standard input within LaunchCodeReadTimeoutSeconds"));
		return false;
	}

	ReadTicker.Reset();
	Reader.Reset();
	switch (Read)
	{
	case EVeyraPipeRead::Line:
		if (VeyraBackendProtocol::IsLaunchCode(Line))
		{
			Redeem(Line);
		}
		else
		{
			Fail(TEXT("standard input did not hold a launch code"));
		}
		break;
	case EVeyraPipeRead::EndOfInput:
		Fail(TEXT("standard input closed without a launch code"));
		break;
	case EVeyraPipeRead::TooLong:
		Fail(TEXT("standard input held a line far too long to be a launch code"));
		break;
	case EVeyraPipeRead::NotAPipe:
		Fail(TEXT("standard input is not a pipe; whoever starts the game must pass the launch code through one"));
		break;
	default:
		Fail(TEXT("reading standard input failed"));
		break;
	}
	return false;
}

void UVeyraSessionSubsystem::Redeem(const FString& LaunchCode)
{
	Progress(TEXT("redeeming the launch code."));
	Backend->Post(TEXT("/v1/game-sessions"), FString(), VeyraBackendProtocol::BuildRedeemBody(LaunchCode, BuildVersion),
		[WeakThis = TWeakObjectPtr<UVeyraSessionSubsystem>(this)](const FVeyraBackendResponse& Response) {
			if (UVeyraSessionSubsystem* This = WeakThis.Get())
			{
				This->OnRedeemed(Response);
			}
		});
}

void UVeyraSessionSubsystem::OnRedeemed(const FVeyraBackendResponse& Response)
{
	if (!Response.IsSuccess())
	{
		// A launch code is single use, so a failed redemption is not retried.
		Fail(TEXT("the backend did not redeem the launch code: ") + Response.Describe());
		return;
	}
	VeyraBackendProtocol::FGameSession Session;
	FString Problem;
	if (!VeyraBackendProtocol::ParseGameSession(Response.Body, Session, Problem))
	{
		Fail(TEXT("the backend's answer to the launch code was not understood: ") + Problem);
		return;
	}
	GameSession = MoveTemp(Session.Token);
	Progress(FString::Printf(TEXT("signed in as %s; waiting for the match."), *Session.DisplayName));
	MatchDeadline = FPlatformTime::Seconds() + GetDefault<UVeyraServicesSettings>()->MatchWaitTimeoutSeconds;
	AskForMatch();
}

void UVeyraSessionSubsystem::AskForMatch()
{
	Backend->Get(TEXT("/v1/me/match"), GameSession,
		[WeakThis = TWeakObjectPtr<UVeyraSessionSubsystem>(this)](const FVeyraBackendResponse& Response) {
			if (UVeyraSessionSubsystem* This = WeakThis.Get())
			{
				This->OnMatchAnswer(Response);
			}
		});
}

void UVeyraSessionSubsystem::OnMatchAnswer(const FVeyraBackendResponse& Response)
{
	if (Response.IsSuccess())
	{
		VeyraBackendProtocol::FMyMatch Match;
		FString Problem;
		if (!VeyraBackendProtocol::ParseMyMatch(Response.Body, Match, Problem))
		{
			Fail(TEXT("the backend's answer about the match was not understood: ") + Problem);
			return;
		}
		if (Match.bReady)
		{
			Join(Match);
			return;
		}
		if (Match.bHasMatch && !bSawMatch)
		{
			bSawMatch = true;
			Progress(FString::Printf(TEXT("match %s is starting."), *Match.MatchId));
		}
		else if (!Match.bHasMatch && bSawMatch)
		{
			Fail(TEXT("the match ended before its server was ready"));
			return;
		}
	}
	else if (!Response.IsTransient())
	{
		Fail(TEXT("the backend did not report the match: ") + Response.Describe());
		return;
	}

	if (FPlatformTime::Seconds() >= MatchDeadline)
	{
		Fail(TEXT("the match was not ready within MatchWaitTimeoutSeconds"));
		return;
	}
	PollTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float) {
		PollTicker.Reset();
		AskForMatch();
		return false;
	}), GetDefault<UVeyraServicesSettings>()->MatchPollIntervalSeconds);
}

void UVeyraSessionSubsystem::Join(const VeyraBackendProtocol::FMyMatch& Match)
{
	UGameInstance* GameInstance = GetGameInstance();
	UVeyraLocalPlayer* Player = Cast<UVeyraLocalPlayer>(GameInstance->GetFirstGamePlayer());
	if (!Player)
	{
		Fail(TEXT("the game has no Veyra local player to carry the join ticket"));
		return;
	}
	bFinished = true;
	Player->SetJoinTicket(Match.Ticket);
	const FString Address = FString::Printf(TEXT("%s:%d"), *Match.Host, Match.Port);
	Progress(FString::Printf(TEXT("joining match %s at %s."), *Match.MatchId, *Address));
	GEngine->SetClientTravel(GameInstance->GetWorld(), *Address, TRAVEL_Absolute);
}

void UVeyraSessionSubsystem::Progress(const FString& Message) const
{
	UE_LOG(LogVeyraServices, Display, TEXT("VeyraHandoff: %s"), *Message);
}

void UVeyraSessionSubsystem::Fail(const FString& Reason)
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	StopTickers();
	UE_LOG(LogVeyraServices, Error, TEXT("VeyraHandoff: FAIL: %s."), *VeyraBackendProtocol::RedactCredentials(Reason));
	FPlatformMisc::RequestExitWithStatus(/*bForce*/ false, VeyraHandoff::FailedExitCode, TEXT("VeyraHandoff"));
}

void UVeyraSessionSubsystem::StopTickers()
{
	FTSTicker::GetCoreTicker().RemoveTicker(ReadTicker);
	FTSTicker::GetCoreTicker().RemoveTicker(PollTicker);
	ReadTicker.Reset();
	PollTicker.Reset();
}
