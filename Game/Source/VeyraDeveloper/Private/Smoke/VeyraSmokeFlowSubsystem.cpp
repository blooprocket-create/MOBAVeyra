// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Smoke/VeyraSmokeFlowSubsystem.h"

#include "Algo/Count.h"
#include "Algo/Find.h"
#include "Client/VeyraClientFlowSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Inventory/VeyraInventoryComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Recall/VeyraRecallComponent.h"
#include "Structures/VeyraStructure.h"
#include "Slots/VeyraAbilitySlot.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "UnrealClient.h"
#include "VeyraGameState.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardCharacter.h"

#if WITH_VEYRA_UI
#include "Match/VeyraMatchMenu.h"
#include "Match/VeyraMatchMenuSubsystem.h"
#include "Scoreboard/VeyraScoreboard.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellModels.h"
#include "Shell/VeyraShellScreen.h"
#include "Shell/VeyraShellUISubsystem.h"
#include "Shop/VeyraShopScreen.h"
#include "Text/VeyraContentText.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogVeyraSmokeFlow, Log, All);

namespace
{
	using VeyraBackendProtocol::EPartyStatus;

	const TCHAR* const FlowSwitch = TEXT("VeyraSmokeFlow=");
	const TCHAR* const VanguardSwitch = TEXT("VeyraSmokeFlowVanguard=");
	const TCHAR* const ScreenshotSwitch = TEXT("VeyraSmokeFlowScreenshots=");
	const TCHAR* const EndsMatchSwitch = TEXT("VeyraSmokeFlowEndsMatch");
	const TCHAR* const SiegesSwitch = TEXT("VeyraSmokeFlowSieges");
	const TCHAR* const VictorySwitch = TEXT("VeyraSmokeFlowVictory");
	const TCHAR* const ReconnectsSwitch = TEXT("VeyraSmokeFlowReconnects");
	const TCHAR* const AwaitsReturnSwitch = TEXT("VeyraSmokeFlowAwaitsReturn");
	// Harness settings for the siege: how often it asks, leaving time for each fall to replicate, and
	// how many asks mean something is wrong, well above the structures on the way to a Prime Well.
	constexpr double SiegeIntervalRealSeconds = 1.0;
	constexpr int32 MaxSiegeRequests = 40;
	// Harness settings, not gameplay: how long the whole script may take, how far the Vanguard must
	// walk toward the lane centre before the match is ended, and how long to wait for a screenshot to
	// be saved.
	constexpr double FlowTimeoutRealSeconds = 300.0;
	constexpr double MoveProofDistance = 100.0;
	// -VeyraSmokeFlowReconnects: how long, in real seconds, it stays away before pressing Reconnect, so
	// the other player sees its PlayerState go inactive, as it would after a real drop.
	constexpr double AwayRealSeconds = 5.0;
	// -VeyraSmokeFlowReconnects: how long, in match seconds, it plays before leaving, so the other
	// player has seen it in the match first.
	constexpr double LeaveAfterMatchSeconds = 10.0;
	constexpr double ScreenshotHoldRealSeconds = 1.0;
	// Practice: how far the Vanguard walks from where it shopped before it recalls: beyond the
	// fountain, so arriving home is told apart from staying.
	constexpr double RecallWalkDistance = 800.0;
	// The sparring partner locks its pick once the other team has locked, or with this much of the
	// pick timer left, so it never takes the Vanguard the person was about to lock.
	constexpr double OpponentLockSeconds = 15.0;
	// What the verified result must say: a practice its host ended (ADR-010 §7), or a standard match
	// a developer ended.
	const TCHAR* const PracticeEndReason = TEXT("host_ended");
	const TCHAR* const PracticeRules = TEXT("practice");
	const TCHAR* const DeveloperEndReason = TEXT("developer_request");
	const TCHAR* const VictoryEndReason = TEXT("prime_well_destroyed");
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
	bSieges = FParse::Param(FCommandLine::Get(), SiegesSwitch);
	bVictory = FParse::Param(FCommandLine::Get(), VictorySwitch);
	bReconnects = FParse::Param(FCommandLine::Get(), ReconnectsSwitch);
	bAwaitsReturn = FParse::Param(FCommandLine::Get(), AwaitsReturnSwitch);
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
	UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: started the %s flow script%s%s."), *Mode, bEndsMatch ? TEXT(", which ends the match") : TEXT(""),
		bSieges ? TEXT(", which sieges") : TEXT(""));
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
		// It left on purpose, and goes back as a player would (UX-17).
		if (bReconnects && bLeft && !bCameBack)
		{
			if (Flow.CanIssue(EVeyraClientIntent::Reconnect) && FPlatformTime::Seconds() - LeftAtRealSeconds >= AwayRealSeconds)
			{
				UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: pressed Reconnect."));
				Flow.Reconnect();
			}
			break;
		}
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
		else if (bSawResults && !TickHistory(Flow))
		{
			Finish(true, FString::Printf(TEXT("clicked through the starter choice, Play and Practice, locked %s, %sbought %s in the shop, recalled home, ")
										 TEXT("ended the match from its menu as its host, saw its verified result, returned to the shell and found the match in Match History"),
				*LockedVanguard, bSieges ? TEXT("sieged the enemy Prime Well down, played on, ") : TEXT(""), *BoughtItem));
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
			else if (!ChooseFluxSpells(Snapshot, Flow))
			{
				// One choice a tick: each waits for the backend's answer.
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
			bReconnects && bEndsMatch ? TEXT("left the match, came back to its Vanguard and ended the match from its menu as a developer")
			: bReconnects ? TEXT("left the match, came back to its Vanguard and waited for the match to end")
			: bAwaitsReturn ? TEXT("saw the other player leave and come back, and ended the match from its menu as a developer")
			: bEndsMatch ? TEXT("ended the match from its menu as a developer")
			: bSieges  ? TEXT("won the match by siege")
					   : TEXT("waited for the match to end")));
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
	UWorld* World = GetGameInstance()->GetWorld();
	AVeyraPlayerController* Controller = Cast<AVeyraPlayerController>(GetGameInstance()->GetFirstLocalPlayerController());
	const AVeyraGameState* GameState = World ? World->GetGameState<AVeyraGameState>() : nullptr;
	const AActor* Vanguard = Controller ? Controller->GetVanguard() : nullptr;
	if (!Controller || !GameState || !Vanguard || GameState->GetPhase() != EVeyraMatchPhase::Live)
	{
		return;
	}
	if (bReconnects && TickReconnect(*Controller, *World))
	{
		return;
	}
	// The practice host ends its match; of a standard match's players, the one told to, or the one
	// that sieges.
	if (Script == EScript::Casual && !bEndsMatch && !bSieges)
	{
		return;
	}
	if (bAwaitsReturn && TickAwaitReturn(*Controller, *GameState))
	{
		return;
	}
	if (bSieges && TickSiege(*Controller, *World))
	{
		return;
	}
	if (Script == EScript::Practice && TickShop(*Controller))
	{
		return;
	}
	if (Script == EScript::Practice && TickScoreboard(*Controller))
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
	// Once a Recall is asked for, the walk has been proved, and the Vanguard is meant to come home.
	if (!bAskedToRecall && FVector::Dist2D(Vanguard->GetActorLocation(), MoveStart) < MoveProofDistance)
	{
		return;
	}
	if (Script == EScript::Practice && TickRecall(*Controller, *Vanguard))
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

