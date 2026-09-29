// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraShellModels.h"

#include "Text/VeyraContentText.h"

#define LOCTEXT_NAMESPACE "VeyraShell"

namespace VeyraShellModels
{
namespace
{
	using VeyraBackendProtocol::EPartyStatus;
	using VeyraBackendProtocol::ESelectState;
	using VeyraBackendProtocol::FSelectSeat;

	constexpr int32 SecondsPerMinute = 60;
	/** The kind of champion select matchmaking opens, which a player may leave. */
	const TCHAR* const MatchmadeSelectKind = TEXT("casual");
	/** A player's answer to a match found, once given. */
	const TCHAR* const AcceptedAnswer = TEXT("accepted");
	const TCHAR* const DeclinedAnswer = TEXT("declined");

	/** "m:ss". */
	FText FormatClock(int32 WholeSeconds)
	{
		const int32 Whole = FMath::Max(0, WholeSeconds);
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), Whole / SecondsPerMinute, Whole % SecondsPerMinute));
	}

	FText SeatStatusText(EVeyraSeatStatus Status)
	{
		switch (Status)
		{
		case EVeyraSeatStatus::Waiting:
			return LOCTEXT("SeatWaiting", "Waiting");
		case EVeyraSeatStatus::NotLockedIn:
			return LOCTEXT("SeatNotLockedIn", "Not Locked In");
		case EVeyraSeatStatus::LockedIn:
			return LOCTEXT("SeatLockedIn", "Locked In");
		}
		return FText::GetEmpty();
	}

	FText EndReasonText(const FString& EndReason)
	{
		if (EndReason == TEXT("host_ended"))
		{
			return LOCTEXT("EndHostEnded", "The host ended the match.");
		}
		if (EndReason == TEXT("developer_request"))
		{
			return LOCTEXT("EndDeveloperRequest", "A developer ended the match.");
		}
		if (EndReason == TEXT("abandoned"))
		{
			return LOCTEXT("EndAbandoned", "The match was abandoned.");
		}
		if (EndReason == TEXT("prime_well_destroyed"))
		{
			return LOCTEXT("EndPrimeWellDestroyed", "A Prime Well was destroyed.");
		}
		return FText::Format(LOCTEXT("EndOther", "The match ended ({0})."), FText::FromString(EndReason));
	}

	FText FailureText(const FString& FailureReason)
	{
		if (FailureReason == TEXT("server_exited"))
		{
			return LOCTEXT("FailServerExited", "Its server stopped before reporting a result.");
		}
		if (FailureReason == TEXT("ready_timeout"))
		{
			return LOCTEXT("FailReadyTimeout", "Its server never became ready.");
		}
		if (FailureReason == TEXT("allocation_failed"))
		{
			return LOCTEXT("FailAllocation", "No server could be started for it.");
		}
		if (FailureReason == TEXT("max_duration"))
		{
			return LOCTEXT("FailMaxDuration", "It ran past its longest allowed length.");
		}
		return FText::Format(LOCTEXT("FailOther", "It failed ({0})."), FText::FromString(FailureReason));
	}
}

EVeyraShellScreen ScreenFor(EVeyraClientState State)
{
	switch (State)
	{
	case EVeyraClientState::InMatch:
		return EVeyraShellScreen::None;
	case EVeyraClientState::SignInFailed:
	case EVeyraClientState::SessionEnded:
		return EVeyraShellScreen::Stopped;
	case EVeyraClientState::StarterChoice:
		return EVeyraShellScreen::StarterChoice;
	case EVeyraClientState::Shell:
		return EVeyraShellScreen::Shell;
	case EVeyraClientState::MatchFound:
		return EVeyraShellScreen::MatchFound;
	case EVeyraClientState::Selecting:
		return EVeyraShellScreen::ChampionSelect;
	case EVeyraClientState::ReconnectOnly:
		return EVeyraShellScreen::ReconnectOnly;
	case EVeyraClientState::Results:
		return EVeyraShellScreen::Results;
	case EVeyraClientState::SigningIn:
	case EVeyraClientState::Loading:
	case EVeyraClientState::MatchStarting:
	case EVeyraClientState::Connecting:
	case EVeyraClientState::Returning:
	case EVeyraClientState::AwaitingResults:
		return EVeyraShellScreen::Status;
	}
	return EVeyraShellScreen::Status;
}

