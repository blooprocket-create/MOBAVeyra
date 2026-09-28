// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Smoke/VeyraSmokeFlowSubsystem.h"

#include "Algo/Find.h"
#include "Client/VeyraClientFlowSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "VeyraGameState.h"
#include "VeyraPlayerController.h"
#include "VeyraVanguardCharacter.h"

#if WITH_VEYRA_UI
#include "Match/VeyraMatchMenu.h"
#include "Match/VeyraMatchMenuSubsystem.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellModels.h"
#include "Shell/VeyraShellScreen.h"
#include "Shell/VeyraShellUISubsystem.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogVeyraSmokeFlow, Log, All);

namespace
{
	using VeyraBackendProtocol::EPartyStatus;

	const TCHAR* const FlowSwitch = TEXT("VeyraSmokeFlow=");
	const TCHAR* const VanguardSwitch = TEXT("VeyraSmokeFlowVanguard=");
	const TCHAR* const ScreenshotSwitch = TEXT("VeyraSmokeFlowScreenshots=");
	const TCHAR* const EndsMatchSwitch = TEXT("VeyraSmokeFlowEndsMatch");
	// Harness settings, not gameplay: how long the whole script may take, how far the Vanguard must
	// walk toward the lane centre before the match is ended, and how long to wait for a screenshot to
	// be saved.
	constexpr double FlowTimeoutRealSeconds = 300.0;
	constexpr double MoveProofDistance = 100.0;
	constexpr double ScreenshotHoldRealSeconds = 1.0;
	// The sparring partner locks its pick once the other team has locked, or with this much of the
	// pick timer left, so it never takes the Vanguard the person was about to lock.
	constexpr double OpponentLockSeconds = 15.0;
	// What the verified result must say: a practice its host ended (ADR-010 §7), or a standard match
	// a developer ended.
	const TCHAR* const PracticeEndReason = TEXT("host_ended");
	const TCHAR* const PracticeRules = TEXT("practice");
	const TCHAR* const DeveloperEndReason = TEXT("developer_request");
	const TCHAR* const StandardRules = TEXT("standard");
	// How the coordinator explains a match found that did not go ahead.
	const TCHAR* const DeclinedNotice = TEXT("match_found_declined");
	const TCHAR* const RequeuedNotice = TEXT("match_found_requeued");
	// The labels of the buttons the script clicks, as the shell and the menu show them.
	const TCHAR* const PlayLabel = TEXT("Play");
	const TCHAR* const PracticeLabel = TEXT("Practice");
	const TCHAR* const ReadyLabel = TEXT("Ready");
	const TCHAR* const FindMatchLabel = TEXT("Find Match");
	const TCHAR* const CancelQueueLabel = TEXT("Cancel");
	const TCHAR* const AcceptLabel = TEXT("Accept");
	const TCHAR* const DeclineLabel = TEXT("Decline");
	const TCHAR* const LockInLabel = TEXT("Lock In");
	const TCHAR* const ContinueLabel = TEXT("Continue");
	const TCHAR* const EndCustomMatchLabel = TEXT("End Custom Match");
	const TCHAR* const DeveloperEndLabel = TEXT("End Match (Developer)");
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
	FParse::Value(FCommandLine::Get(), ScreenshotSwitch, ScreenshotFolder);
	bEndsMatch = FParse::Param(FCommandLine::Get(), EndsMatchSwitch);
	StartRealTime = FPlatformTime::Seconds();
	const TPair<const TCHAR*, EScript> Scripts[] = {
		{ TEXT("join"), EScript::Join },
		{ TEXT("practice"), EScript::Practice },
		{ TEXT("casual"), EScript::Casual },
		{ TEXT("decline"), EScript::Decline },
		{ TEXT("requeue"), EScript::Requeue },
		{ TEXT("opponent"), EScript::Opponent },
	};
	const TPair<const TCHAR*, EScript>* Known = Algo::FindByPredicate(Scripts, [&Mode](const TPair<const TCHAR*, EScript>& Candidate) {
		return Mode.Equals(Candidate.Key, ESearchCase::CaseSensitive);
	});
	if (!Known)
	{
		Finish(false, FString::Printf(TEXT("-VeyraSmokeFlow takes join, practice, casual, decline, requeue or opponent, not \"%s\""), *Mode));
		return;
	}
	Script = Known->Value;
	UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: started the %s flow script%s."), *Mode, bEndsMatch ? TEXT(", which ends the match") : TEXT(""));
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
	UVeyraClientFlowSubsystem* FlowHost = GetGameInstance()->GetSubsystem<UVeyraClientFlowSubsystem>();
	if (!FlowHost)
	{
		Finish(false, TEXT("the game has no client flow; start it with -VeyraLaunchCode=stdin"));
		return false;
	}
	IVeyraClientIntents& Flow = FlowHost->GetClient();
	if (Script == EScript::Join)
	{
		// A script created the match before the game started, so it waits behind Reconnect (UX-17).
		if (Flow.CanIssue(EVeyraClientIntent::Reconnect))
		{
			UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: pressed Reconnect."));
			Flow.Reconnect();
			return false;
		}
		return true;
	}
	if (Script == EScript::Opponent)
	{
		// It spars until it is closed.
		TickOpponent(Flow);
		return true;
	}
	if (FPlatformTime::Seconds() - StartRealTime > FlowTimeoutRealSeconds)
	{
		Finish(false, FString::Printf(TEXT("timed out in %s"), LexToString(Flow.GetSnapshot().State)));
		return false;
	}
	if (FPlatformTime::Seconds() >= HoldUntil)
	{
		TickScript(Flow);
	}
	return !bFinished;
}

