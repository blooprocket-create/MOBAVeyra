// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraShellModels.h"

#include "Algo/Count.h"
#include "Shell/VeyraChatModels.h"
#include "Shell/VeyraProgressionModels.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "Slots/VeyraAbilitySlot.h"
#include "Text/VeyraContentText.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraFluxTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"

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
	/** A matchmade Draft Pick select (ADR-042). */
	const TCHAR* const DraftSelectKind = TEXT("draft");
	/** A custom lobby's champion select, which a player may leave too, back to the lobby. */
	const TCHAR* const CustomSelectKind = TEXT("custom");
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
		if (EndReason == TEXT("surrender"))
		{
			return LOCTEXT("EndSurrender", "A team surrendered.");
		}
		if (EndReason == TEXT("remake"))
		{
			return LOCTEXT("EndRemake", "The match was remade: no contest.");
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
	case EVeyraClientState::Lobby:
		return EVeyraShellScreen::Lobby;
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

FText ModeNameOf(const FString& ModeId)
{
	const TOptional<FVeyraContentId> Id = FVeyraContentId::FromText(ModeId);
	return Id.IsSet() ? VeyraContentText::ModeName(Id.GetValue(), NameOf(ModeId).ToString()) : NameOf(ModeId);
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
	if (Notice == TEXT("you_left_custom"))
	{
		return LOCTEXT("NoticeYouLeftCustom", "You left champion select, which ended it for everyone. The lobby is open again.");
	}
	if (Notice == TEXT("lobby_gone"))
	{
		return LOCTEXT("NoticeLobbyGone", "You are no longer in the custom lobby: the host removed you, or it closed.");
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
	if (Problem.Code == TEXT("invalid_flux_spells"))
	{
		return LOCTEXT("ProblemInvalidFluxSpells", "Those Flux Spells cannot be taken: each slot takes a different spell.");
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
		return LOCTEXT("ProblemMemberBusy", "Someone is still in a match, champion select or queue.");
	}
	// The custom lobby's (ADR-021).
	if (Problem.Code == TEXT("vanguard_taken"))
	{
		return LOCTEXT("ProblemVanguardTaken", "A bot on that side already plays that Vanguard.");
	}
	if (Problem.Code == TEXT("seat_taken"))
	{
		return LOCTEXT("ProblemSeatTaken", "That seat is taken.");
	}
	if (Problem.Code == TEXT("victory_needs_both_sides"))
	{
		return LOCTEXT("ProblemVictoryNeedsSides", "Victory needs a Vanguard on each side.");
	}
	if (Problem.Code == TEXT("not_host"))
	{
		return LOCTEXT("ProblemNotHost", "Only the lobby's host can do that.");
	}
	if (Problem.Code == TEXT("lobby_full"))
	{
		return LOCTEXT("ProblemLobbyFull", "The lobby has no empty seat.");
	}
	if (Problem.Code == TEXT("starting_gold_out_of_range"))
	{
		return LOCTEXT("ProblemGoldRange", "That starting Gold is outside what custom matches allow.");
	}
	if (Problem.Code == TEXT("launch_unavailable"))
	{
		return LOCTEXT("ProblemLaunchUnavailable", "Custom matches cannot be started right now.");
	}
	// Anything else is shown as the flow reported it; the message never holds a credential.
	return FText::FromString(Problem.Message);
}

FText FormatCountdown(double Seconds)
{
	return FormatClock(FMath::CeilToInt(Seconds));
}

FText SpellSlotTitle(int32 Slot)
{
	return FText::Format(LOCTEXT("SpellSlotTitle", "Flux Spell {0}"), FText::AsNumber(Slot + 1));
}

namespace
{
	/** The shown Vanguard's passive, then its Q, W, E and R: the first ability each key casts. */
	TArray<FVeyraAbilityLineModel> DescribeAbilities(const FString& VanguardId)
	{
		TArray<FVeyraAbilityLineModel> Lines;
		const TOptional<FVeyraContentId> Id = FVeyraContentId::FromText(VanguardId);
		const FVeyraVanguardDefinition* Definition = Id.IsSet() ? UVeyraVanguardsTuningSubsystem::FindVanguard(Id.GetValue()) : nullptr;
		if (!Definition)
		{
			return Lines;
		}
		for (const FVeyraContentId& Passive : Definition->Passive)
		{
			Lines.Add(FVeyraAbilityLineModel{ LOCTEXT("PassiveKey", "Passive"), VeyraContentText::PassiveName(Passive), VeyraContentText::PassiveDescription(Passive),
				Passive.ToString() });
		}
		const TPair<FText, const TArray<FVeyraContentId>*> Keys[] = {
			{ LOCTEXT("QKey", "Q"), &Definition->Abilities.Q },
			{ LOCTEXT("WKey", "W"), &Definition->Abilities.W },
			{ LOCTEXT("EKey", "E"), &Definition->Abilities.E },
			{ LOCTEXT("RKey", "R"), &Definition->Abilities.R },
		};
		for (const TPair<FText, const TArray<FVeyraContentId>*>& Key : Keys)
		{
			if (!Key.Value->IsEmpty())
			{
				const FVeyraContentId& Ability = (*Key.Value)[0];
				Lines.Add(FVeyraAbilityLineModel{ Key.Key, VeyraContentText::AbilityName(Ability), VeyraContentText::AbilityDescription(Ability), Ability.ToString() });
			}
		}
		return Lines;
	}

	FText SpellNameOf(const FString& SpellId)
	{
		const TOptional<FVeyraContentId> Id = FVeyraContentId::FromText(SpellId);
		return Id.IsSet() ? VeyraContentText::AbilityName(Id.GetValue()) : FText::FromString(SpellId);
	}

	/** The player's two spell slots, and once locked in the Match Setup summary (Pre-Game Client UX Bible 36, 38). */
	void DescribeFluxSpells(const FSelectSeat& You, FVeyraSelectModel& Model)
	{
		const TArray<FVeyraContentId>& Roster = UVeyraAbilitiesTuningSubsystem::Get().FluxSpells.Roster;
		const TArray<double>& Thresholds = UVeyraFluxTuningSubsystem::Get().SpellSlots.Thresholds;
		TArray<FText> Summary;
		for (int32 Slot = 0; Slot < static_cast<int32>(UE_ARRAY_COUNT(VeyraAbilitySlots::Spells)); ++Slot)
		{
			const FString Chosen = You.FluxSpells.IsValidIndex(Slot) ? You.FluxSpells[Slot] : FString();
			FVeyraSpellSlotModel& SlotModel = Model.SpellSlots.AddDefaulted_GetRef();
			SlotModel.Slot = Slot;
			SlotModel.Title = SpellSlotTitle(Slot);
			const FText Threshold = Thresholds.IsValidIndex(Slot) ? FText::AsNumber(Thresholds[Slot]) : FText::GetEmpty();
			SlotModel.Unlock = FText::Format(LOCTEXT("SpellSlotUnlock", "Unlocks at {0} permanent Team Flux"), Threshold);
			SlotModel.Chosen = Chosen.IsEmpty() ? LOCTEXT("SpellSlotEmpty", "Empty") : SpellNameOf(Chosen);
			SlotModel.ChosenId = Chosen;
			SlotModel.Choices.Add(FVeyraSpellChoiceModel{ FString(), LOCTEXT("SpellNone", "None"), FText::GetEmpty(), Chosen.IsEmpty() });
			for (const FVeyraContentId& Spell : Roster)
			{
				SlotModel.Choices.Add(FVeyraSpellChoiceModel{ Spell.ToString(), VeyraContentText::AbilityName(Spell), VeyraContentText::AbilityDescription(Spell),
					Spell.ToString() == Chosen });
			}
			Summary.Add(FText::Format(LOCTEXT("SetupSpell", "{0} ({1} Flux)"), SlotModel.Chosen, Threshold));
		}
		if (!You.Locked.IsEmpty())
		{
			Model.Setup = FText::Format(LOCTEXT("MatchSetup", "Your Match Setup: {0}, locked in. Flux Spells: {1}."), VanguardNameOf(You.Locked),
				FText::Join(FText::FromString(TEXT(", ")), Summary));
		}
	}
}

namespace
{
	/**
	 * What happens now, as the banner under the timer says it: whose turn it is in a draft (ADR-042 §1),
	 * the final window, or the single phase's picking.
	 */
	FText DescribeSelectPhase(const VeyraBackendProtocol::FSelect& Select, const FSelectSeat* You)
	{
		const bool bLocked = You && !You->Locked.IsEmpty();
		if (Select.State == ESelectState::Starting)
		{
			return LOCTEXT("SelectStarting", "Everyone is locked in. The match is being created.");
		}
		if (Select.Phase == VeyraBackendProtocol::ESelectPhase::Final)
		{
			return LOCTEXT("SelectFinal", "Everyone is locked in. Locked teammates may trade until the match starts.");
		}
		if (Select.Turn.IsSet() && You)
		{
			const bool bYourSide = Select.Turn->Side == You->Side;
			if (Select.Turn->bBan)
			{
				return You->bActing ? LOCTEXT("SelectYourBan", "Your turn to ban.")
									: (bYourSide ? LOCTEXT("SelectTeamBans", "Your team is banning.") : LOCTEXT("SelectEnemyBans", "The enemy team is banning."));
			}
			if (You->bActing)
			{
				return LOCTEXT("SelectYourPick", "Your turn to pick: lock in your Vanguard.");
			}
			if (!bLocked)
			{
				return bYourSide ? LOCTEXT("SelectTeamPicks", "Your team is picking. Hover your Vanguard to show them.")
								 : LOCTEXT("SelectEnemyPicks", "The enemy team is picking. Hover your Vanguard to show your team.");
			}
		}
		if (bLocked)
		{
			return LOCTEXT("SelectLockedIn", "Locked in. Waiting for the others.");
		}
		return LOCTEXT("SelectPicking", "Choose your Vanguard and lock it in.");
	}
}

FVeyraSelectModel DescribeSelect(const FVeyraClientSnapshot& Snapshot, double RemainingSeconds, bool bCanHover, bool bCanLock, bool bCanLeave,
	bool bCanChooseSpells, const FVeyraSelectDraftPermissions& Draft)
{
	const VeyraBackendProtocol::FSelect& Select = Snapshot.Select;
	FVeyraSelectModel Model;
	Model.Title = FText::Format(LOCTEXT("SelectTitle", "{0}: Champion Select"), ModeNameOf(Select.Mode));
	Model.Countdown = FormatCountdown(RemainingSeconds);
	const FSelectSeat* You = Select.FindYou();
	Model.Phase = DescribeSelectPhase(Select, You);
	Model.bDraft = Select.Kind == DraftSelectKind;
	Model.bBanning = Select.YouBan();
	for (const VeyraBackendProtocol::FSelectBan& Ban : Select.Bans)
	{
		Model.Bans.Add(FVeyraSelectBanModel{ Ban.VanguardId, VanguardNameOf(Ban.VanguardId), !You || Ban.Side == You->Side });
	}

	for (int32 Index = 0; Index < Select.Seats.Num(); ++Index)
	{
		const FSelectSeat& Seat = Select.Seats[Index];
		FVeyraSelectSeatModel SeatModel;
		SeatModel.bYou = Seat.bYou;
		SeatModel.SeatIndex = Index;
		// The backend never shows the enemy team's hovers, only its locks.
		SeatModel.bAlly = !You || Seat.Side == You->Side;
		Model.bTeams |= !SeatModel.bAlly;
		SeatModel.Name = Seat.bYou ? FText::Format(LOCTEXT("SeatYou", "{0} (you)"), FText::FromString(Seat.DisplayName)) : FText::FromString(Seat.DisplayName);
		SeatModel.Status = !Seat.Locked.IsEmpty() ? EVeyraSeatStatus::LockedIn : (!Seat.Hover.IsEmpty() ? EVeyraSeatStatus::NotLockedIn : EVeyraSeatStatus::Waiting);
		SeatModel.StatusText = SeatStatusText(SeatModel.Status);
		SeatModel.bActing = Seat.bActing;
		if (!Seat.BanHover.IsEmpty())
		{
			SeatModel.BanHover = FText::Format(LOCTEXT("SeatBanHover", "Banning {0}"), VanguardNameOf(Seat.BanHover));
		}
		// Trades are between locked teammates, offered by seat (ADR-042 §2).
		SeatModel.bOfferedByYou = Seat.bOfferedByYou;
		SeatModel.bOffersYou = Seat.bOffersYou;
		SeatModel.bCanOfferTrade = Draft.bCanOfferTrade && !Seat.bYou && SeatModel.bAlly && !Seat.Locked.IsEmpty() && !Seat.bOfferedByYou;
		SeatModel.bCanAnswerTrade = Draft.bCanAnswerTrade && Seat.bOffersYou;
		const FString& Shown = !Seat.Locked.IsEmpty() ? Seat.Locked : Seat.Hover;
		SeatModel.Vanguard = Shown.IsEmpty() ? FText::GetEmpty() : VanguardNameOf(Shown);
		SeatModel.VanguardId = Shown;
		for (const FString& Spell : Seat.FluxSpells)
		{
			SeatModel.Spells.Add(Spell.IsEmpty() ? FText::GetEmpty() : SpellNameOf(Spell));
		}
		Model.Seats.Add(MoveTemp(SeatModel));
	}
	// A custom lobby's bots sit locked from the start (ADR-021 §3).
	for (const VeyraBackendProtocol::FSelectBot& Bot : Select.Bots)
	{
		FVeyraSelectSeatModel SeatModel;
		SeatModel.bAlly = !You || Bot.Side == You->Side;
		Model.bTeams |= !SeatModel.bAlly;
		SeatModel.Name = FText::Format(LOCTEXT("SeatBot", "{0} Bot"), DifficultyName(Bot.Difficulty));
		SeatModel.Status = EVeyraSeatStatus::LockedIn;
		SeatModel.StatusText = SeatStatusText(SeatModel.Status);
		SeatModel.Vanguard = VanguardNameOf(Bot.VanguardId);
		SeatModel.VanguardId = Bot.VanguardId;
		Model.Seats.Add(MoveTemp(SeatModel));
	}

	const FString Chosen = You ? (!You->Locked.IsEmpty() ? You->Locked : You->Hover) : FString();
	// A matchmade select's Vanguards are unique across both teams; a custom select's on each side, bots too,
	// so both sides may play the same one (ADR-021 §8). A draft's bans take theirs from both teams.
	const bool bPerSide = Select.Kind == CustomSelectKind;
	const auto IsTaken = [&Select, You, bPerSide](const FString& Id) {
		const bool bBySeat = Select.Seats.ContainsByPredicate([&Id, You, bPerSide](const FSelectSeat& Seat) {
			return !Seat.bYou && Seat.Locked == Id && (!bPerSide || (You && Seat.Side == You->Side));
		});
		const bool bByBot = Select.Bots.ContainsByPredicate([&Id, You](const VeyraBackendProtocol::FSelectBot& Bot) {
			return Bot.VanguardId == Id && You && Bot.Side == You->Side;
		});
		return bBySeat || bByBot || Select.IsBanned(Id);
	};
	if (Model.bBanning)
	{
		// The player's ban turn: any released Vanguard may be banned, owned or not, once (ADR-042 §1).
		const FString BanHover = You ? You->BanHover : FString();
		for (const FString& Id : Snapshot.ReleasedVanguards)
		{
			const bool bBanned = Select.IsBanned(Id);
			Model.Cards.Add(FVeyraSelectCardModel{ Id, VanguardNameOf(Id), Id == BanHover, bBanned, bBanned });
		}
		Model.bCanChoose = Draft.bCanBan;
		Model.BanVanguardId = BanHover;
		Model.bCanBan = Draft.bCanBan && !BanHover.IsEmpty() && !Select.IsBanned(BanHover);
	}
	else
	{
		for (const FString& Id : Snapshot.AvailableVanguards)
		{
			Model.Cards.Add(FVeyraSelectCardModel{ Id, VanguardNameOf(Id), Id == Chosen, IsTaken(Id), Select.IsBanned(Id) });
		}
		Model.bCanChoose = bCanHover;
	}
	if (You && You->Locked.IsEmpty() && !You->Hover.IsEmpty())
	{
		Model.LockInVanguardId = You->Hover;
	}
	Model.bCanLockIn = bCanLock && !Model.LockInVanguardId.IsEmpty() && !IsTaken(Model.LockInVanguardId);
	Model.bOffersLeave = (Select.Kind == MatchmadeSelectKind || Select.Kind == DraftSelectKind || Select.Kind == CustomSelectKind)
		&& Select.State == ESelectState::Picking;
	Model.bCanLeave = bCanLeave;
	if (You)
	{
		DescribeFluxSpells(*You, Model);
	}
	Model.bCanChooseSpells = bCanChooseSpells && You;
	Model.ShownVanguardId = Chosen;
	if (!Chosen.IsEmpty())
	{
		Model.ShownName = VanguardNameOf(Chosen);
		const TOptional<FVeyraContentId> ChosenId = FVeyraContentId::FromText(Chosen);
		Model.ShownTitle = ChosenId.IsSet() ? VeyraContentText::VanguardTitle(ChosenId.GetValue()) : FText::GetEmpty();
		Model.Abilities = DescribeAbilities(Chosen);
	}
	Model.ModeLabel = ModeNameOf(Select.Mode).ToUpper();
	Model.PickSeconds = Select.PickSeconds;
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
		Card.Category = Mode.Category;
		Card.Name = ModeNameOf(Mode.Id);
		// Against an enemy AI team, its humans alone (ADR-039 §2).
		Card.Format = Mode.bVersusAI ? FText::Format(LOCTEXT("ModeFormatVersusAI", "{0} vs AI"), FText::AsNumber(Mode.HumanPlayersPerTeam))
									 : FText::Format(LOCTEXT("ModeFormat", "{0}v{0}"), FText::AsNumber(Mode.HumanPlayersPerTeam));
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

FVeyraPartyModel DescribeParty(const FVeyraClientSnapshot& Snapshot, bool bCanReady, bool bCanFindMatch, bool bCanCancel, const FVeyraPartyPermissions& Permissions)
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
									  : FText::Format(LOCTEXT("PartyMode", "Mode: {0}"), ModeNameOf(Party.Mode));
	for (const VeyraBackendProtocol::FPartyMember& Member : Party.Members)
	{
		const FText Name = FText::FromString(Member.DisplayName);
		const bool bIsYou = Member.AccountId == Snapshot.AccountId;
		const FText Who = bIsYou && Member.bLeader ? FText::Format(LOCTEXT("MemberYouLeader", "{0} (you, leader)"), Name)
			: bIsYou								? FText::Format(LOCTEXT("MemberYou", "{0} (you)"), Name)
			: Member.bLeader						? FText::Format(LOCTEXT("MemberLeader", "{0} (leader)"), Name)
													: Name;
		const FText Line = FText::Format(LOCTEXT("MemberLine", "{0}: {1}"), Who, Member.bReady ? LOCTEXT("MemberReady", "Ready") : LOCTEXT("MemberNotReady", "Not Ready"));
		Model.Members.Add(Line);
		// The leader's card actions are on the other members' cards (UX-10, UX-11).
		FVeyraPartyMemberModel Card;
		Card.AccountId = Member.AccountId;
		Card.Name = Member.DisplayName;
		Card.Line = Line;
		Card.bYou = bIsYou;
		Card.bLeader = Member.bLeader;
		Card.bOffersActions = bLeader && !bIsYou;
		Card.bCanMakeLeader = Card.bOffersActions && Permissions.bCanTransfer;
		Card.bCanRemove = Card.bOffersActions && Permissions.bCanKick;
		Model.Cards.Add(MoveTemp(Card));
	}
	const bool bPublic = Party.Privacy == VeyraBackendProtocol::EPartyPrivacy::Public;
	Model.Privacy = bPublic ? LOCTEXT("PartyPublic", "Public: friends may join.") : LOCTEXT("PartyPrivate", "Private: members invite friends.");
	Model.bOffersPrivacy = bLeader;
	Model.PrivacyTarget = bPublic ? VeyraBackendProtocol::EPartyPrivacy::Private : VeyraBackendProtocol::EPartyPrivacy::Public;
	Model.PrivacyLabel = bPublic ? LOCTEXT("MakePrivate", "Make Party Private") : LOCTEXT("MakePublic", "Make Party Public");
	Model.bCanSetPrivacy = bLeader && Permissions.bCanSetPrivacy;
	Model.bCanLeave = Permissions.bCanLeave;
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
	Model.Mode = ModeNameOf(Found.Mode);
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
	if (!Snapshot.Result.IsSet())
	{
		// Pending, not fabricated (UX-15, UX-17).
		Model.Headline = LOCTEXT("ResultPending", "Result Not Available Yet");
		Model.Lines.Add(LOCTEXT("ResultPendingDetail", "Veyra's services have not confirmed how this match ended."));
	}
	else
	{
		Model = DescribeOutcome(*Snapshot.Result);
	}
	if (const FText Notice = DescribeNotice(Snapshot.Notice); !Notice.IsEmpty())
	{
		Model.Lines.Add(Notice);
	}
	return Model;
}

FVeyraResultsModel DescribeOutcome(const VeyraBackendProtocol::FMatchOutcome& InOutcome)
{
	FVeyraResultsModel Model;
	const VeyraBackendProtocol::FMatchOutcome* Outcome = &InOutcome;
	if (!Outcome->bHasResult)
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
		// A personal loss for absence is the player's own outcome, apart from its team's (Match Flow
		// Bible §6; UX-51).
		if (Outcome->bPersonalLoss)
		{
			const FText Team = Outcome->Winner.IsEmpty() ? LOCTEXT("TeamNoContest", "no contest")
				: Outcome->Winner == Outcome->Side ? LOCTEXT("TeamWon", "won") : LOCTEXT("TeamLost", "lost");
			Model.Lines.Insert(FText::Format(LOCTEXT("ResultPersonalLoss", "Personal loss: you were away too long. Your team {0}."), Team), 0);
			Model.Headline = LOCTEXT("ResultPersonalDefeat", "Defeat");
		}
		Model.Lines.Add(FText::Format(LOCTEXT("ResultMode", "Mode: {0}"), ModeNameOf(Outcome->Mode)));
		if (!Outcome->VanguardId.IsEmpty())
		{
			Model.Lines.Add(FText::Format(LOCTEXT("ResultVanguard", "Your Vanguard: {0}"), VanguardNameOf(Outcome->VanguardId)));
		}
		Model.Lines.Add(FText::Format(LOCTEXT("ResultDuration", "Duration: {0}"), FormatCountdown(Outcome->DurationSeconds)));
		Model.Report = VeyraMatchReportModel::Describe(*Outcome);
	}
	return Model;
}

FText SideName(const FString& Side)
{
	return FText::Format(LOCTEXT("SideName", "Side {0}"), FText::FromString(Side));
}

FText DifficultyName(const FString& Difficulty)
{
	if (Difficulty == TEXT("beginner"))
	{
		return LOCTEXT("DifficultyBeginner", "Beginner");
	}
	if (Difficulty == TEXT("intermediate"))
	{
		return LOCTEXT("DifficultyIntermediate", "Intermediate");
	}
	return NameOf(Difficulty);
}

namespace
{
	/** "Side B, Seat 2": a seat, from its side and its index from 0. */
	FText SeatName(const FString& Side, int32 Index)
	{
		return FText::Format(LOCTEXT("SeatName", "{0}, Seat {1}"), SideName(Side), FText::AsNumber(Index + 1));
	}

	const TCHAR* OtherSide(const FString& Side)
	{
		return Side == TEXT("A") ? TEXT("B") : TEXT("A");
	}
}

FText AddBotLabel(const FString& Side, int32 Index)
{
	return FText::Format(LOCTEXT("AddBotLabel", "Add Bot: {0}"), SeatName(Side, Index));
}

FText ChangeBotLabel(const FString& Side, int32 Index)
{
	return FText::Format(LOCTEXT("ChangeBotLabel", "Change Bot: {0}"), SeatName(Side, Index));
}

FText RemoveBotLabel(const FString& Side, int32 Index)
{
	return FText::Format(LOCTEXT("RemoveBotLabel", "Remove Bot: {0}"), SeatName(Side, Index));
}

FText KickLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("KickLabel", "Remove {0}"), FText::FromString(Name));
}