FText NameOf(const FString& ContentId)
{
	TArray<FString> Words;
	ContentId.ParseIntoArray(Words, TEXT("_"));
	for (FString& Word : Words)
	{
		Word[0] = FChar::ToUpper(Word[0]);
	}
	return FText::FromString(FString::Join(Words, TEXT(" ")));
}

FText VanguardNameOf(const FString& VanguardId)
{
	const TOptional<FVeyraContentId> Id = FVeyraContentId::FromText(VanguardId);
	return Id.IsSet() ? VeyraContentText::VanguardName(Id.GetValue()) : NameOf(VanguardId);
}

FVeyraStatusModel DescribeStatus(const FVeyraClientSnapshot& Snapshot)
{
	switch (Snapshot.State)
	{
	case EVeyraClientState::SigningIn:
		return { LOCTEXT("SigningInTitle", "Signing In"), LOCTEXT("SigningInDetail", "Waiting for the launcher's sign-in.") };
	case EVeyraClientState::Loading:
		return { LOCTEXT("LoadingTitle", "Loading"), LOCTEXT("LoadingDetail", "Finding your match, champion select or profile.") };
	case EVeyraClientState::MatchStarting:
		return { LOCTEXT("MatchStartingTitle", "Match Starting"), LOCTEXT("MatchStartingDetail", "Preparing gameplay: the match's server is starting.") };
	case EVeyraClientState::Connecting:
		return { LOCTEXT("ConnectingTitle", "Connecting to Match"), LOCTEXT("ConnectingDetail", "Joining the match's server.") };
	case EVeyraClientState::Returning:
		return { LOCTEXT("ReturningTitle", "Leaving the Match"), LOCTEXT("ReturningDetail", "Returning to the client.") };
	case EVeyraClientState::AwaitingResults:
		return { LOCTEXT("AwaitingResultsTitle", "Match Over"), LOCTEXT("AwaitingResultsDetail", "Waiting for the verified result.") };
	case EVeyraClientState::SignInFailed:
		return { LOCTEXT("SignInFailedTitle", "Sign-in Failed"),
			Snapshot.Problem.IsSet() ? DescribeProblem(*Snapshot.Problem) : LOCTEXT("SignInFailedDetail", "Close the game and sign in again from the launcher.") };
	case EVeyraClientState::SessionEnded:
		return { LOCTEXT("SessionEndedTitle", "Signed Out"), LOCTEXT("SessionEndedDetail", "Your session has ended. Close the game and sign in again from the launcher.") };
	default:
		return { FText::GetEmpty(), FText::GetEmpty() };
	}
}

FText DescribeNotice(const FString& Notice)
{
	if (Notice.IsEmpty())
	{
		return FText::GetEmpty();
	}
	if (Notice == TEXT("timed_out"))
	{
		return LOCTEXT("NoticeTimedOut", "Champion select ended: no Vanguard was locked in before the timer ran out.");
	}
	if (Notice == TEXT("allocation_failed") || Notice == TEXT("starting_timed_out"))
	{
		return LOCTEXT("NoticeNoServer", "Champion select ended: the match's server could not be started.");
	}
	if (Notice == TEXT("connection_lost"))
	{
		return LOCTEXT("NoticeConnectionLost", "You lost the connection to the match.");
	}
	if (Notice == TEXT("join_failed"))
	{
		return LOCTEXT("NoticeJoinFailed", "The match's server did not let you in.");
	}
	if (Notice == TEXT("left"))
	{
		return LOCTEXT("NoticeLeft", "Champion select ended: a player left it.");
	}
	if (Notice == TEXT("presence_lost"))
	{
		return LOCTEXT("NoticePresenceLost", "Champion select ended: a player lost their connection.");
	}
	if (Notice == TEXT("you_left"))
	{
		return LOCTEXT("NoticeYouLeft", "You left champion select, which ended it for everyone. Your party left the queue.");
	}
	if (Notice == TEXT("match_found_declined"))
	{
		return LOCTEXT("NoticeFoundDeclined", "You declined the match. Your party left the queue.");
	}
	if (Notice == TEXT("match_found_missed"))
	{
		return LOCTEXT("NoticeFoundMissed", "The match was not accepted in time. Your party left the queue.");
	}
	if (Notice == TEXT("match_found_abandoned"))
	{
		return LOCTEXT("NoticeFoundAbandoned", "Someone in your party did not accept the match. Your party left the queue.");
	}
	if (Notice == TEXT("match_found_requeued"))
	{
		// Also after a block between two players, which no one may learn of (Parties & Social Bible §6).
		return LOCTEXT("NoticeFoundRequeued", "The match did not go ahead. You are back in the queue, in your place.");
	}
	if (Notice == TEXT("no_longer_matched"))
	{
		return LOCTEXT("NoticeNoLongerMatched", "Champion select ended: this match can no longer go ahead. You are back in the queue.");
	}
	return FText::Format(LOCTEXT("NoticeOther", "Notice: {0}"), FText::FromString(Notice));
}

