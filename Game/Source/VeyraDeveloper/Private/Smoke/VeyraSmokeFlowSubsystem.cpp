// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Smoke/VeyraSmokeFlowSubsystem.h"

#include "Client/VeyraClientFlowSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "VeyraGameState.h"
#include "VeyraPlayerController.h"
#include "VeyraVanguardCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogVeyraSmokeFlow, Log, All);

namespace
{
	const TCHAR* const FlowSwitch = TEXT("VeyraSmokeFlow=");
	const TCHAR* const VanguardSwitch = TEXT("VeyraSmokeFlowVanguard=");
	// Harness settings, not gameplay: how long the whole practice script may take, how far the
	// Vanguard must walk toward the lane centre before its host ends the match, and what the verified
	// result of a practice its host ended must say (ADR-010 §7).
	constexpr double FlowTimeoutRealSeconds = 300.0;
	constexpr double MoveProofDistance = 100.0;
	const TCHAR* const ExpectedEndReason = TEXT("host_ended");
	const TCHAR* const ExpectedRules = TEXT("practice");
}

bool UVeyraSmokeFlowSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	FString Mode;
	return !IsRunningDedicatedServer() && FParse::Value(FCommandLine::Get(), FlowSwitch, Mode);
}

void UVeyraSmokeFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FString Mode;
	FParse::Value(FCommandLine::Get(), FlowSwitch, Mode);
	FParse::Value(FCommandLine::Get(), VanguardSwitch, WantedVanguard);
	bPractice = Mode.Equals(TEXT("practice"), ESearchCase::CaseSensitive);
	StartRealTime = FPlatformTime::Seconds();
	if (!bPractice && !Mode.Equals(TEXT("join"), ESearchCase::CaseSensitive))
	{
		Finish(false, FString::Printf(TEXT("-VeyraSmokeFlow takes \"practice\" or \"join\", not \"%s\""), *Mode));
		return;
	}
	UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: started the %s flow script."), *Mode);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVeyraSmokeFlowSubsystem::Tick));
}

void UVeyraSmokeFlowSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	Super::Deinitialize();
}

bool UVeyraSmokeFlowSubsystem::Tick(float /*DeltaSeconds*/)
{
	if (bFinished)
	{
		return false;
	}
	UVeyraClientFlowSubsystem* Flow = GetGameInstance()->GetSubsystem<UVeyraClientFlowSubsystem>();
	if (!Flow)
	{
		Finish(false, TEXT("the game has no client flow; start it with -VeyraLaunchCode=stdin"));
		return false;
	}
	if (!bPractice)
	{
		// A script created the match before the game started, so it waits behind Reconnect (UX-17).
		if (Flow->CanIssue(EVeyraClientIntent::Reconnect))
		{
			UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: pressed Reconnect."));
			Flow->Reconnect();
			return false;
		}
		return true;
	}
	if (FPlatformTime::Seconds() - StartRealTime > FlowTimeoutRealSeconds)
	{
		Finish(false, FString::Printf(TEXT("timed out in %s"), LexToString(Flow->GetSnapshot().State)));
		return false;
	}
	TickPractice(*Flow);
	return !bFinished;
}