void UVeyraSmokeFlowSubsystem::TickScript(IVeyraClientIntents& Flow)
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
		if (Flow.CanIssue(EVeyraClientIntent::ChooseStarter) && !Capture(TEXT("StarterChoice")))
		{
			const FString Starter = ChooseFrom(Snapshot.Starters);
			if (Starter.IsEmpty())
			{
				Finish(false, TEXT("no starter is on offer"));
				break;
			}
			UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: choosing %s as the starter."), *Starter);
			Click(VanguardLabel(Starter));
		}
		break;

	case EVeyraClientState::Shell:
		if (IsMatchmade())
		{
			TickMatchmadeShell(Flow);
		}
		else if (bSawResults)
		{
			Finish(true, FString::Printf(TEXT("clicked through the starter choice, Play and Practice, locked %s, ended the match from its menu as its host, ")
										 TEXT("saw its verified result and returned to the shell"),
				*LockedVanguard));
		}
		else if (bStartedPractice && !Snapshot.Notice.IsEmpty())
		{
			Finish(false, FString::Printf(TEXT("the practice select was cancelled (%s)"), *Snapshot.Notice));
		}
		else if (!bStartedPractice && Flow.CanIssue(EVeyraClientIntent::StartPractice))
		{
			if (!bOpenedPlay)
			{
				if (!Capture(TEXT("Home")) && Click(PlayLabel))
				{
					bOpenedPlay = true;
				}
			}
			else if (!Capture(TEXT("Play")) && Click(PracticeLabel))
			{
				bStartedPractice = true;
				UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: Play, Custom, Practice."));
			}
		}
		break;

	case EVeyraClientState::MatchFound:
		TickMatchFound(Flow);
		break;

	case EVeyraClientState::Selecting:
		if (Script == EScript::Decline || Script == EScript::Requeue)
		{
			Finish(false, TEXT("the match found went ahead, but this script expected a player to decline it"));
			break;
		}
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
				Click(VanguardLabel(Pick));
			}
			else if (!Capture(TEXT("ChampionSelect")))
			{
				LockedVanguard = Pick;
				UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: locking in %s."), *Pick);
				Click(LockInLabel);
			}
		}
		break;

	case EVeyraClientState::InMatch:
		TickInMatch();
		break;

	case EVeyraClientState::Results:
		CheckResults(Snapshot);
		break;

	default:
		break;
	}
}