FText SwitchSideLabel(const FString& Name, const FString& ToSide)
{
	return FText::Format(LOCTEXT("SwitchSideLabel", "Move {0} to {1}"), FText::FromString(Name), SideName(ToSide));
}

FText InviteLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("InviteLabel", "Invite {0}"), FText::FromString(Name));
}

FText AcceptRequestLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("AcceptRequestLabel", "Accept {0}"), FText::FromString(Name));
}

FText DeclineRequestLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("DeclineRequestLabel", "Decline {0}"), FText::FromString(Name));
}

FText JoinLobbyLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("JoinLobbyLabel", "Join {0}'s Lobby"), FText::FromString(Name));
}

FText DeclineInviteLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("DeclineInviteLabel", "Decline {0}'s Invite"), FText::FromString(Name));
}

FText PartyInviteLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("PartyInviteLabel", "Invite {0} to Party"), FText::FromString(Name));
}

FText AcceptPartyInviteLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("AcceptPartyInviteLabel", "Accept {0}'s Party Invite"), FText::FromString(Name));
}

FText DeclinePartyInviteLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("DeclinePartyInviteLabel", "Decline {0}'s Party Invite"), FText::FromString(Name));
}

FText JoinPartyLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("JoinPartyLabel", "Join {0}'s Party"), FText::FromString(Name));
}