bool UVeyraSmokeFlowSubsystem::TickShop(AVeyraPlayerController& Controller)
{
	if (bShopped)
	{
		return false;
	}
#if WITH_VEYRA_UI
	UVeyraMatchMenuSubsystem* Screens = GetGameInstance()->GetSubsystem<UVeyraMatchMenuSubsystem>();
	const UVeyraInventoryComponent* Inventory = Controller.PlayerState ? Controller.PlayerState->FindComponentByClass<UVeyraInventoryComponent>() : nullptr;
	if (!Screens || !Inventory)
	{
		Finish(false, TEXT("the game has no shop"));
		return true;
	}
	if (!Screens->IsShopOpen())
	{
		if (!BoughtItem.IsEmpty())
		{
			Finish(false, TEXT("the shop closed before the purchase arrived"));
			return true;
		}
		// As its key does; the shop opens once its key is bound to this match's controller.
		Screens->ToggleShop();
		if (Screens->IsShopOpen())
		{
			UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: the match is live; opened the shop with %.0f Gold."), Screens->GetShop()->GetView().Gold);
		}
		return true;
	}
	UVeyraShopScreen& Shop = *Screens->GetShop();
	const FVeyraShopView& View = Shop.GetView();
	if (BoughtItem.IsEmpty())
	{
		// The shop reads the owner's inventory: it arrives, and the shop learns it is at the fountain, a moment after the match goes live.
		if (!View.bAtShop)
		{
			return true;
		}
		const FVeyraShopOffer* Offer = Algo::FindByPredicate(View.Offers, [](const FVeyraShopOffer& Candidate) {
			const FVeyraItemDefinition* Definition = UVeyraItemsTuningSubsystem::FindItem(Candidate.Item);
			return Candidate.Refusal == EVeyraShopRefusal::None && Definition && Definition->Category == EVeyraItemCategory::Equipment;
		});
		// As a player does: choose the item's tile, then buy it with the one purchase button.
		UVeyraShellButton* Tile = Offer ? Shop.FindButton(UVeyraShopScreen::TileLabel(Offer->Item)) : nullptr;
		if (Tile && Shop.GetSelectedItem() != Offer->Item)
		{
			Tile->Press();
			return true;
		}
		if (Capture(TEXT("Shop")))
		{
			return true;
		}
		UVeyraShellButton* Buy = Offer ? Shop.FindButton(UVeyraShopScreen::BuyLabel(Offer->Item, Offer->Price)) : nullptr;
		if (!Buy || !Buy->GetIsEnabled())
		{
			Finish(false, TEXT("the shop offers no equipment the starting Gold affords"));
			return true;
		}
		BoughtItem = Offer->Item.ToString();
		UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: buying %s for %.0f Gold."), *BoughtItem, Offer->Price);
		Buy->Press();
		return true;
	}
	if (Controller.GetShopRefusalCount() > 0)
	{
		Finish(false, FString::Printf(TEXT("the server refused the purchase or swap: %s"), LexToString(Controller.GetLastShopRefusal())));
		return true;
	}
	const bool bArrived = Algo::FindByPredicate(Inventory->GetSlots(), [this](const FVeyraInventorySlot& Slot) {
		return !Slot.IsEmpty() && Slot.Item.ToString() == BoughtItem;
	}) != nullptr;
	if (!bArrived || Capture(TEXT("ShopBought")))
	{
		return true;
	}
	// Then Flux Spell slot 1 swaps, for Gold, to the first roster spell neither slot holds (ADR-015 §7).
	if (SwappedSpell.IsEmpty())
	{
		const FVeyraShopSpellOffer* Offer = View.SpellSlots.IsEmpty() ? nullptr : Algo::FindByPredicate(View.SpellSlots[0].Offers, [](const FVeyraShopSpellOffer& Candidate) {
			return Candidate.Refusal == EVeyraShopRefusal::None;
		});
		// The swaps have their own tab.
		if (UVeyraShellButton* SpellsTab = Shop.FindButton(UVeyraShopScreen::SpellsTabLabel()); SpellsTab && Offer && !Shop.FindButton(UVeyraShopScreen::SwapLabel(0, Offer->Spell)))
		{
			SpellsTab->Press();
			return true;
		}
		UVeyraShellButton* Swap = Offer ? Shop.FindButton(UVeyraShopScreen::SwapLabel(0, Offer->Spell)) : nullptr;
		if (!Swap || !Swap->GetIsEnabled())
		{
			Finish(false, TEXT("the shop offers no Flux Spell swap the Gold left affords"));
			return true;
		}
		SwappedSpell = Offer->Spell.ToString();
		UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: swapping Flux Spell slot 1 from %s to %s for %.0f Gold."), *View.SpellSlots[0].Spell.ToString(), *SwappedSpell,
			View.SpellSwapCost);
		Swap->Press();
		return true;
	}
	if (View.SpellSlots.IsEmpty() || View.SpellSlots[0].Spell.ToString() != SwappedSpell)
	{
		return true;
	}
	const double GoldLeft = View.Gold;
	Screens->ToggleShop();
	bShopped = true;
	UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: %s arrived in the inventory and slot 1 holds %s, leaving %.0f Gold; closed the shop."), *BoughtItem,
		*SwappedSpell, GoldLeft);
	return false;
