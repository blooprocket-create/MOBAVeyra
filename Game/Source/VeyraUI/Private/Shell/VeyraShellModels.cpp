// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraShellModels.h"

#include "Text/VeyraContentText.h"

#define LOCTEXT_NAMESPACE "VeyraShell"

namespace VeyraShellModels
{
namespace
{
	using VeyraBackendProtocol::ESelectState;
	using VeyraBackendProtocol::FSelectSeat;

	constexpr int32 SecondsPerMinute = 60;

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
	// Anything else is shown as the flow reported it; the message never holds a credential.
	return FText::FromString(Problem.Message);
}

FText FormatCountdown(double Seconds)
{
	const int32 Whole = FMath::Max(0, FMath::CeilToInt(Seconds));
	return FText::FromString(FString::Printf(TEXT("%d:%02d"), Whole / SecondsPerMinute, Whole % SecondsPerMinute));
}

FVeyraSelectModel DescribeSelect(const FVeyraClientSnapshot& Snapshot, double RemainingSeconds, bool bCanHover, bool bCanLock)
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
		SeatModel.Name = Seat.bYou ? FText::Format(LOCTEXT("SeatYou", "{0} (you)"), FText::FromString(Seat.DisplayName)) : FText::FromString(Seat.DisplayName);
		SeatModel.Status = !Seat.Locked.IsEmpty() ? EVeyraSeatStatus::LockedIn : (!Seat.Hover.IsEmpty() ? EVeyraSeatStatus::NotLockedIn : EVeyraSeatStatus::Waiting);
		SeatModel.StatusText = SeatStatusText(SeatModel.Status);
		const FString& Shown = !Seat.Locked.IsEmpty() ? Seat.Locked : Seat.Hover;
		SeatModel.Vanguard = Shown.IsEmpty() ? FText::GetEmpty() : VanguardNameOf(Shown);
		Model.Seats.Add(MoveTemp(SeatModel));
	}

	const FString Chosen = You ? (!You->Locked.IsEmpty() ? You->Locked : You->Hover) : FString();
	for (const FString& Id : Snapshot.AvailableVanguards)
	{
		Model.Cards.Add(FVeyraSelectCardModel{ Id, VanguardNameOf(Id), Id == Chosen });
	}
	Model.bCanChoose = bCanHover;
	if (You && You->Locked.IsEmpty() && !You->Hover.IsEmpty())
	{
		Model.LockInVanguardId = You->Hover;
	}
	Model.bCanLockIn = bCanLock && !Model.LockInVanguardId.IsEmpty();
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
		Model.Headline = Outcome->Winner.IsEmpty() ? LOCTEXT("ResultNoWinner", "Match Over: No Winner")
												   : FText::Format(LOCTEXT("ResultWinner", "Match Over: Side {0} Won"), FText::FromString(Outcome->Winner));
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
	TStringBuilder<512> Text;
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