FText FriendCardLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("FriendCardLabel", "Friend {0}"), FText::FromString(Name));
}

FText RemoveFriendLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("RemoveFriendLabel", "Remove Friend {0}"), FText::FromString(Name));
}

FText BlockLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("BlockLabel", "Block {0}"), FText::FromString(Name));
}

FText ConfirmBlockLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("ConfirmBlockLabel", "Confirm Block {0}"), FText::FromString(Name));
}

FText ConfirmBlockPrompt(const FString& Name)
{
	// What a block does, said before it is done (Parties & Social Bible §6).
	return FText::Format(LOCTEXT("ConfirmBlockPrompt", "Block {0}? You stop being friends, and cannot invite or party with each other."),
		FText::FromString(Name));
}

FText UnblockLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("UnblockLabel", "Unblock {0}"), FText::FromString(Name));
}

FText CancelRequestLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("CancelRequestLabel", "Cancel Request to {0}"), FText::FromString(Name));
}

FText PartyMemberLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("PartyMemberLabel", "Party Member {0}"), FText::FromString(Name));
}

FText MakeLeaderLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("MakeLeaderLabel", "Make {0} Party Leader"), FText::FromString(Name));
}

FText ConfirmLeaderLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("ConfirmLeaderLabel", "Confirm {0} as Party Leader"), FText::FromString(Name));
}