#else
	Finish(false, TEXT("this build has no shop"));
	return true;
#endif
}

bool UVeyraSmokeFlowSubsystem::TickScoreboard(AVeyraPlayerController& /*Controller*/)
{
	if (bScoreboardChecked)
	{
		return false;
	}
#if WITH_VEYRA_UI
	UVeyraMatchMenuSubsystem* Screens = GetGameInstance()->GetSubsystem<UVeyraMatchMenuSubsystem>();
	if (!Screens)
	{
		Finish(false, TEXT("the game has no scoreboard"));
		return true;
	}
	if (!Screens->GetScoreboard())
	{
		// As holding its key does; it shows once its key is bound to this match's controller.
		Screens->ShowScoreboard();
		return true;
	}
	// The players' scores, levels and Vanguards replicate a moment after the match goes live.
	const FVeyraScoreboardView& View = Screens->GetScoreboard()->GetView();
	const bool bShowsThePlayer = View.Sides.Num() == 2 && View.Sides[0].bAllies
		&& View.Sides[0].Rows.ContainsByPredicate([](const FVeyraScoreboardRow& Row) { return Row.bLocal && Row.Vanguard.IsValid() && Row.Level >= 1; });
	if (!bShowsThePlayer || Capture(TEXT("Scoreboard")))
	{
		return true;
	}
	UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: the scoreboard shows %d ally and %d enemy player(s); let go of it."), View.Sides[0].Rows.Num(),
		View.Sides[1].Rows.Num());
	Screens->HideScoreboard();
	bScoreboardChecked = true;
	return false;