void UVeyraSmokeFlowSubsystem::TickMatchmadeShell(IVeyraClientIntents& Flow)
{
	const FVeyraClientSnapshot& Snapshot = Flow.GetSnapshot();
	const TOptional<VeyraBackendProtocol::FParty>& Party = Snapshot.Party;
	if (bSawResults)
	{
		Finish(true, FString::Printf(TEXT("queued, accepted the match found, locked %s, %s, saw its verified result and returned to the shell"), *LockedVanguard,
			bEndsMatch ? TEXT("ended the match from its menu as a developer") : TEXT("waited for the match to end")));
		return;
	}

	// A match found that did not go ahead. Its notice is final once the shell has read the party
	// afresh, which says whether the party is queued again.
	const bool bPartyRead = !Party.IsSet() || Party->Status == EPartyStatus::Idle || Party->Status == EPartyStatus::Queued;
	if (bAnswered && !bPartyRead)
	{
		return;
	}
	if (Script == EScript::Decline && bAnswered)
	{
		if (Snapshot.Notice != DeclinedNotice)
		{
			Finish(false, FString::Printf(TEXT("after declining, the shell's notice is \"%s\""), *Snapshot.Notice));
		}
		else if (Party.IsSet() && Party->Status == EPartyStatus::Idle && !Capture(TEXT("Declined")))
		{
			Finish(true, TEXT("declined the match found once the other player accepted, and is back in the shell, out of the queue"));
		}
		return;
	}
	if (Script == EScript::Requeue && Snapshot.Notice == RequeuedNotice)
	{
		if (!bCancelledQueue)
		{
			if (Flow.CanIssue(EVeyraClientIntent::CancelQueue) && !Capture(TEXT("Requeued")) && Click(CancelQueueLabel))
			{
				bCancelledQueue = true;
				UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: back in the queue after the other player declined; leaving it."));
			}
		}
		else if (Party.IsSet() && Party->Status == EPartyStatus::Idle)
		{
			Finish(true, TEXT("accepted the match found, was queued again in its place when the other player declined, and left the queue"));
		}
		return;
	}
	if (bAnswered && !Snapshot.Notice.IsEmpty())
	{
		Finish(false, FString::Printf(TEXT("the match did not go ahead (%s)"), *Snapshot.Notice));
		return;
	}

	// Into the queue: the first matchmade mode, Ready, then Find Match (UX-6).
	if (bFoundMatch)
	{
		if (Party.IsSet() && Party->Status == EPartyStatus::Queued)
		{
			Capture(TEXT("Queue"));
		}
		return;
	}
	if (Snapshot.Modes.IsEmpty())
	{
		return;
	}
	const VeyraBackendProtocol::FModeInfo* Mode =
		Snapshot.Modes.FindByPredicate([](const VeyraBackendProtocol::FModeInfo& Candidate) { return Candidate.bEnabled && Candidate.bMatchmade; });
	if (!Mode)
	{
		Finish(false, TEXT("the backend offers no matchmade mode"));
		return;
	}
	if (ChosenMode.IsEmpty())
	{
		if (!bOpenedPlay)
		{
			if (!Capture(TEXT("Home")) && Click(PlayLabel))
			{
				bOpenedPlay = true;
			}
		}
		else if (Capture(TEXT("Play")))
		{
			// The Play page's screenshot comes first.
		}
		else if (Party.IsSet() && Party->Mode == Mode->Id)
		{
			// A party of an earlier run still has the mode.
			ChosenMode = Mode->Id;
		}
		else if (Flow.CanIssue(EVeyraClientIntent::SelectMode) && Click(VanguardLabel(Mode->Id)))
		{
			ChosenMode = Mode->Id;
			UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: chose %s."), *Mode->Id);
		}
		return;
	}
	if (!Party.IsSet() || Party->Mode != ChosenMode)
	{
		return;
	}
	if (Party->Status != EPartyStatus::Idle)
	{
		// Queued by an earlier run, or found already.
		bFoundMatch = true;
		return;
	}
	const VeyraBackendProtocol::FPartyMember* You = Party->Find(Snapshot.AccountId);
	if (!You)
	{
		return;
	}
	if (!You->bReady)
	{
		if (!bReadied && Flow.CanIssue(EVeyraClientIntent::SetReady) && Click(ReadyLabel))
		{
			bReadied = true;
		}
		return;
	}
	if (Flow.CanIssue(EVeyraClientIntent::FindMatch) && !Capture(TEXT("Party")) && Click(FindMatchLabel))
	{
		bFoundMatch = true;
		UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: Ready; finding a match."));
	}
}