FText ConfirmLeaderPrompt(const FString& Name)
{
	// The confirmation names the recipient (UX-11).
	return FText::Format(LOCTEXT("ConfirmLeaderPrompt", "Make {0} the party leader?"), FText::FromString(Name));
}

FText RemoveFromPartyLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("RemoveFromPartyLabel", "Remove {0} from Party"), FText::FromString(Name));
}

FText CancelConfirmLabel()
{
	return LOCTEXT("CancelConfirmLabel", "Cancel Confirmation");
}

FText LeavePartyLabel()
{
	return LOCTEXT("LeavePartyLabel", "Leave Party");
}

FText StartingGoldLabel(TOptional<double> Gold)
{
	return Gold.IsSet() ? FText::Format(LOCTEXT("GoldChoice", "{0} Gold"), FText::AsNumber(FMath::RoundToInt64(Gold.GetValue())))
						: LOCTEXT("GoldDefault", "Default Gold");
}

FText BotChoiceLabel(const FString& VanguardId)
{
	return FText::Format(LOCTEXT("BotChoiceLabel", "Bot: {0}"), VanguardNameOf(VanguardId));
}

FVeyraLobbyModel DescribeLobby(const FVeyraClientSnapshot& Snapshot, bool bHosts, bool bCanStart, bool bCanLeave)
{
	using VeyraBackendProtocol::ELobbySeatKind;
	FVeyraLobbyModel Model;
	if (!Snapshot.Lobby.IsSet())
	{
		return Model;
	}
	const VeyraBackendProtocol::FLobby& Lobby = *Snapshot.Lobby;
	Model.bHost = Lobby.HostAccountId == Snapshot.AccountId;
	const VeyraBackendProtocol::FLobbySeat* Host = Lobby.FindMember(Lobby.HostAccountId);
	Model.Title = FText::Format(LOCTEXT("LobbyTitle", "{0}'s Lobby"), FText::FromString(Host ? Host->DisplayName : FString()));
	const auto FirstEmpty = [&Lobby](const FString& Side) {
		return Lobby.Seats.FindByPredicate([&Side](const VeyraBackendProtocol::FLobbySeat& Seat) { return Seat.Side == Side && Seat.Kind == ELobbySeatKind::Empty; });
	};
	const auto Occupied = [&Lobby](const FString& Side) {
		return Lobby.Seats.ContainsByPredicate([&Side](const VeyraBackendProtocol::FLobbySeat& Seat) { return Seat.Side == Side && Seat.Kind != ELobbySeatKind::Empty; });
	};
	for (const VeyraBackendProtocol::FLobbySeat& Seat : Lobby.Seats)
	{
		FVeyraLobbySeatModel SeatModel;
		SeatModel.Side = Seat.Side;
		SeatModel.Index = Seat.Index;
		SeatModel.Kind = Seat.Kind;
		switch (Seat.Kind)
		{
		case ELobbySeatKind::Human:
		{
			SeatModel.AccountId = Seat.AccountId;
			SeatModel.PlayerName = Seat.DisplayName;
			SeatModel.bYou = Seat.AccountId == Snapshot.AccountId;
			SeatModel.Name = SeatModel.bYou ? FText::Format(LOCTEXT("LobbyYou", "{0} (you)"), FText::FromString(Seat.DisplayName)) : FText::FromString(Seat.DisplayName);
			SeatModel.Detail = Seat.bHost ? LOCTEXT("LobbyHost", "Host") : LOCTEXT("LobbyPlayer", "Player");
			SeatModel.bCanKick = bHosts && !SeatModel.bYou;
			// The host places every human (§1): across to the other side's first empty seat.
			if (const VeyraBackendProtocol::FLobbySeat* Empty = FirstEmpty(OtherSide(Seat.Side)))
			{
				SeatModel.bCanSwitchSide = bHosts;
				SeatModel.SwitchToSide = Empty->Side;
				SeatModel.SwitchToIndex = Empty->Index;
			}
			break;
		}
		case ELobbySeatKind::Bot:
			SeatModel.VanguardId = Seat.VanguardId;
			SeatModel.Difficulty = Seat.Difficulty;
			SeatModel.Name = VanguardNameOf(Seat.VanguardId);
			SeatModel.Detail = FText::Format(LOCTEXT("LobbyBot", "{0} Bot"), DifficultyName(Seat.Difficulty));
			SeatModel.bCanSetBot = bHosts;
			SeatModel.bCanRemoveBot = bHosts;
			break;
		case ELobbySeatKind::Empty:
			SeatModel.Name = LOCTEXT("LobbyEmpty", "Empty");
			SeatModel.bCanSetBot = bHosts && !Lobby.BotVanguards.IsEmpty() && !Lobby.BotDifficulties.IsEmpty();
			break;
		}
		(Seat.Side == TEXT("A") ? Model.SideA : Model.SideB).Add(MoveTemp(SeatModel));
	}
	// Victory needs a Vanguard on each side: a side with none could never lose (§1).
	Model.bVictoryEnabled = Lobby.bVictoryEnabled;
	Model.Victory = Lobby.bVictoryEnabled ? LOCTEXT("LobbyVictoryOn", "Victory: on. Destroying a Prime Well wins the match.")
										  : LOCTEXT("LobbyVictoryOff", "Victory: off. The match runs until the host ends it.");
	Model.bCanToggleVictory = bHosts && (Lobby.bVictoryEnabled || (Occupied(TEXT("A")) && Occupied(TEXT("B"))));
	Model.StartingGold = Lobby.StartingGold.IsSet()
		? FText::Format(LOCTEXT("LobbyGold", "Starting Gold: {0}."), FText::AsNumber(FMath::RoundToInt64(Lobby.StartingGold.GetValue())))
		: LOCTEXT("LobbyGoldDefault", "Starting Gold: the game's own.");
	Model.bCanSetGold = bHosts;
	Model.GoldChoices.Add(FVeyraGoldChoiceModel{ {}, StartingGoldLabel({}), !Lobby.StartingGold.IsSet() });
	for (const float Choice : GetDefault<UVeyraShellStyleSettings>()->LobbyStartingGoldChoices)
	{
		// Only what the lobby's range allows is offered.
		if (Choice >= Lobby.StartingGoldMin && Choice <= Lobby.StartingGoldMax)
		{
			const bool bChosen = Lobby.StartingGold.IsSet() && FMath::IsNearlyEqual(Lobby.StartingGold.GetValue(), static_cast<double>(Choice));
			Model.GoldChoices.Add(FVeyraGoldChoiceModel{ static_cast<double>(Choice), StartingGoldLabel(static_cast<double>(Choice)), bChosen });
		}
	}
	Model.bCanStart = bCanStart;
	Model.bCanLeave = bCanLeave;
	Model.Status = Lobby.bSelecting ? LOCTEXT("LobbySelecting", "Champion select is opening.")
		: Model.bHost				? LOCTEXT("LobbyHostStarts", "Seat your players and bots, then start the game.")
									: LOCTEXT("LobbyGuestWaits", "Waiting for the host to start the game.");
	return Model;
}