#else
	Finish(false, TEXT("this build has no scoreboard"));
	return true;
#endif
}

bool UVeyraSmokeFlowSubsystem::TickRecall(AVeyraPlayerController& Controller, const AActor& Vanguard)
{
	if (bRecalled)
	{
		return false;
	}
	const UVeyraRecallComponent* Recall = Controller.PlayerState ? Controller.PlayerState->FindComponentByClass<UVeyraRecallComponent>() : nullptr;
	if (!Recall)
	{
		Finish(false, TEXT("the participant cannot recall"));
		return true;
	}
	const double FromHome = FVector::Dist2D(Vanguard.GetActorLocation(), MoveStart);
	if (!bAskedToRecall)
	{
		if (FromHome < RecallWalkDistance)
		{
			return true;
		}
		bAskedToRecall = true;
		UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: %.0f units from the fountain; recalling."), FromHome);
		Controller.RequestRecall();
		return true;
	}
	if (Controller.GetOrderRejectionCount() > 0)
	{
		Finish(false, FString::Printf(TEXT("the server refused the Recall: %s"), LexToString(Controller.GetLastOrderRejection())));
		return true;
	}
	if (Recall->IsRecalling())
	{
		bSawRecall = true;
		Capture(TEXT("Recall"));
		return true;
	}
	// Home is the game's own fountain, where its shop opens: the arrival may land beside the start when
	// someone stands on it, so a distance from where the walk began says too little.
	const UVeyraInventoryComponent* Inventory = Controller.PlayerState ? Controller.PlayerState->FindComponentByClass<UVeyraInventoryComponent>() : nullptr;
	if (!bSawRecall || !Inventory || !Inventory->IsAtFountain())
	{
		return true;
	}
	bRecalled = true;
	UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: the Recall brought the Vanguard home."));
	return false;
}