void UVeyraSmokeFlowSubsystem::TickMatchFound(IVeyraClientIntents& Flow)
{
	const FVeyraClientSnapshot& Snapshot = Flow.GetSnapshot();
	if (Script == EScript::Practice)
	{
		Finish(false, TEXT("a match was found for a player who only practises"));
		return;
	}
	if (bAnswered || !Flow.CanIssue(EVeyraClientIntent::AcceptMatch))
	{
		return;
	}
	const bool bDecline = Script == EScript::Decline;
	// The decline waits for everyone else to accept, so the others are queued again after accepting.
	if (bDecline && Snapshot.MatchFound.Accepted < Snapshot.MatchFound.Total - 1)
	{
		return;
	}
	if (Capture(TEXT("MatchFound")))
	{
		return;
	}
	if (Click(bDecline ? DeclineLabel : AcceptLabel))
	{
		bAnswered = true;
		UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: %s the match found with %.0f s left."), bDecline ? TEXT("declined") : TEXT("accepted"),
			Flow.GetRemainingAcceptSeconds());
	}
}

void UVeyraSmokeFlowSubsystem::TickOpponent(IVeyraClientIntents& Flow)
{
	const FVeyraClientSnapshot& Snapshot = Flow.GetSnapshot();
	// A sparring partner carries on: it retries what can be retried, and otherwise goes on playing, as
	// after a pick another player took first.
	if (Snapshot.Problem.IsSet() && Flow.CanIssue(EVeyraClientIntent::Retry))
	{
		UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: the opponent retries after %s."), *Snapshot.Problem->Code);
		Flow.Retry();
		return;
	}
	switch (Snapshot.State)
	{
	case EVeyraClientState::StarterChoice:
		if (Flow.CanIssue(EVeyraClientIntent::ChooseStarter) && !Snapshot.Starters.IsEmpty())
		{
			Flow.ChooseStarter(Snapshot.Starters.Contains(WantedVanguard) ? WantedVanguard : Snapshot.Starters[0]);
		}
		break;

	case EVeyraClientState::Shell:
	{
		const TOptional<VeyraBackendProtocol::FParty>& Party = Snapshot.Party;
		const VeyraBackendProtocol::FModeInfo* Mode =
			Snapshot.Modes.FindByPredicate([](const VeyraBackendProtocol::FModeInfo& Candidate) { return Candidate.bEnabled && Candidate.bMatchmade; });
		if (!Mode)
		{
			break;
		}
		if (!Party.IsSet() || Party->Mode != Mode->Id)
		{
			if (Flow.CanIssue(EVeyraClientIntent::SelectMode))
			{
				Flow.SelectMode(Mode->Id);
			}
			break;
		}
		const VeyraBackendProtocol::FPartyMember* You = Party->Find(Snapshot.AccountId);
		if (Party->Status != EPartyStatus::Idle || !You)
		{
			break;
		}
		if (!You->bReady)
		{
			if (Flow.CanIssue(EVeyraClientIntent::SetReady))
			{
				Flow.SetReady(true);
			}
		}
		else if (Flow.CanIssue(EVeyraClientIntent::FindMatch))
		{
			UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: the opponent queues for %s."), *Mode->Id);
			Flow.FindMatch();
		}
		break;
	}

	case EVeyraClientState::MatchFound:
		if (Flow.CanIssue(EVeyraClientIntent::AcceptMatch))
		{
			UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: the opponent accepts the match found."));
			Flow.AcceptMatch();
		}
		break;

	case EVeyraClientState::Selecting:
	{
		const VeyraBackendProtocol::FSelectSeat* You = Snapshot.Select.FindYou();
		if (!You || !Flow.CanIssue(EVeyraClientIntent::LockVanguard))
		{
			break;
		}
		const TArray<VeyraBackendProtocol::FSelectSeat>& Seats = Snapshot.Select.Seats;
		const bool bOthersLocked = Seats.ContainsByPredicate([You](const VeyraBackendProtocol::FSelectSeat& Seat) { return Seat.Side != You->Side && !Seat.Locked.IsEmpty(); });
		if (!bOthersLocked && Flow.GetRemainingPickSeconds() > OpponentLockSeconds)
		{
			break;
		}
		const auto IsFree = [&Seats](const FString& Id) {
			return !Seats.ContainsByPredicate([&Id](const VeyraBackendProtocol::FSelectSeat& Seat) { return Seat.Locked == Id; });
		};
		FString Pick = IsFree(WantedVanguard) && Snapshot.AvailableVanguards.Contains(WantedVanguard) ? WantedVanguard : FString();
		for (const FString& Id : Snapshot.AvailableVanguards)
		{
			if (Pick.IsEmpty() && IsFree(Id))
			{
				Pick = Id;
			}
		}
		if (Pick.IsEmpty())
		{
			break;
		}
		if (You->Hover != Pick)
		{
			Flow.HoverVanguard(Pick);
		}
		else
		{
			UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: the opponent locks in %s."), *Pick);
			Flow.LockVanguard(Pick);
		}
		break;
	}

	case EVeyraClientState::Results:
		// It stands still in the match; afterwards it goes again.
		if (Flow.CanIssue(EVeyraClientIntent::ContinueFromResults))
		{
			Flow.ContinueFromResults();
		}
		break;

	default:
		break;
	}
}