FVeyraBotPickerModel DescribeBotPicker(const FVeyraClientSnapshot& Snapshot, const FString& Side, int32 Index)
{
	using VeyraBackendProtocol::ELobbySeatKind;
	FVeyraBotPickerModel Model;
	if (!Snapshot.Lobby.IsSet())
	{
		return Model;
	}
	const VeyraBackendProtocol::FLobby& Lobby = *Snapshot.Lobby;
	Model.Title = FText::Format(LOCTEXT("BotPickerTitle", "Bot for {0}"), SeatName(Side, Index));
	const VeyraBackendProtocol::FLobbySeat* This =
		Lobby.Seats.FindByPredicate([&Side, Index](const VeyraBackendProtocol::FLobbySeat& Seat) { return Seat.Side == Side && Seat.Index == Index; });
	// A side plays each Vanguard once, bots included (ADR-021 §8).
	for (const FString& Vanguard : Lobby.BotVanguards)
	{
		const bool bTaken = Lobby.Seats.ContainsByPredicate([&Side, Index, &Vanguard](const VeyraBackendProtocol::FLobbySeat& Seat) {
			return Seat.Side == Side && Seat.Index != Index && Seat.Kind == ELobbySeatKind::Bot && Seat.VanguardId == Vanguard;
		});
		const bool bChosen = This && This->Kind == ELobbySeatKind::Bot && This->VanguardId == Vanguard;
		Model.Vanguards.Add(FVeyraBotChoiceModel{ Vanguard, VanguardNameOf(Vanguard), bTaken, bChosen });
	}
	for (const FString& Difficulty : Lobby.BotDifficulties)
	{
		Model.Difficulties.Add({ Difficulty, DifficultyName(Difficulty) });
	}
	return Model;
}