bool UVeyraSmokeFlowSubsystem::TickSiege(AVeyraPlayerController& Controller, const UWorld& World)
{
	if (bSiegeDone)
	{
		return false;
	}
	// Practice goes on after the enemy Prime Well falls (ADR-011 §14): the siege stops there, and the
	// script ends the match as its host. A standard match ends at that fall, and the flow leaves it.
	if (Script == EScript::Practice)
	{
		const EVeyraTeam Enemies = VeyraTeams::Opposing(VeyraTeams::TeamOf(Controller.PlayerState));
		for (TActorIterator<AVeyraStructure> It(&World); It; ++It)
		{
			if (It->GetVeyraTeam() == Enemies && It->GetStructureKind() == EVeyraStructureKind::PrimeWell && It->IsDestroyed())
			{
				bSiegeDone = true;
				UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: the enemy Prime Well fell after %d siege(s), and the practice match goes on."), SiegeRequests);
				return false;
			}
		}
	}
	if (SiegeRequests >= MaxSiegeRequests)
	{
		Finish(false, FString::Printf(TEXT("sieged %d times and the %s"), SiegeRequests,
			Script == EScript::Practice ? TEXT("enemy Prime Well still stands") : TEXT("match has not ended")));
		return true;
	}
	const double Now = FPlatformTime::Seconds();
	if (Now >= NextSiegeAt)
	{
		NextSiegeAt = Now + SiegeIntervalRealSeconds;
		++SiegeRequests;
		UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: asked for developer siege %d."), SiegeRequests);
		Controller.RequestDeveloperSiege();
	}
	return true;
}

bool UVeyraSmokeFlowSubsystem::TickReconnect(AVeyraPlayerController& Controller, UWorld& World)
{
	if (!bLeft)
	{
		const AVeyraGameState* GameState = World.GetGameState<AVeyraGameState>();
		if (!GameState || GameState->GetMatchClockSeconds() < LeaveAfterMatchSeconds)
		{
			return true;
		}
		bLeft = true;
		LeftAtRealSeconds = FPlatformTime::Seconds();
		UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: the match is live; leaving it."));
		GEngine->Exec(&World, TEXT("disconnect"));
		return true;
	}
	if (!bCameBack)
	{
		// Back in the match: the same Vanguard it locked, not a new one (Match Flow Bible §3).
		const AVeyraPlayerState* Own = Controller.GetPlayerState<AVeyraPlayerState>();
		if (!Own || Own->GetVanguardId().ToString() != LockedVanguard)
		{
			Finish(false, FString::Printf(TEXT("came back to %s, not the %s it locked"), Own ? *Own->GetVanguardId().ToString() : TEXT("nothing"), *LockedVanguard));
			return true;
		}
		bCameBack = true;
		UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: back in the match, as %s."), *LockedVanguard);
	}
	return false;
}