FText DescribeProblem(const FVeyraClientProblem& Problem)
{
	if (Problem.Code == TEXT("backend_unreachable"))
	{
		return LOCTEXT("ProblemUnreachable", "Veyra's services did not answer.");
	}
	if (Problem.Code == TEXT("not_available"))
	{
		return LOCTEXT("ProblemNotAvailable", "That Vanguard is not available to you.");
	}
	if (Problem.Code == TEXT("practice_disabled"))
	{
		return LOCTEXT("ProblemPracticeDisabled", "Practice is not available right now.");
	}
	if (Problem.Code == TEXT("match_not_ready"))
	{
		return LOCTEXT("ProblemMatchNotReady", "The match's server is taking too long to start.");
	}
	if (Problem.Code == TEXT("taken"))
	{
		return LOCTEXT("ProblemTaken", "Another player has already locked in that Vanguard.");
	}
	if (Problem.Code == TEXT("not_all_ready"))
	{
		return LOCTEXT("ProblemNotAllReady", "Everyone in the party must be Ready first.");
	}
	if (Problem.Code == TEXT("mode_not_available"))
	{
		return LOCTEXT("ProblemModeNotAvailable", "That mode is not available yet.");
	}
	if (Problem.Code == TEXT("party_too_large_for_mode"))
	{
		return LOCTEXT("ProblemPartyTooLarge", "Your party is too large for that mode.");
	}
	if (Problem.Code == TEXT("not_leader"))
	{
		return LOCTEXT("ProblemNotLeader", "Only the party leader can do that.");
	}
	if (Problem.Code == TEXT("member_busy"))
	{
		return LOCTEXT("ProblemMemberBusy", "Someone in your party is still in a match or champion select.");
	}
	// Anything else is shown as the flow reported it; the message never holds a credential.
	return FText::FromString(Problem.Message);
}

FText FormatCountdown(double Seconds)
{
	return FormatClock(FMath::CeilToInt(Seconds));
}