FText DescribeSocialFeedback(const FString& Code, const FString& Name)
{
	const FText Who = FText::FromString(Name);
	if (Code.IsEmpty())
	{
		return FText::GetEmpty();
	}
	if (Code == TEXT("friend_requested"))
	{
		return FText::Format(LOCTEXT("SocialRequested", "Friend request sent to {0}."), Who);
	}
	if (Code == TEXT("friend_added"))
	{
		return FText::Format(LOCTEXT("SocialAdded", "You and {0} are now friends."), Who);
	}
	if (Code == TEXT("lobby_invited"))
	{
		return FText::Format(LOCTEXT("SocialInvited", "Invited {0} to your lobby."), Who);
	}
	if (Code == TEXT("account_not_found"))
	{
		return FText::Format(LOCTEXT("SocialNotFound", "No player is named {0}."), Who);
	}
	if (Code == TEXT("already_friends"))
	{
		return FText::Format(LOCTEXT("SocialAlreadyFriends", "You and {0} are already friends."), Who);
	}
	if (Code == TEXT("cannot_target_self"))
	{
		return LOCTEXT("SocialSelf", "That is you.");
	}
	if (Code == TEXT("blocked") || Code == TEXT("not_friends"))
	{
		// A block is never revealed (Parties & Social Bible §6).
		return FText::Format(LOCTEXT("SocialUnavailable", "{0} cannot be asked."), Who);
	}
	if (Code == TEXT("invite_not_found"))
	{
		return FText::Format(LOCTEXT("SocialInviteGone", "{0}'s invitation has expired."), Who);
	}
	if (Code == TEXT("lobby_full"))
	{
		return FText::Format(LOCTEXT("SocialLobbyFull", "{0}'s lobby is full."), Who);
	}
	if (Code == TEXT("lobby_locked"))
	{
		return FText::Format(LOCTEXT("SocialLobbyStarted", "{0}'s lobby has already started."), Who);
	}
	if (Code == TEXT("member_busy"))
	{
		return FText::Format(LOCTEXT("SocialBusy", "{0} is in a match, champion select or queue."), Who);
	}
	if (Code == TEXT("already_in_lobby"))
	{
		return FText::Format(LOCTEXT("SocialInLobby", "{0} is already in a lobby."), Who);
	}
	if (Code == TEXT("friend_request_not_found"))
	{
		return FText::Format(LOCTEXT("SocialRequestGone", "{0}'s friend request is gone."), Who);
	}
	if (Code == TEXT("party_invited"))
	{
		return FText::Format(LOCTEXT("SocialPartyInvited", "Invited {0} to your party."), Who);
	}
	if (Code == TEXT("player_blocked"))
	{
		return FText::Format(LOCTEXT("SocialBlocked", "You blocked {0}."), Who);
	}
	if (Code == TEXT("player_unblocked"))
	{
		return FText::Format(LOCTEXT("SocialUnblocked", "You unblocked {0}."), Who);
	}
	if (Code == TEXT("friend_request_cancelled"))
	{
		return FText::Format(LOCTEXT("SocialRequestCancelled", "Withdrew your friend request to {0}."), Who);
	}
	if (Code == TEXT("party_full"))
	{
		return FText::Format(LOCTEXT("SocialPartyFull", "{0}'s party is full."), Who);
	}
	if (Code == TEXT("party_locked"))
	{
		return FText::Format(LOCTEXT("SocialPartyLocked", "{0}'s party is in matchmaking."), Who);
	}
	if (Code == TEXT("party_not_joinable") || Code == TEXT("party_not_found"))
	{
		return FText::Format(LOCTEXT("SocialPartyClosed", "{0}'s party is no longer open to join."), Who);
	}
	if (Code == TEXT("already_in_party"))
	{
		return FText::Format(LOCTEXT("SocialAlreadyInParty", "You and {0} are already in a party together."), Who);
	}
	return FText::Format(LOCTEXT("SocialOther", "That did not work ({0})."), FText::FromString(Code));
}