bool UVeyraSmokeFlowSubsystem::TickAwaitReturn(const AVeyraPlayerController& Controller, const AVeyraGameState& GameState)
{
	// A client's GameState lists only active players: one who left drops out of it, and returns to it
	// when it comes back (APlayerState::OnRep_bIsInactive).
	const APlayerState* Own = Controller.PlayerState;
	const int32 Others = Algo::CountIf(GameState.PlayerArray, [Own](const APlayerState* Member) { return Member && Member != Own; });
	MostOthers = FMath::Max(MostOthers, Others);
	const bool bAnyAway = Others < MostOthers;
	if (bAnyAway && !bSawAway)
	{
		bSawAway = true;
		UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: the other player left the match."));
	}
	if (bSawAway && !bAnyAway && !bSawReturn)
	{
		bSawReturn = true;
		UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: the other player came back."));
	}
	return !bSawReturn;
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
	PlayedMatchId = Result->MatchId;
	UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: the verified result of match %s: %s, %s rules, winner %s, %.1f s, as %s, %s, %s at the end."),
		*Result->MatchId, *Result->EndReason, *Result->Rules, Result->Winner.IsEmpty() ? TEXT("none") : *Result->Winner, Result->DurationSeconds,
		*Result->VanguardId, Result->bJoined ? TEXT("joined") : TEXT("never joined"), Result->bConnectedAtEnd ? TEXT("connected") : TEXT("disconnected"));
	const bool bPractice = Script == EScript::Practice;
	const TCHAR* const ExpectedEndReason = bPractice ? PracticeEndReason : bVictory ? VictoryEndReason : DeveloperEndReason;
	const TCHAR* const ExpectedRules = bPractice ? PracticeRules : StandardRules;
	// Only a won match has a winner: the sieging player's side, which the other lost to.
	const bool bWinnerRight = bVictory ? !Result->Winner.IsEmpty() && (Result->Winner == Result->Side) == bSieges : Result->Winner.IsEmpty();
	// A player back within the grace has no personal loss (Match Flow Bible §5.2).
	if (Result->EndReason != ExpectedEndReason || Result->Rules != ExpectedRules || !bWinnerRight || Result->VanguardId != LockedVanguard
		|| !Result->bJoined || !Result->bConnectedAtEnd || Result->bPersonalLoss || Result->MatchId != Snapshot.MatchId)
	{
		Finish(false, FString::Printf(TEXT("the verified result is not a %s match ended by %s, %s, of the locked Vanguard, joined and connected at the end"),
			ExpectedRules, ExpectedEndReason, !bVictory ? TEXT("with no winner") : bSieges ? TEXT("won by this side") : TEXT("lost by this side")));
		return;
	}
	// The verified scoreboard (ADR-017 §5): the player's own line, with its Vanguard and its starting Gold
	// at least, and in practice each bot beside it.
	const VeyraBackendProtocol::FPlayerOutcome* You = Result->Players.FindByPredicate([](const VeyraBackendProtocol::FPlayerOutcome& Line) { return Line.bYou; });
	const int32 Bots = Algo::CountIf(Result->Players, [](const VeyraBackendProtocol::FPlayerOutcome& Line) { return Line.Name.StartsWith(TEXT("Bot ")); });
	if (!Result->bHasScoreboard || !You || You->VanguardId != LockedVanguard || !(You->Statistics.GoldBySource.Starting > 0.0) || You->Statistics.Level < 1
		|| (bPractice && Bots == 0))
	{
		Finish(false, TEXT("the verified result has no scoreboard with the player's own line, and in practice its bots"));
		return;
	}
	UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: the verified scoreboard lists %d player(s) (%d bot(s)) and %d Flux Well capture(s); this player went %d/%d/%d, earned %.0f Gold, dealt %.0f to towers."),
		Result->Players.Num(), Bots, Result->Wells.Num(), You->Statistics.Kills, You->Statistics.Deaths, You->Statistics.Assists, You->Statistics.GoldEarned,
		You->Statistics.TowerDamage);
#if WITH_VEYRA_UI
	// The results screen says so (ADR-011 §13).
	if (bVictory)
	{
		const FString Headline = VeyraShellModels::DescribeResults(Snapshot).Headline.ToString();
		const TCHAR* const ExpectedHeadline = bSieges ? TEXT("Victory") : TEXT("Defeat");
		if (Headline != ExpectedHeadline)
		{
			Finish(false, FString::Printf(TEXT("the results screen says \"%s\", not \"%s\""), *Headline, ExpectedHeadline));
			return;
		}
	}
#endif
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