FVeyraSelectModel DescribeSelect(const FVeyraClientSnapshot& Snapshot, double RemainingSeconds, bool bCanHover, bool bCanLock, bool bCanLeave)
{
	const VeyraBackendProtocol::FSelect& Select = Snapshot.Select;
	FVeyraSelectModel Model;
	Model.Title = FText::Format(LOCTEXT("SelectTitle", "{0}: Champion Select"), NameOf(Select.Mode));
	Model.Countdown = FormatCountdown(RemainingSeconds);
	const FSelectSeat* You = Select.FindYou();
	if (Select.State == ESelectState::Starting)
	{
		Model.Phase = LOCTEXT("SelectStarting", "Everyone is locked in. The match is being created.");
	}
	else if (You && !You->Locked.IsEmpty())
	{
		Model.Phase = LOCTEXT("SelectLockedIn", "Locked in. Waiting for the others.");
	}
	else
	{
		Model.Phase = LOCTEXT("SelectPicking", "Choose your Vanguard and lock it in.");
	}

	for (const FSelectSeat& Seat : Select.Seats)
	{
		FVeyraSelectSeatModel SeatModel;
		SeatModel.bYou = Seat.bYou;
		// The backend never shows the enemy team's hovers, only its locks.
		SeatModel.bAlly = !You || Seat.Side == You->Side;
		Model.bTeams |= !SeatModel.bAlly;
		SeatModel.Name = Seat.bYou ? FText::Format(LOCTEXT("SeatYou", "{0} (you)"), FText::FromString(Seat.DisplayName)) : FText::FromString(Seat.DisplayName);
		SeatModel.Status = !Seat.Locked.IsEmpty() ? EVeyraSeatStatus::LockedIn : (!Seat.Hover.IsEmpty() ? EVeyraSeatStatus::NotLockedIn : EVeyraSeatStatus::Waiting);
		SeatModel.StatusText = SeatStatusText(SeatModel.Status);
		const FString& Shown = !Seat.Locked.IsEmpty() ? Seat.Locked : Seat.Hover;
		SeatModel.Vanguard = Shown.IsEmpty() ? FText::GetEmpty() : VanguardNameOf(Shown);
		Model.Seats.Add(MoveTemp(SeatModel));
	}

	const FString Chosen = You ? (!You->Locked.IsEmpty() ? You->Locked : You->Hover) : FString();
	const auto IsTaken = [&Select](const FString& Id) {
		return Select.Seats.ContainsByPredicate([&Id](const FSelectSeat& Seat) { return !Seat.bYou && Seat.Locked == Id; });
	};
	for (const FString& Id : Snapshot.AvailableVanguards)
	{
		Model.Cards.Add(FVeyraSelectCardModel{ Id, VanguardNameOf(Id), Id == Chosen, IsTaken(Id) });
	}
	Model.bCanChoose = bCanHover;
	if (You && You->Locked.IsEmpty() && !You->Hover.IsEmpty())
	{
		Model.LockInVanguardId = You->Hover;
	}
	Model.bCanLockIn = bCanLock && !Model.LockInVanguardId.IsEmpty() && !IsTaken(Model.LockInVanguardId);
	Model.bOffersLeave = Select.Kind == MatchmadeSelectKind && Select.State == ESelectState::Picking;
	Model.bCanLeave = bCanLeave;
	return Model;
}

TArray<FVeyraModeCardModel> DescribeModes(const FVeyraClientSnapshot& Snapshot)
{
	TArray<FVeyraModeCardModel> Cards;
	const FString PartyMode = Snapshot.Party.IsSet() ? Snapshot.Party->Mode : FString();
	for (const VeyraBackendProtocol::FModeInfo& Mode : Snapshot.Modes)
	{
		if (!Mode.bEnabled)
		{
			continue;
		}
		FVeyraModeCardModel Card;
		Card.ModeId = Mode.Id;
		Card.Name = NameOf(Mode.Id);
		Card.Format = FText::Format(LOCTEXT("ModeFormat", "{0}v{0}"), FText::AsNumber(Mode.HumanPlayersPerTeam));
		Card.bAvailable = Mode.bMatchmade;
		if (!Mode.bMatchmade)
		{
			Card.Availability = LOCTEXT("ModeNotYetAvailable", "Not yet available");
		}
		Card.bSelected = Mode.Id == PartyMode;
		Cards.Add(MoveTemp(Card));
	}
	return Cards;
}

FVeyraPartyModel DescribeParty(const FVeyraClientSnapshot& Snapshot, bool bCanReady, bool bCanFindMatch, bool bCanCancel)
{
	FVeyraPartyModel Model;
	if (!Snapshot.Party.IsSet())
	{
		return Model;
	}
	const VeyraBackendProtocol::FParty& Party = *Snapshot.Party;
	const VeyraBackendProtocol::FPartyMember* You = Party.Find(Snapshot.AccountId);
	const bool bLeader = You && You->bLeader;
	Model.bShown = true;
	Model.Mode = Party.Mode.IsEmpty() ? LOCTEXT("PartyNoMode", "No mode chosen yet: the leader chooses one in Play.")
									  : FText::Format(LOCTEXT("PartyMode", "Mode: {0}"), NameOf(Party.Mode));
	for (const VeyraBackendProtocol::FPartyMember& Member : Party.Members)
	{
		const FText Name = FText::FromString(Member.DisplayName);
		const bool bIsYou = Member.AccountId == Snapshot.AccountId;
		const FText Who = bIsYou && Member.bLeader ? FText::Format(LOCTEXT("MemberYouLeader", "{0} (you, leader)"), Name)
			: bIsYou								? FText::Format(LOCTEXT("MemberYou", "{0} (you)"), Name)
			: Member.bLeader						? FText::Format(LOCTEXT("MemberLeader", "{0} (leader)"), Name)
													: Name;
		Model.Members.Add(FText::Format(LOCTEXT("MemberLine", "{0}: {1}"), Who, Member.bReady ? LOCTEXT("MemberReady", "Ready") : LOCTEXT("MemberNotReady", "Not Ready")));
	}
	Model.bQueued = Party.Status != EPartyStatus::Idle;
	if (!Model.bQueued && !Party.Mode.IsEmpty())
	{
		if (!Party.AllReady())
		{
			Model.Status = bLeader ? LOCTEXT("PartyLeaderWaits", "Find Match opens once everyone is Ready.")
								   : LOCTEXT("PartyMemberWaits", "Ready up: the leader finds a match once everyone is Ready.");
		}
		else
		{
			Model.Status = bLeader ? LOCTEXT("PartyLeaderReady", "Everyone is Ready.") : LOCTEXT("PartyMemberReady", "Everyone is Ready: the leader finds a match.");
		}
	}
	Model.bReadyTarget = !(You && You->bReady);
	Model.ReadyLabel = Model.bReadyTarget ? LOCTEXT("ReadyUp", "Ready") : LOCTEXT("Unready", "Unready");
	Model.bCanReady = bCanReady;
	Model.bOffersFindMatch = bLeader && !Model.bQueued;
	Model.bCanFindMatch = bCanFindMatch;
	Model.bOffersCancel = bLeader && Model.bQueued;
	Model.bCanCancel = bCanCancel;
	return Model;
}