void UVeyraSmokeFlowSubsystem::TickPractice(UVeyraClientFlowSubsystem& Flow)
{
	const FVeyraClientSnapshot& Snapshot = Flow.GetSnapshot();
	if (Snapshot.Problem.IsSet())
	{
		Finish(false, FString::Printf(TEXT("the flow shows a problem in %s: %s (%s)"), LexToString(Snapshot.State), *Snapshot.Problem->Message, *Snapshot.Problem->Code));
		return;
	}

	switch (Snapshot.State)
	{
	case EVeyraClientState::ReconnectOnly:
		Finish(false, FString::Printf(TEXT("the player is still in match %s from an earlier run; try again once it ends"), *Snapshot.MatchId));
		break;

	case EVeyraClientState::StarterChoice:
		if (Flow.CanIssue(EVeyraClientIntent::ChooseStarter))
		{
			const FString Starter = ChooseFrom(Snapshot.Starters);
			if (Starter.IsEmpty())
			{
				Finish(false, TEXT("no starter is on offer"));
				break;
			}
			UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: choosing %s as the starter."), *Starter);
			Flow.ChooseStarter(Starter);
		}
		break;

	case EVeyraClientState::Shell:
		if (bSawResults)
		{
			Finish(true, FString::Printf(TEXT("chose a starter as needed, started practice, locked %s, ended the match as its host, saw its verified result and returned to the shell"),
				*LockedVanguard));
		}
		else if (!bStartedPractice && Flow.CanIssue(EVeyraClientIntent::StartPractice))
		{
			bStartedPractice = true;
			UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: Play, Custom, Practice."));
			Flow.StartPractice();
		}
		else if (bStartedPractice && !Snapshot.Notice.IsEmpty())
		{
			Finish(false, FString::Printf(TEXT("the practice select was cancelled (%s)"), *Snapshot.Notice));
		}
		break;

	case EVeyraClientState::Selecting:
		if (Flow.CanIssue(EVeyraClientIntent::HoverVanguard))
		{
			const FString Pick = ChooseFrom(Snapshot.AvailableVanguards);
			if (Pick.IsEmpty())
			{
				Finish(false, TEXT("the wanted Vanguard is not available"));
				break;
			}
			if (Snapshot.Select.FindYou()->Hover != Pick)
			{
				UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: hovering %s with %.0f s on the timer."), *Pick, Flow.GetRemainingPickSeconds());
				Flow.HoverVanguard(Pick);
			}
			else
			{
				LockedVanguard = Pick;
				UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: locking in %s."), *Pick);
				Flow.LockVanguard(Pick);
			}
		}
		break;

	case EVeyraClientState::InMatch:
	{
		const UWorld* World = GetGameInstance()->GetWorld();
		AVeyraPlayerController* Controller = Cast<AVeyraPlayerController>(GetGameInstance()->GetFirstLocalPlayerController());
		const AVeyraGameState* GameState = World ? World->GetGameState<AVeyraGameState>() : nullptr;
		const AActor* Vanguard = Controller ? Controller->GetVanguard() : nullptr;
		if (!Controller || !GameState || !Vanguard || GameState->GetPhase() != EVeyraMatchPhase::Live)
		{
			break;
		}
		if (!bOrderedMove)
		{
			// Play a little: walk toward the lane centre.
			bOrderedMove = true;
			MoveStart = Vanguard->GetActorLocation();
			UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: the practice match is live; walking toward the lane centre."));
			Controller->IssueMoveOrder(FVector(0.0, MoveStart.Y, 0.0));
		}
		else if (Controller->GetOrderRejectionCount() > 0)
		{
			Finish(false, FString::Printf(TEXT("the server refused the move: %s"), LexToString(Controller->GetLastOrderRejection())));
		}
		else if (!bAskedToEnd && FVector::Dist2D(Vanguard->GetActorLocation(), MoveStart) >= MoveProofDistance)
		{
			// The host ends the practice match (ADR-010 §7), as the in-match menu's End Custom Match does.
			bAskedToEnd = true;
			UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: the Vanguard walks; ending the match as its host."));
			Controller->RequestEndCustomMatch();
		}
		else if (bAskedToEnd && Controller->GetEndCustomMatchRefusalCount() > 0)
		{
			Finish(false, FString::Printf(TEXT("the server refused to end the custom match: %s"), LexToString(Controller->GetLastEndCustomMatchRefusal())));
		}
		break;
	}

	case EVeyraClientState::Results:
		if (!bSawResults)
		{
			const TOptional<VeyraBackendProtocol::FMatchOutcome>& Result = Snapshot.Result;
			if (!Result.IsSet() || !Result->bHasResult)
			{
				Finish(false, FString::Printf(TEXT("match %s has no verified result"), *Snapshot.MatchId));
				break;
			}
			UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: the verified result of match %s: %s, %s rules, winner %s, %.1f s, as %s, %s, %s at the end."),
				*Result->MatchId, *Result->EndReason, *Result->Rules, Result->Winner.IsEmpty() ? TEXT("none") : *Result->Winner, Result->DurationSeconds,
				*Result->VanguardId, Result->bJoined ? TEXT("joined") : TEXT("never joined"), Result->bConnectedAtEnd ? TEXT("connected") : TEXT("disconnected"));
			if (Result->EndReason != ExpectedEndReason || Result->Rules != ExpectedRules || !Result->Winner.IsEmpty() || Result->VanguardId != LockedVanguard
				|| !Result->bJoined || !Result->bConnectedAtEnd || Result->MatchId != Snapshot.MatchId)
			{
				Finish(false, TEXT("the verified result is not a host-ended practice, with no winner, of the locked Vanguard, joined and connected at the end"));
				break;
			}
			bSawResults = true;
			Flow.ContinueFromResults();
		}
		break;

	default:
		break;
	}
}

FString UVeyraSmokeFlowSubsystem::ChooseFrom(const TArray<FString>& Offered) const
{
	if (WantedVanguard.IsEmpty())
	{
		return Offered.IsEmpty() ? FString() : Offered[0];
	}
	return Offered.Contains(WantedVanguard) ? WantedVanguard : FString();
}

void UVeyraSmokeFlowSubsystem::Finish(bool bPassed, const FString& Reason)
{
	bFinished = true;
	UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: %s: %s."), bPassed ? TEXT("PASS") : TEXT("FAIL"), *Reason);
	// The line above is the result; Game/Scripts/Smoke.ps1 reads it. The script then quits the way a
	// player would.
	if (UVeyraClientFlowSubsystem* Flow = GetGameInstance()->GetSubsystem<UVeyraClientFlowSubsystem>())
	{
		Flow->Quit();
	}
	else
	{
		FPlatformMisc::RequestExit(/*bForce*/ false, TEXT("VeyraSmokeFlow"));
	}
}