bool UVeyraSmokeFlowSubsystem::ChooseFluxSpells(const FVeyraClientSnapshot& Snapshot, IVeyraClientIntents& Flow)
{
	const VeyraBackendProtocol::FSelectSeat* You = Snapshot.Select.FindYou();
	const TArray<FVeyraContentId>& Roster = UVeyraAbilitiesTuningSubsystem::Get().FluxSpells.Roster;
	for (int32 Slot = 0; You && Slot < static_cast<int32>(UE_ARRAY_COUNT(VeyraAbilitySlots::Spells)) && Roster.IsValidIndex(Slot); ++Slot)
	{
		const FString Wanted = Roster[Slot].ToString();
		if (!You->FluxSpells.IsValidIndex(Slot) || You->FluxSpells[Slot] != Wanted)
		{
			if (Flow.CanIssue(EVeyraClientIntent::ChooseFluxSpell))
			{
				// As League's summoner spells: the slot's tile opens its picker, whose button of that name chooses it.
				if (OpenSpellSlot() != Slot)
				{
					Click(SpellSlotLabel(Slot));
				}
				else
				{
					UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: choosing Flux Spell %s for slot %d."), *Wanted, Slot + 1);
					Click(SpellLabel(Roster[Slot]));
				}
			}
			return false;
		}
	}
	return true;
}

FString UVeyraSmokeFlowSubsystem::SpellSlotLabel(int32 Slot)
{
#if WITH_VEYRA_UI
	return VeyraShellModels::SpellSlotTitle(Slot).ToString();
#else
	return FString::FromInt(Slot + 1);
#endif
}

int32 UVeyraSmokeFlowSubsystem::OpenSpellSlot() const
{
#if WITH_VEYRA_UI
	const UVeyraShellUISubsystem* Shell = GetGameInstance()->GetSubsystem<UVeyraShellUISubsystem>();
	const UVeyraShellScreen* Screen = Shell ? Shell->GetScreen() : nullptr;
	return Screen ? Screen->GetOpenSpellSlot() : INDEX_NONE;
#else
	return INDEX_NONE;
#endif
}

FString UVeyraSmokeFlowSubsystem::SpellLabel(const FVeyraContentId& SpellId)
{
#if WITH_VEYRA_UI
	return VeyraContentText::AbilityName(SpellId).ToString();
#else
	return SpellId.ToString();
#endif
}

bool UVeyraSmokeFlowSubsystem::TickHistory(const IVeyraClientIntents& Flow)
{
	if (bCheckedHistory)
	{
		return false;
	}
	const FVeyraMatchHistory& History = Flow.GetSnapshot().History;
	if (Flow.GetSnapshot().bBusy)
	{
		return true;
	}
	if (!bOpenedHistory)
	{
		// As a player does: the shell's Match History, newest first (UX-51).
		bOpenedHistory = Click(TEXT("Match History"));
		return true;
	}
	if (!History.bLoaded)
	{
		return true;
	}
	if (!History.Opened.IsSet())
	{
		if (History.Entries.IsEmpty() || History.Entries[0].MatchId != PlayedMatchId)
		{
			Finish(false, TEXT("Match History does not list the match just played first"));
			return true;
		}
		if (Capture(TEXT("History")))
		{
			return true;
		}
		UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: Match History lists %d match(es), newest %s, a %s."), History.Entries.Num(),
			*History.Entries[0].MatchId, *History.Entries[0].Outcome);
		Click(TEXT("Open"));
		return true;
	}
	if (!History.Opened->bHasScoreboard || History.Opened->MatchId != PlayedMatchId)
	{
		Finish(false, TEXT("the match opened from Match History has no saved scoreboard"));
		return true;
	}
	if (Capture(TEXT("HistoryRecord")))
	{
		return true;
	}
	UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: opened it from Match History, with its scoreboard of %d player(s)."), History.Opened->Players.Num());
	bCheckedHistory = Click(TEXT("Back to Match History"));
	return !bCheckedHistory;
}

bool UVeyraSmokeFlowSubsystem::Click(const FString& Label, int32 Occurrence)
{
#if WITH_VEYRA_UI
	const UVeyraShellUISubsystem* Shell = GetGameInstance()->GetSubsystem<UVeyraShellUISubsystem>();
	UVeyraShellScreen* Screen = Shell ? Shell->GetScreen() : nullptr;
	UVeyraShellButton* Button = Screen ? Screen->FindButton(FText::FromString(Label), Occurrence) : nullptr;
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