FText FormatQueueStatus(double QueuedSeconds)
{
	// Elapsed time only: there is no estimator yet, and the canon forbids a made-up one (UX-2).
	return FText::Format(LOCTEXT("QueueStatus", "In queue: {0}. Estimate unavailable."), FormatClock(FMath::FloorToInt(QueuedSeconds)));
}

FVeyraMatchFoundModel DescribeMatchFound(const FVeyraClientSnapshot& Snapshot, double RemainingSeconds, bool bCanAnswer)
{
	const VeyraBackendProtocol::FMatchFound& Found = Snapshot.MatchFound;
	FVeyraMatchFoundModel Model;
	Model.Title = LOCTEXT("MatchFoundTitle", "Match Found");
	Model.Mode = NameOf(Found.Mode);
	Model.Countdown = FormatCountdown(RemainingSeconds);
	Model.Progress = FText::Format(LOCTEXT("MatchFoundProgress", "{0} of {1} accepted"), FText::AsNumber(Found.Accepted), FText::AsNumber(Found.Total));
	if (Found.You == AcceptedAnswer)
	{
		Model.Phase = LOCTEXT("MatchFoundAccepted", "Accepted. Waiting for the other players.");
	}
	else if (Found.You == DeclinedAnswer)
	{
		Model.Phase = LOCTEXT("MatchFoundDeclined", "Declined.");
	}
	else
	{
		Model.Phase = LOCTEXT("MatchFoundPending", "Accept to play. Declining takes your party out of the queue.");
	}
	Model.bCanAnswer = bCanAnswer;
	return Model;
}

FVeyraResultsModel DescribeResults(const FVeyraClientSnapshot& Snapshot)
{
	FVeyraResultsModel Model;
	const TOptional<VeyraBackendProtocol::FMatchOutcome>& Outcome = Snapshot.Result;
	if (!Outcome.IsSet())
	{
		// Pending, not fabricated (UX-15, UX-17).
		Model.Headline = LOCTEXT("ResultPending", "Result Not Available Yet");
		Model.Lines.Add(LOCTEXT("ResultPendingDetail", "Veyra's services have not confirmed how this match ended."));
	}
	else if (!Outcome->bHasResult)
	{
		Model.bVerified = true;
		Model.Headline = LOCTEXT("ResultFailed", "The Match Did Not Finish");
		Model.Lines.Add(FailureText(Outcome->FailureReason));
	}
	else
	{
		Model.bVerified = true;
		// The player's own side against the winner's (ADR-011 §13).
		if (Outcome->Winner.IsEmpty())
		{
			Model.Headline = LOCTEXT("ResultNoWinner", "Match Over: No Winner");
		}
		else if (Outcome->Side.IsEmpty())
		{
			Model.Headline = FText::Format(LOCTEXT("ResultWinner", "Match Over: Side {0} Won"), FText::FromString(Outcome->Winner));
		}
		else
		{
			Model.Headline = Outcome->Winner == Outcome->Side ? LOCTEXT("ResultVictory", "Victory") : LOCTEXT("ResultDefeat", "Defeat");
		}
		Model.Lines.Add(EndReasonText(Outcome->EndReason));
		Model.Lines.Add(FText::Format(LOCTEXT("ResultMode", "Mode: {0}"), NameOf(Outcome->Mode)));
		if (!Outcome->VanguardId.IsEmpty())
		{
			Model.Lines.Add(FText::Format(LOCTEXT("ResultVanguard", "Your Vanguard: {0}"), VanguardNameOf(Outcome->VanguardId)));
		}
		Model.Lines.Add(FText::Format(LOCTEXT("ResultDuration", "Duration: {0}"), FormatCountdown(Outcome->DurationSeconds)));
	}
	if (const FText Notice = DescribeNotice(Snapshot.Notice); !Notice.IsEmpty())
	{
		Model.Lines.Add(Notice);
	}
	return Model;
}