FVeyraFriendsModel DescribeFriends(const FVeyraClientSnapshot& Snapshot, bool bCanAdd, bool bCanAnswerRequests, bool bCanJoin, bool bCanAnswerInvitations,
	bool bCanInvite, const FVeyraSocialPermissions& Permissions)
{
	const FVeyraSocial& Social = Snapshot.Social;
	FVeyraFriendsModel Model;
	Model.bLoaded = Social.bLoaded;
	Model.bCanAdd = bCanAdd;
	Model.Feedback = DescribeSocialFeedback(Social.Feedback, Social.FeedbackName);
	for (const VeyraBackendProtocol::FLobbyInvite& Invite : Social.LobbyInvites)
	{
		const FText Name = FText::FromString(Invite.Inviter.DisplayName);
		Model.Invitations.Add(FVeyraSocialRequestModel{ Invite.Id, Name, FText::Format(LOCTEXT("InviteLine", "{0} invites you to a custom game."), Name) });
	}
	Model.bCanJoin = bCanJoin;
	Model.bCanAnswerInvitations = bCanAnswerInvitations;
	for (const VeyraBackendProtocol::FAccount& From : Social.Friends.Incoming)
	{
		const FText Name = FText::FromString(From.DisplayName);
		Model.Requests.Add(FVeyraSocialRequestModel{ From.Id, Name, FText::Format(LOCTEXT("RequestLine", "{0} wants to be friends."), Name) });
	}
	Model.bCanAnswerRequests = bCanAnswerRequests;
	Model.bCanBlockRequests = Permissions.bCanBlock;
	for (const VeyraBackendProtocol::FPartyInvite& Invite : Social.PartyInvites)
	{
		const FText Name = FText::FromString(Invite.Inviter.DisplayName);
		Model.PartyInvitations.Add(
			FVeyraSocialRequestModel{ Invite.Id, Name, FText::Format(LOCTEXT("PartyInviteLine", "{0} invites you to their party."), Name) });
	}
	Model.bCanJoinPartyInvitations = Permissions.bCanAcceptPartyInvite;
	Model.bCanDeclinePartyInvitations = Permissions.bCanDeclinePartyInvite;
	// In the lobby its host invites friends who are not in it yet (Custom Matches Bible §1); in the shell any
	// member invites friends into the party, or joins a friend's Public party (ADR-044 §1, §3).
	const bool bInLobby = Snapshot.State == EVeyraClientState::Lobby && Snapshot.Lobby.IsSet();
	const bool bInShell = Snapshot.State == EVeyraClientState::Shell;
	for (const VeyraBackendProtocol::FAccount& Friend : Social.Friends.Friends)
	{
		FVeyraFriendModel FriendModel;
		FriendModel.AccountId = Friend.Id;
		FriendModel.Name = FText::FromString(Friend.DisplayName);
		FriendModel.bOffersInvite = bInLobby && Snapshot.Lobby->HostAccountId == Snapshot.AccountId;
		FriendModel.bCanInvite = FriendModel.bOffersInvite && bCanInvite && !Snapshot.Lobby->FindMember(Friend.Id);
		FriendModel.bOffersPartyInvite = bInShell && !(Snapshot.Party.IsSet() && Snapshot.Party->Find(Friend.Id));
		FriendModel.bCanPartyInvite = FriendModel.bOffersPartyInvite && Permissions.bCanInviteToParty;
		FriendModel.bOffersJoinParty = bInShell && Social.Friends.JoinablePartyOf(Friend.Id) != nullptr;
		FriendModel.bCanJoinParty = FriendModel.bOffersJoinParty && Permissions.bCanJoinFriendParty;
		FriendModel.bCanRemove = Permissions.bCanRemoveFriend;
		FriendModel.bCanBlock = Permissions.bCanBlock;
		FriendModel.bCanMessage = Permissions.bCanMessage;
		if (const FVeyraChatConversation* Conversation = Snapshot.Chat.Direct.Find(Friend.Id))
		{
			FriendModel.Unread = Conversation->Unread;
		}
		Model.Friends.Add(MoveTemp(FriendModel));
	}
	for (const VeyraBackendProtocol::FAccount& To : Social.Friends.Outgoing)
	{
		const FText Name = FText::FromString(To.DisplayName);
		Model.Pending.Add(FVeyraSocialRequestModel{ To.Id, Name, FText::Format(LOCTEXT("PendingLine", "{0}: request sent"), Name) });
	}
	Model.bCanCancelRequests = Permissions.bCanCancelRequest;
	for (const VeyraBackendProtocol::FAccount& Player : Social.Blocked)
	{
		const FText Name = FText::FromString(Player.DisplayName);
		Model.Blocked.Add(FVeyraSocialRequestModel{ Player.Id, Name, Name });
	}
	Model.bCanUnblock = Permissions.bCanUnblock;
	return Model;
}

FString PlayersTurn(const FVeyraClientSnapshot& Snapshot)
{
	const VeyraBackendProtocol::FSelect& Select = Snapshot.Select;
	if (Snapshot.State != EVeyraClientState::Selecting || !Select.Turn.IsSet() || !(Select.YouBan() || Select.YouMayLock()))
	{
		return FString();
	}
	// The turn's kind and side, and how far the draft had gone as it began: a teammate acting in the same
	// turn changes none of them.
	const VeyraBackendProtocol::FSelectTurn& Turn = *Select.Turn;
	const int32 Locked = Algo::CountIf(Select.Seats, [](const FSelectSeat& Seat) { return !Seat.Locked.IsEmpty(); });
	return FString::Printf(TEXT("%s:%s:%s:%d:%d"), *Select.Id, Turn.bBan ? TEXT("ban") : TEXT("pick"), *Turn.Side, Select.Bans.Num() - (Turn.bBan ? Turn.Done : 0),
		Locked - (Turn.bBan ? 0 : Turn.Done));
}