void UVeyraSmokeFlowSubsystem::TickInMatch()
{
	// The practice host ends its match; of a standard match's players, the one told to.
	if (Script == EScript::Casual && !bEndsMatch)
	{
		return;
	}
	const UWorld* World = GetGameInstance()->GetWorld();
	AVeyraPlayerController* Controller = Cast<AVeyraPlayerController>(GetGameInstance()->GetFirstLocalPlayerController());
	const AVeyraGameState* GameState = World ? World->GetGameState<AVeyraGameState>() : nullptr;
	const AActor* Vanguard = Controller ? Controller->GetVanguard() : nullptr;
	if (!Controller || !GameState || !Vanguard || GameState->GetPhase() != EVeyraMatchPhase::Live)
	{
		return;
	}
	if (!bOrderedMove)
	{
		// Play a little: walk toward the lane centre.
		bOrderedMove = true;
		MoveStart = Vanguard->GetActorLocation();
		UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: the match is live; walking toward the lane centre."));
		Controller->IssueMoveOrder(FVector(0.0, MoveStart.Y, 0.0));
		return;
	}
	if (Controller->GetOrderRejectionCount() > 0)
	{
		Finish(false, FString::Printf(TEXT("the server refused the move: %s"), LexToString(Controller->GetLastOrderRejection())));
		return;
	}
	if (bAskedToEnd)
	{
		if (Controller->GetEndCustomMatchRefusalCount() > 0)
		{
			Finish(false, FString::Printf(TEXT("the server refused to end the custom match: %s"), LexToString(Controller->GetLastEndCustomMatchRefusal())));
		}
		return;
	}
	if (FVector::Dist2D(Vanguard->GetActorLocation(), MoveStart) < MoveProofDistance)
	{
		return;
	}
#if WITH_VEYRA_UI
	// The match is ended from the in-match menu (ADR-010 §4, §7), behind its confirmation: End Custom
	// Match by a practice host, the developer end in a standard match.
	const TCHAR* const EndLabel = Script == EScript::Practice ? EndCustomMatchLabel : DeveloperEndLabel;
	UVeyraMatchMenuSubsystem* Menus = GetGameInstance()->GetSubsystem<UVeyraMatchMenuSubsystem>();
	if (!Menus)
	{
		Finish(false, TEXT("the game has no in-match menu"));
		return;
	}
	if (!Menus->IsMenuOpen())
	{
		if (bOpenedMenu)
		{
			Finish(false, TEXT("the in-match menu closed before the match ended"));
			return;
		}
		// As its key does; the menu opens once its key is bound to this match's controller.
		Menus->ToggleMenu();
		bOpenedMenu = Menus->IsMenuOpen();
		if (bOpenedMenu)
		{
			UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: the Vanguard walks; opened the in-match menu."));
		}
		return;
	}
	UVeyraShellButton* EndButton = Menus->GetMenu()->FindButton(FText::FromString(EndLabel));
	if (!EndButton || !EndButton->GetIsEnabled())
	{
		Finish(false, FString::Printf(TEXT("the in-match menu offers no %s"), EndLabel));
		return;
	}
	if (!bConfirmingEnd)
	{
		bConfirmingEnd = true;
		EndButton->Press();
		return;
	}
	if (Capture(TEXT("MatchMenu")))
	{
		return;
	}
	UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: confirmed %s."), EndLabel);
	bAskedToEnd = true;
	EndButton->Press();
#else
	Finish(false, TEXT("this build has no in-match menu"));
#endif
}