FString Signature(const FVeyraClientSnapshot& Snapshot)
{
	TStringBuilder<1024> Text;
	Text << LexToString(Snapshot.State) << TEXT("|") << Snapshot.DisplayName << TEXT("|") << (Snapshot.bBusy ? TEXT("busy") : TEXT("idle")) << TEXT("|") << Snapshot.Notice;
	if (Snapshot.Problem.IsSet())
	{
		Text << TEXT("|problem:") << Snapshot.Problem->Code << TEXT(":") << Snapshot.Problem->Message << (Snapshot.Problem->bCanRetry ? TEXT(":retry") : TEXT(""));
	}
	Text << TEXT("|starters:") << FString::Join(Snapshot.Starters, TEXT(",")) << TEXT("|available:") << FString::Join(Snapshot.AvailableVanguards, TEXT(","));
	if (Snapshot.State == EVeyraClientState::Selecting)
	{
		Text << TEXT("|select:") << Snapshot.Select.Id << TEXT(":") << static_cast<int32>(Snapshot.Select.State);
		for (const FSelectSeat& Seat : Snapshot.Select.Seats)
		{
			Text << TEXT(";") << Seat.DisplayName << TEXT(":") << Seat.Hover << TEXT(":") << Seat.Locked;
		}
	}
	Text << TEXT("|modes:");
	for (const VeyraBackendProtocol::FModeInfo& Mode : Snapshot.Modes)
	{
		Text << Mode.Id << TEXT(":") << (Mode.bEnabled ? TEXT("on") : TEXT("off")) << TEXT(":") << (Mode.bMatchmade ? TEXT("matchmade") : TEXT("unmatched")) << TEXT(":")
			 << Mode.HumanPlayersPerTeam << TEXT(";");
	}
	// The queue's time and the acceptance timer count on every frame without a rebuild.
	if (Snapshot.Party.IsSet())
	{
		const VeyraBackendProtocol::FParty& Party = *Snapshot.Party;
		Text << TEXT("|party:") << Party.Id << TEXT(":") << Party.Mode << TEXT(":") << static_cast<int32>(Party.Status);
		for (const VeyraBackendProtocol::FPartyMember& Member : Party.Members)
		{
			Text << TEXT(";") << Member.AccountId << TEXT(":") << Member.DisplayName << TEXT(":") << (Member.bReady ? TEXT("ready") : TEXT("not ready"))
				 << (Member.bLeader ? TEXT(":leader") : TEXT(""));
		}
	}
	if (Snapshot.State == EVeyraClientState::MatchFound)
	{
		const VeyraBackendProtocol::FMatchFound& Found = Snapshot.MatchFound;
		Text << TEXT("|found:") << Found.Id << TEXT(":") << Found.State << TEXT(":") << Found.You << TEXT(":") << Found.Accepted << TEXT("/") << Found.Total;
	}
	Text << TEXT("|match:") << Snapshot.MatchId;
	if (Snapshot.Result.IsSet())
	{
		const VeyraBackendProtocol::FMatchOutcome& Outcome = *Snapshot.Result;
		Text << TEXT("|result:") << Outcome.State << TEXT(":") << Outcome.EndReason << TEXT(":") << Outcome.FailureReason;
	}
	return FString(Text.ToString());
}
}

#undef LOCTEXT_NAMESPACE