FString Signature(const FVeyraClientSnapshot& Snapshot)
{
	TStringBuilder<1024> Text;
	Text << LexToString(Snapshot.State) << TEXT("|") << Snapshot.DisplayName << TEXT("|") << (Snapshot.bBusy ? TEXT("busy") : TEXT("idle")) << TEXT("|") << Snapshot.Notice;
	// Chat changes what the sidebar, champion select and the results show (ADR-046 §6).
	Text << TEXT("|") << VeyraChatModels::Signature(Snapshot.Chat);
	if (Snapshot.Problem.IsSet())
	{
		Text << TEXT("|problem:") << Snapshot.Problem->Code << TEXT(":") << Snapshot.Problem->Message << (Snapshot.Problem->bCanRetry ? TEXT(":retry") : TEXT(""));
	}
	if (Snapshot.bSettingsConflict)
	{
		Text << TEXT("|settings conflict");
	}
	Text << TEXT("|starters:") << FString::Join(Snapshot.Starters, TEXT(",")) << TEXT("|available:") << FString::Join(Snapshot.AvailableVanguards, TEXT(","))
		 << TEXT("|released:") << FString::Join(Snapshot.ReleasedVanguards, TEXT(","));
	if (Snapshot.State == EVeyraClientState::Selecting)
	{
		const VeyraBackendProtocol::FSelect& Select = Snapshot.Select;
		Text << TEXT("|select:") << Select.Id << TEXT(":") << static_cast<int32>(Select.State) << TEXT(":") << static_cast<int32>(Select.Phase);
		// A draft's turn and bans (ADR-042 §1): a turn passes without any seat's pick changing.
		if (Select.Turn.IsSet())
		{
			Text << TEXT(":turn ") << (Select.Turn->bBan ? TEXT("ban ") : TEXT("pick ")) << Select.Turn->Side << Select.Turn->Count << TEXT("/") << Select.Turn->Done;
		}
		for (const VeyraBackendProtocol::FSelectBan& Ban : Select.Bans)
		{
			Text << TEXT(":ban ") << Ban.Side << Ban.VanguardId;
		}
		for (const FSelectSeat& Seat : Select.Seats)
		{
			Text << TEXT(";") << Seat.DisplayName << TEXT(":") << Seat.Hover << TEXT(":") << Seat.Locked << TEXT(":") << FString::Join(Seat.FluxSpells, TEXT(","))
				 << TEXT(":") << Seat.BanHover << (Seat.bActing ? TEXT(":acting") : TEXT("")) << (Seat.bOffersYou ? TEXT(":offers") : TEXT(""))
				 << (Seat.bOfferedByYou ? TEXT(":offered") : TEXT(""));
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
		Text << TEXT("|party:") << Party.Id << TEXT(":") << Party.Mode << TEXT(":") << static_cast<int32>(Party.Status) << TEXT(":")
			 << static_cast<int32>(Party.Privacy);
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
	const FVeyraMatchHistory& History = Snapshot.History;
	Text << TEXT("|history:") << History.Filter.VanguardId << TEXT(":") << History.Filter.Mode << TEXT(":") << History.Filter.Outcome << TEXT(":")
		 << (History.bLoaded ? TEXT("loaded") : TEXT("unread")) << TEXT(":") << History.Next << TEXT(":") << (History.Opened.IsSet() ? *History.Opened->MatchId : TEXT(""));
	for (const VeyraBackendProtocol::FHistoryEntry& Entry : History.Entries)
	{
		Text << TEXT(";") << Entry.MatchId;
	}
	if (Snapshot.Result.IsSet())
	{
		const VeyraBackendProtocol::FMatchOutcome& Outcome = *Snapshot.Result;
		Text << TEXT("|result:") << Outcome.State << TEXT(":") << Outcome.EndReason << TEXT(":") << Outcome.FailureReason << TEXT(":") << Outcome.Players.Num()
			 << TEXT(":") << Outcome.Wells.Num();
	}
	if (Snapshot.State == EVeyraClientState::Lobby && Snapshot.Lobby.IsSet())
	{
		const VeyraBackendProtocol::FLobby& Lobby = *Snapshot.Lobby;
		Text << TEXT("|lobby:") << Lobby.Id << TEXT(":") << Lobby.HostAccountId << TEXT(":") << (Lobby.bSelecting ? TEXT("selecting") : TEXT("open")) << TEXT(":")
			 << (Lobby.bVictoryEnabled ? TEXT("victory") : TEXT("sandbox")) << TEXT(":") << (Lobby.StartingGold.IsSet() ? FString::SanitizeFloat(*Lobby.StartingGold) : FString());
		for (const VeyraBackendProtocol::FLobbySeat& Seat : Lobby.Seats)
		{
			Text << TEXT(";") << static_cast<int32>(Seat.Kind) << Seat.AccountId << Seat.DisplayName << Seat.VanguardId << Seat.Difficulty;
		}
	}
	// The friends panel shows in the shell and the lobby.
	const FVeyraSocial& Social = Snapshot.Social;
	Text << TEXT("|social:") << (Social.bLoaded ? TEXT("read") : TEXT("unread")) << TEXT(":") << Social.Feedback << TEXT(":") << Social.FeedbackName;
	for (const TArray<VeyraBackendProtocol::FAccount>* List : { &Social.Friends.Friends, &Social.Friends.Incoming, &Social.Friends.Outgoing })
	{
		Text << TEXT(";");
		for (const VeyraBackendProtocol::FAccount& Account : *List)
		{
			Text << Account.Id << TEXT(",");
		}
	}
	for (const VeyraBackendProtocol::FLobbyInvite& Invite : Social.LobbyInvites)
	{
		Text << TEXT(";invite:") << Invite.Id;
	}
	for (const VeyraBackendProtocol::FPartyInvite& Invite : Social.PartyInvites)
	{
		Text << TEXT(";party invite:") << Invite.Id;
	}
	for (const VeyraBackendProtocol::FJoinableParty& Joinable : Social.Friends.JoinableParties)
	{
		Text << TEXT(";joinable:") << Joinable.AccountId << TEXT(":") << Joinable.PartyId;
	}
	for (const VeyraBackendProtocol::FAccount& Player : Social.Blocked)
	{
		Text << TEXT(";blocked:") << Player.Id;
	}
	// The account's level and balances, the Collection and a result's rewards (ADR-045 §8).
	Text << VeyraProgressionModels::Signature(Snapshot);
	return FString(Text.ToString());
}
}

#undef LOCTEXT_NAMESPACE