void UVeyraSmokeFlowSubsystem::CheckResults(const FVeyraClientSnapshot& Snapshot)
{
	if (bSawResults)
	{
		return;
	}
	const TOptional<VeyraBackendProtocol::FMatchOutcome>& Result = Snapshot.Result;
	if (!Result.IsSet() || !Result->bHasResult)
	{
		Finish(false, FString::Printf(TEXT("match %s has no verified result"), *Snapshot.MatchId));
		return;
	}
	UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: the verified result of match %s: %s, %s rules, winner %s, %.1f s, as %s, %s, %s at the end."),
		*Result->MatchId, *Result->EndReason, *Result->Rules, Result->Winner.IsEmpty() ? TEXT("none") : *Result->Winner, Result->DurationSeconds,
		*Result->VanguardId, Result->bJoined ? TEXT("joined") : TEXT("never joined"), Result->bConnectedAtEnd ? TEXT("connected") : TEXT("disconnected"));
	const bool bPractice = Script == EScript::Practice;
	const TCHAR* const ExpectedEndReason = bPractice ? PracticeEndReason : DeveloperEndReason;
	const TCHAR* const ExpectedRules = bPractice ? PracticeRules : StandardRules;
	if (Result->EndReason != ExpectedEndReason || Result->Rules != ExpectedRules || !Result->Winner.IsEmpty() || Result->VanguardId != LockedVanguard
		|| !Result->bJoined || !Result->bConnectedAtEnd || Result->MatchId != Snapshot.MatchId)
	{
		Finish(false, FString::Printf(TEXT("the verified result is not a %s match ended by %s, with no winner, of the locked Vanguard, joined and connected at the end"),
			ExpectedRules, ExpectedEndReason));
		return;
	}
	if (!Capture(TEXT("Results")) && Click(ContinueLabel))
	{
		bSawResults = true;
	}
}

FString UVeyraSmokeFlowSubsystem::VanguardLabel(const FString& VanguardId)
{
#if WITH_VEYRA_UI
	// A Vanguard's or a mode's button shows its name, not its content ID.
	return VeyraShellModels::NameOf(VanguardId).ToString();
#else
	return VanguardId;
#endif
}

bool UVeyraSmokeFlowSubsystem::Click(const FString& Label)
{
#if WITH_VEYRA_UI
	const UVeyraShellUISubsystem* Shell = GetGameInstance()->GetSubsystem<UVeyraShellUISubsystem>();
	UVeyraShellScreen* Screen = Shell ? Shell->GetScreen() : nullptr;
	UVeyraShellButton* Button = Screen ? Screen->FindButton(FText::FromString(Label)) : nullptr;
	if (!Button || !Button->GetIsEnabled())
	{
		Finish(false, FString::Printf(TEXT("the shell shows no enabled \"%s\" button%s"), *Label, Screen ? TEXT("") : TEXT(": it shows no screen at all")));
		return false;
	}
	UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: clicked \"%s\"."), *Label);
	Button->Press();
	return true;
#else
	Finish(false, TEXT("this build has no shell to click"));
	return false;
#endif
}

bool UVeyraSmokeFlowSubsystem::Capture(const TCHAR* Name)
{
	if (ScreenshotFolder.IsEmpty() || Captured.Contains(Name))
	{
		return false;
	}
	Captured.Add(Name);
	const FString Path = FPaths::Combine(ScreenshotFolder, FString::Printf(TEXT("Flow-%s.png"), Name));
	FScreenshotRequest::RequestScreenshot(Path, /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
	HoldUntil = FPlatformTime::Seconds() + ScreenshotHoldRealSeconds;
	UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: asked for a screenshot at %s."), *Path);
	return true;
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
	if (UVeyraClientFlowSubsystem* FlowHost = GetGameInstance()->GetSubsystem<UVeyraClientFlowSubsystem>())
	{
		FlowHost->GetClient().Quit();
	}
	else
	{
		FPlatformMisc::RequestExit(/*bForce*/ false, TEXT("VeyraSmokeFlow"));
	}
}
