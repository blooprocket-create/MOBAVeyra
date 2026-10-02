// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Blueprint/UserWidget.h"
#include "Components/ActorTestSpawner.h"
#include "GameFramework/PlayerState.h"
#include "Match/VeyraMatchMenu.h"
#include "Engine/Texture2D.h"
#include "Shell/VeyraShellArt.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellModels.h"
#include "Shell/VeyraShellScreen.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "Shell/VeyraUIInputSettings.h"
#include "Tests/Services/VeyraClientFlowTestRig.h"
#include "Slots/VeyraAbilitySlot.h"
#include "Text/VeyraContentText.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraFluxTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "UObject/Package.h"
#include "VeyraGameState.h"
#include "VeyraPlayerController.h"

namespace VeyraShellTests
{
	using namespace VeyraClientFlowTests;

	/** The labels of Buttons, in order. */
	TArray<FString> LabelsOf(const TArray<UVeyraShellButton*>& Buttons)
	{
		TArray<FString> Labels;
		for (const UVeyraShellButton* Button : Buttons)
		{
			Labels.Add(Button->GetLabel().ToString());
		}
		return Labels;
	}

	/** A select as its only player sees it, hovering or locking a Vanguard. */
	FVeyraClientSnapshot SelectSnapshot(const FString& Hover, const FString& Locked)
	{
		FVeyraClientSnapshot Snapshot;
		Snapshot.State = EVeyraClientState::Selecting;
		Snapshot.Select.Id = SelectId;
		Snapshot.Select.Mode = TEXT("custom_practice");
		Snapshot.Select.Seats.Add(VeyraBackendProtocol::FSelectSeat{ TEXT("DevOne"), TEXT("A"), true, Hover, Locked });
		Snapshot.AvailableVanguards = { TEXT("cairn"), TEXT("oriel") };
		return Snapshot;
	}

	// Veyra.UI.Shell.*: what the shell's screens show, from the coordinator's snapshot (ADR-010 §4).
	TEST_CLASS(Shell, "Veyra.UI")
	{
		TEST_METHOD(ScreenForState)
		{
			const TPair<EVeyraClientState, EVeyraShellScreen> Expected[] = {
				{ EVeyraClientState::SigningIn, EVeyraShellScreen::Status },
				{ EVeyraClientState::SignInFailed, EVeyraShellScreen::Stopped },
				{ EVeyraClientState::Loading, EVeyraShellScreen::Status },
				{ EVeyraClientState::StarterChoice, EVeyraShellScreen::StarterChoice },
				{ EVeyraClientState::Shell, EVeyraShellScreen::Shell },
				{ EVeyraClientState::Lobby, EVeyraShellScreen::Lobby },
				{ EVeyraClientState::MatchFound, EVeyraShellScreen::MatchFound },
				{ EVeyraClientState::Selecting, EVeyraShellScreen::ChampionSelect },
				{ EVeyraClientState::MatchStarting, EVeyraShellScreen::Status },
				{ EVeyraClientState::Connecting, EVeyraShellScreen::Status },
				{ EVeyraClientState::InMatch, EVeyraShellScreen::None },
				{ EVeyraClientState::Returning, EVeyraShellScreen::Status },
				{ EVeyraClientState::AwaitingResults, EVeyraShellScreen::Status },
				{ EVeyraClientState::Results, EVeyraShellScreen::Results },
				{ EVeyraClientState::ReconnectOnly, EVeyraShellScreen::ReconnectOnly },
				{ EVeyraClientState::SessionEnded, EVeyraShellScreen::Stopped },
			};
			ASSERT_THAT(AreEqual(static_cast<int32>(EVeyraClientState::SessionEnded) + 1, static_cast<int32>(UE_ARRAY_COUNT(Expected)), TEXT("every state has a screen")));
			for (const TPair<EVeyraClientState, EVeyraShellScreen>& Pair : Expected)
			{
				ASSERT_THAT(IsTrue(VeyraShellModels::ScreenFor(Pair.Key) == Pair.Value, LexToString(Pair.Key)));
			}
		}

		TEST_METHOD(ChampionSelectOffersBothSpellSlotsAndSumsUpTheSetup)
		{
			const TArray<FVeyraContentId>& Roster = UVeyraAbilitiesTuningSubsystem::Get().FluxSpells.Roster;
			const TArray<double>& Thresholds = UVeyraFluxTuningSubsystem::Get().SpellSlots.Thresholds;
			ASSERT_THAT(IsTrue(Roster.Num() >= 2 && Thresholds.Num() == 2));
			FVeyraClientSnapshot Snapshot = SelectSnapshot(TEXT("oriel"), FString());
			Snapshot.Select.Seats[0].FluxSpells = { Roster[0].ToString(), FString() };
			FVeyraSelectModel Model = VeyraShellModels::DescribeSelect(Snapshot, 20.0, true, true, false, true);
			ASSERT_THAT(IsTrue(Model.bCanChooseSpells && Model.SpellSlots.Num() == 2));
			// Each slot shows its own permanent-Flux threshold (UX 36).
			ASSERT_THAT(IsTrue(Model.SpellSlots[0].Unlock.ToString().Contains(FText::AsNumber(Thresholds[0]).ToString())));
			ASSERT_THAT(IsTrue(Model.SpellSlots[1].Unlock.ToString().Contains(FText::AsNumber(Thresholds[1]).ToString())));
			ASSERT_THAT(AreEqual(Model.SpellSlots[0].Chosen.ToString(), VeyraContentText::AbilityName(Roster[0]).ToString()));
			ASSERT_THAT(AreEqual(Model.SpellSlots[1].Chosen.ToString(), FString(TEXT("Empty"))));
			// None, then every roster spell, the chosen one marked.
			ASSERT_THAT(AreEqual(Model.SpellSlots[0].Choices.Num(), Roster.Num() + 1));
			ASSERT_THAT(IsTrue(!Model.SpellSlots[0].Choices[0].bChosen && Model.SpellSlots[0].Choices[1].bChosen));
			ASSERT_THAT(IsTrue(Model.SpellSlots[1].Choices[0].bChosen, TEXT("an empty slot has None chosen")));
			ASSERT_THAT(IsTrue(Model.Setup.IsEmpty(), TEXT("Your Match Setup comes with lock-in")));
			// Beside the player's portrait.
			ASSERT_THAT(AreEqual(Model.Seats[0].Spells.Num(), 2));
			ASSERT_THAT(IsTrue(Model.Seats[0].Spells[0].ToString() == VeyraContentText::AbilityName(Roster[0]).ToString() && Model.Seats[0].Spells[1].IsEmpty()));

			Snapshot.Select.Seats[0].Locked = TEXT("oriel");
			Model = VeyraShellModels::DescribeSelect(Snapshot, 10.0, false, false, false, true);
			ASSERT_THAT(IsTrue(Model.bCanChooseSpells, TEXT("spells stay open after lock-in")));
			ASSERT_THAT(IsTrue(Model.Setup.ToString().Contains(TEXT("Oriel")) && Model.Setup.ToString().Contains(VeyraContentText::AbilityName(Roster[0]).ToString()),
				Model.Setup.ToString()));
		}

		TEST_METHOD(SelectModel)
		{
			// A hover is tentative: Not Locked In, and Lock In would lock it (UX-35).
			FVeyraSelectModel Model = VeyraShellModels::DescribeSelect(SelectSnapshot(TEXT("oriel"), FString()), 27.2, true, true, false);
			ASSERT_THAT(AreEqual(Model.Countdown.ToString(), FString(TEXT("0:28"))));
			ASSERT_THAT(AreEqual(Model.Title.ToString(), FString(TEXT("Custom Practice: Champion Select"))));
			ASSERT_THAT(AreEqual(Model.Seats.Num(), 1));
			ASSERT_THAT(IsTrue(Model.Seats[0].Status == EVeyraSeatStatus::NotLockedIn && Model.Seats[0].bYou));
			ASSERT_THAT(AreEqual(Model.Seats[0].StatusText.ToString(), FString(TEXT("Not Locked In"))));
			ASSERT_THAT(AreEqual(Model.Seats[0].Vanguard.ToString(), FString(TEXT("Oriel"))));
			ASSERT_THAT(AreEqual(Model.Cards.Num(), 2));
			ASSERT_THAT(IsTrue(!Model.Cards[0].bChosen && Model.Cards[1].bChosen));
			ASSERT_THAT(AreEqual(Model.LockInVanguardId, FString(TEXT("oriel"))));
			ASSERT_THAT(IsTrue(Model.bCanLockIn));
			// The large art shows the hover (UX 27), with its name, title and kit: the passive, then Q, W, E, R.
			ASSERT_THAT(AreEqual(Model.ShownVanguardId, FString(TEXT("oriel"))));
			ASSERT_THAT(AreEqual(Model.ShownName.ToString(), FString(TEXT("Oriel"))));
			ASSERT_THAT(AreEqual(Model.ShownTitle.ToString(), VeyraContentText::VanguardTitle(*FVeyraContentId::FromText(TEXT("oriel"))).ToString()));
			ASSERT_THAT(AreEqual(Model.Abilities.Num(), 5));
			ASSERT_THAT(IsTrue(Model.Abilities[0].Key.ToString() == TEXT("Passive") && Model.Abilities[1].Key.ToString() == TEXT("Q")
				&& Model.Abilities[4].Key.ToString() == TEXT("R")));
			ASSERT_THAT(IsFalse(Model.Abilities[1].Name.IsEmpty() || Model.Abilities[1].Description.IsEmpty()));
			ASSERT_THAT(AreEqual(Model.ModeLabel.ToString(), FString(TEXT("CUSTOM PRACTICE"))));
			ASSERT_THAT(AreEqual(Model.Seats[0].VanguardId, FString(TEXT("oriel"))));

			// With nothing hovered there is nothing to lock, and no art to show.
			FVeyraClientSnapshot Nothing = SelectSnapshot(FString(), FString());
			Nothing.Select.PickSeconds = 30.0;
			Model = VeyraShellModels::DescribeSelect(Nothing, 30.0, true, true, false);
			ASSERT_THAT(IsTrue(Model.Seats[0].Status == EVeyraSeatStatus::Waiting && !Model.bCanLockIn));
			ASSERT_THAT(IsTrue(Model.ShownVanguardId.IsEmpty() && Model.Abilities.IsEmpty()));
			ASSERT_THAT(IsTrue(Model.PickSeconds == 30.0, TEXT("the timer's full length, for its bars")));

			// A lock is final.
			Model = VeyraShellModels::DescribeSelect(SelectSnapshot(TEXT("oriel"), TEXT("oriel")), 12.0, false, false, false);
			ASSERT_THAT(IsTrue(Model.Seats[0].Status == EVeyraSeatStatus::LockedIn));
			ASSERT_THAT(IsTrue(Model.LockInVanguardId.IsEmpty() && !Model.bCanLockIn && !Model.bCanChoose));
			ASSERT_THAT(AreEqual(VeyraShellModels::FormatCountdown(-3.0).ToString(), FString(TEXT("0:00"))));
			ASSERT_THAT(AreEqual(VeyraShellModels::FormatCountdown(75.0).ToString(), FString(TEXT("1:15"))));
		}

		TEST_METHOD(TeamSelectModel)
		{
			// A matchmade select: the enemy team apart, its locks visible and unique, and Leave offered.
			FVeyraClientSnapshot Snapshot = SelectSnapshot(TEXT("oriel"), FString());
			Snapshot.Select.Kind = TEXT("casual");
			Snapshot.Select.Mode = TEXT("casual_select");
			Snapshot.Select.Seats.Add(VeyraBackendProtocol::FSelectSeat{ TEXT("DevTwo"), TEXT("B"), false, FString(), TEXT("cairn") });
			FVeyraSelectModel Model = VeyraShellModels::DescribeSelect(Snapshot, 50.0, true, true, true);
			ASSERT_THAT(IsTrue(Model.bTeams));
			ASSERT_THAT(IsTrue(Model.Seats[0].bAlly && !Model.Seats[1].bAlly));
			ASSERT_THAT(AreEqual(Model.Seats[1].StatusText.ToString(), FString(TEXT("Locked In"))));
			ASSERT_THAT(AreEqual(Model.Seats[1].Vanguard.ToString(), FString(TEXT("Cairn"))));
			ASSERT_THAT(IsTrue(Model.Cards[0].VanguardId == TEXT("cairn") && Model.Cards[0].bTaken, TEXT("another player locked Cairn")));
			ASSERT_THAT(IsFalse(Model.Cards[1].bTaken));
			ASSERT_THAT(IsTrue(Model.bCanLockIn, TEXT("the player's own hover is free")));
			ASSERT_THAT(IsTrue(Model.bOffersLeave && Model.bCanLeave));

			// A hover on a Vanguard someone else locks cannot be locked in.
			Snapshot.Select.Seats[0].Hover = TEXT("cairn");
			Model = VeyraShellModels::DescribeSelect(Snapshot, 49.0, true, true, true);
			ASSERT_THAT(IsFalse(Model.bCanLockIn));

			// Once every pick is in, there is nothing left to leave.
			Snapshot.Select.State = VeyraBackendProtocol::ESelectState::Starting;
			ASSERT_THAT(IsFalse(VeyraShellModels::DescribeSelect(Snapshot, 0.0, false, false, false).bOffersLeave));

			// Practice has one team, and no Leave.
			Model = VeyraShellModels::DescribeSelect(SelectSnapshot(FString(), FString()), 30.0, true, true, false);
			ASSERT_THAT(IsFalse(Model.bTeams || Model.bOffersLeave));
		}

		TEST_METHOD(DraftSelectModel)
		{
			// The player's ban turn in a draft (ADR-041 §1): the cards are every released Vanguard, the ban hover is chosen,
			// and the tile bans.
			FVeyraClientSnapshot Snapshot = SelectSnapshot(TEXT("oriel"), FString());
			Snapshot.Select.Kind = TEXT("draft");
			Snapshot.Select.Mode = TEXT("draft_pick");
			Snapshot.Select.Phase = VeyraBackendProtocol::ESelectPhase::Banning;
			Snapshot.Select.Turn = VeyraBackendProtocol::FSelectTurn{ true, TEXT("A"), 1, 0 };
			Snapshot.Select.Seats[0].bActing = true;
			Snapshot.Select.Seats[0].BanHover = TEXT("bryn");
			Snapshot.Select.Seats.Add(VeyraBackendProtocol::FSelectSeat{ TEXT("DevThree"), TEXT("A"), false, FString(), FString() });
			Snapshot.Select.Seats.Add(VeyraBackendProtocol::FSelectSeat{ TEXT("DevTwo"), TEXT("B"), false, FString(), FString() });
			Snapshot.Select.Bans.Add(VeyraBackendProtocol::FSelectBan{ TEXT("B"), TEXT("qazharr") });
			Snapshot.ReleasedVanguards = { TEXT("cairn"), TEXT("qazharr"), TEXT("oriel"), TEXT("bryn") };
			FVeyraSelectDraftPermissions Draft;
			Draft.bCanBan = true;
			FVeyraSelectModel Model = VeyraShellModels::DescribeSelect(Snapshot, 20.0, true, false, true, true, Draft);
			ASSERT_THAT(IsTrue(Model.bDraft && Model.bBanning && Model.bCanChoose));
			ASSERT_THAT(AreEqual(Model.Phase.ToString(), FString(TEXT("Your turn to ban."))));
			ASSERT_THAT(AreEqual(Model.Cards.Num(), 4));
			ASSERT_THAT(IsTrue(Model.Cards[1].bBanned && Model.Cards[1].bTaken, TEXT("Qazharr is banned already")));
			ASSERT_THAT(IsTrue(Model.Cards[3].bChosen && Model.BanVanguardId == TEXT("bryn") && Model.bCanBan));
			ASSERT_THAT(IsTrue(Model.Bans.Num() == 1 && !Model.Bans[0].bAlly && Model.Bans[0].Name.ToString() == TEXT("Qazharr")));
			ASSERT_THAT(IsTrue(Model.Seats[0].bActing && Model.Seats[0].SeatIndex == 0 && Model.Seats[2].SeatIndex == 2));
			ASSERT_THAT(AreEqual(Model.Seats[0].BanHover.ToString(), FString(TEXT("Banning Bryn"))));
			ASSERT_THAT(IsTrue(Model.bOffersLeave, TEXT("a draft may be left, as a dodge")));

			// The player's turn is one key through the turn, a teammate's ban in it included; none outside it.
			const FString Turn = VeyraShellModels::PlayersTurn(Snapshot);
			ASSERT_THAT(IsFalse(Turn.IsEmpty()));
			FVeyraClientSnapshot Teammate = Snapshot;
			Teammate.Select.Turn = VeyraBackendProtocol::FSelectTurn{ true, TEXT("A"), 2, 1 };
			Teammate.Select.Bans.Add(VeyraBackendProtocol::FSelectBan{ TEXT("A"), TEXT("cairn") });
			ASSERT_THAT(AreEqual(VeyraShellModels::PlayersTurn(Teammate), VeyraShellModels::PlayersTurn(
				[&Snapshot] { FVeyraClientSnapshot Before = Snapshot; Before.Select.Turn = VeyraBackendProtocol::FSelectTurn{ true, TEXT("A"), 2, 0 }; return Before; }())));

			// The enemy's ban turn says so; the bench is the player's own again, with the bans taken.
			Snapshot.Select.Turn = VeyraBackendProtocol::FSelectTurn{ true, TEXT("B"), 1, 0 };
			Snapshot.Select.Seats[0].bActing = false;
			Snapshot.Select.Seats[2].bActing = true;
			Model = VeyraShellModels::DescribeSelect(Snapshot, 20.0, true, false, true, true, FVeyraSelectDraftPermissions());
			ASSERT_THAT(IsTrue(!Model.bBanning && !Model.bCanBan));
			ASSERT_THAT(AreEqual(Model.Phase.ToString(), FString(TEXT("The enemy team is banning."))));
			ASSERT_THAT(AreEqual(Model.Cards.Num(), 2));
			ASSERT_THAT(IsTrue(VeyraShellModels::PlayersTurn(Snapshot).IsEmpty(), TEXT("the enemy's turn is not the player's")));

			// The final window: locked teammates trade. An offer to DevThree stands; DevTwo is an enemy.
			Snapshot.Select.Phase = VeyraBackendProtocol::ESelectPhase::Final;
			Snapshot.Select.Turn.Reset();
			Snapshot.Select.Seats[2].bActing = false;
			for (VeyraBackendProtocol::FSelectSeat& Seat : Snapshot.Select.Seats)
			{
				Seat.Locked = Seat.bYou ? TEXT("oriel") : TEXT("cairn");
			}
			Draft = FVeyraSelectDraftPermissions();
			Draft.bCanOfferTrade = true;
			Model = VeyraShellModels::DescribeSelect(Snapshot, 8.0, false, false, true, true, Draft);
			ASSERT_THAT(AreEqual(Model.Phase.ToString(), FString(TEXT("Everyone is locked in. Locked teammates may trade until the match starts."))));
			ASSERT_THAT(IsTrue(!Model.Seats[0].bCanOfferTrade && Model.Seats[1].bCanOfferTrade && !Model.Seats[2].bCanOfferTrade));
			Snapshot.Select.Seats[1].bOfferedByYou = true;
			Model = VeyraShellModels::DescribeSelect(Snapshot, 8.0, false, false, true, true, Draft);
			ASSERT_THAT(IsTrue(Model.Seats[1].bOfferedByYou && !Model.Seats[1].bCanOfferTrade, TEXT("one offer is enough")));
			Snapshot.Select.Seats[1].bOfferedByYou = false;
			Snapshot.Select.Seats[1].bOffersYou = true;
			Draft.bCanAnswerTrade = true;
			Model = VeyraShellModels::DescribeSelect(Snapshot, 8.0, false, false, true, true, Draft);
			ASSERT_THAT(IsTrue(Model.Seats[1].bOffersYou && Model.Seats[1].bCanAnswerTrade));
		}

		TEST_METHOD(MatchFoundModel)
		{
			FVeyraClientSnapshot Snapshot;
			Snapshot.State = EVeyraClientState::MatchFound;
			Snapshot.MatchFound.Id = FoundId;
			Snapshot.MatchFound.Mode = CasualMode;
			Snapshot.MatchFound.State = TEXT("pending");
			Snapshot.MatchFound.You = TEXT("pending");
			Snapshot.MatchFound.Accepted = 1;
			Snapshot.MatchFound.Total = 2;
			FVeyraMatchFoundModel Model = VeyraShellModels::DescribeMatchFound(Snapshot, 14.2, true);
			ASSERT_THAT(AreEqual(Model.Title.ToString(), FString(TEXT("Match Found"))));
			ASSERT_THAT(AreEqual(Model.Mode.ToString(), FString(TEXT("Blind Pick"))));
			ASSERT_THAT(AreEqual(Model.Countdown.ToString(), FString(TEXT("0:15"))));
			ASSERT_THAT(AreEqual(Model.Progress.ToString(), FString(TEXT("1 of 2 accepted"))));
			ASSERT_THAT(IsTrue(Model.Phase.ToString().StartsWith(TEXT("Accept to play")) && Model.bCanAnswer));

			Snapshot.MatchFound.You = TEXT("accepted");
			Model = VeyraShellModels::DescribeMatchFound(Snapshot, 9.0, false);
			ASSERT_THAT(AreEqual(Model.Phase.ToString(), FString(TEXT("Accepted. Waiting for the other players."))));
			ASSERT_THAT(IsFalse(Model.bCanAnswer));

			// How a match found ended reads by the player's own answer; nobody learns who declined.
			ASSERT_THAT(IsTrue(VeyraShellModels::DescribeNotice(TEXT("match_found_declined")).ToString().Contains(TEXT("Your party left the queue"))));
			ASSERT_THAT(IsTrue(VeyraShellModels::DescribeNotice(TEXT("match_found_missed")).ToString().Contains(TEXT("not accepted in time"))));
			ASSERT_THAT(IsTrue(VeyraShellModels::DescribeNotice(TEXT("match_found_abandoned")).ToString().StartsWith(TEXT("Someone in your party did not accept"))));
			ASSERT_THAT(IsTrue(VeyraShellModels::DescribeNotice(TEXT("match_found_requeued")).ToString().Contains(TEXT("You are back in the queue"))));
			// A block between two players ends a match found or a select, and nobody is told of it (§6).
			ASSERT_THAT(IsFalse(VeyraShellModels::DescribeNotice(TEXT("match_found_requeued")).ToString().Contains(TEXT("player"))));
			ASSERT_THAT(IsTrue(VeyraShellModels::DescribeNotice(TEXT("no_longer_matched")).ToString().Contains(TEXT("can no longer go ahead"))));
			ASSERT_THAT(AreEqual(VeyraShellModels::DescribeProblem(FVeyraClientProblem{ TEXT("member_busy"), TEXT("raw"), false }).ToString(),
				FString(TEXT("Someone is still in a match, champion select or queue."))));
		}

		TEST_METHOD(PartyAndModeModels)
		{
			FVeyraClientSnapshot Snapshot;
			Snapshot.State = EVeyraClientState::Shell;
			Snapshot.AccountId = AccountId;
			ASSERT_THAT(IsFalse(VeyraShellModels::DescribeParty(Snapshot, false, false, false).bShown, TEXT("no party, no panel")));

			// Only enabled modes show; one without a matchmaker says so (UX-12).
			FString Problem;
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseModes(ModesBody, Snapshot.Modes, Problem), Problem));
			Snapshot.Modes.Add(VeyraBackendProtocol::FModeInfo{ TEXT("ranked"), false, 5, false });
			TOptional<VeyraBackendProtocol::FParty> Party;
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseParty(PartyBody(TEXT("idle"), false), Party, Problem), Problem));
			Snapshot.Party = Party;
			const TArray<FVeyraModeCardModel> Cards = VeyraShellModels::DescribeModes(Snapshot);
			ASSERT_THAT(AreEqual(Cards.Num(), 2));
			ASSERT_THAT(IsTrue(Cards[0].bAvailable && Cards[0].bSelected && Cards[0].Availability.IsEmpty()));
			ASSERT_THAT(AreEqual(Cards[0].Format.ToString(), FString(TEXT("1v1"))));
			ASSERT_THAT(IsTrue(!Cards[1].bAvailable && !Cards[1].bSelected));
			ASSERT_THAT(AreEqual(Cards[1].Availability.ToString(), FString(TEXT("Not yet available"))));

			// A co-op queue plays its humans against AI, and takes its name from the text table (ADR-039 §6).
			Snapshot.Modes.Add(VeyraBackendProtocol::FModeInfo{ TEXT("coop_beginner"), true, 1, true, true });
			const TArray<FVeyraModeCardModel> WithCoop = VeyraShellModels::DescribeModes(Snapshot);
			ASSERT_THAT(AreEqual(WithCoop.Num(), 3));
			ASSERT_THAT(IsTrue(WithCoop[2].bAvailable && WithCoop[2].Availability.IsEmpty()));
			ASSERT_THAT(AreEqual(WithCoop[2].Format.ToString(), FString(TEXT("1 vs AI"))));
			ASSERT_THAT(AreEqual(WithCoop[2].Name.ToString(), FString(TEXT("Co-op vs AI: Beginner"))));
			Snapshot.Modes.Pop();

			// The leader of a party that is not Ready yet.
			FVeyraPartyModel Model = VeyraShellModels::DescribeParty(Snapshot, true, false, false);
			ASSERT_THAT(IsTrue(Model.bShown && !Model.bQueued));
			ASSERT_THAT(AreEqual(Model.Mode.ToString(), FString(TEXT("Mode: Blind Pick"))));
			ASSERT_THAT(AreEqual(Model.Members[0].ToString(), FString(TEXT("DevOne (you, leader): Not Ready"))));
			ASSERT_THAT(AreEqual(Model.Status.ToString(), FString(TEXT("Find Match opens once everyone is Ready."))));
			ASSERT_THAT(IsTrue(Model.bReadyTarget && Model.bCanReady));
			ASSERT_THAT(AreEqual(Model.ReadyLabel.ToString(), FString(TEXT("Ready"))));
			ASSERT_THAT(IsTrue(Model.bOffersFindMatch && !Model.bCanFindMatch && !Model.bOffersCancel));

			// Queued: the time shows instead, only the leader may cancel, and Ready is locked (§2).
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseParty(PartyBody(TEXT("queued"), true, 3.0), Party, Problem), Problem));
			Snapshot.Party = Party;
			Model = VeyraShellModels::DescribeParty(Snapshot, false, false, true);
			ASSERT_THAT(IsTrue(Model.bQueued && Model.Status.IsEmpty()));
			ASSERT_THAT(IsTrue(!Model.bOffersFindMatch && Model.bOffersCancel && Model.bCanCancel));
			ASSERT_THAT(AreEqual(Model.ReadyLabel.ToString(), FString(TEXT("Unready"))));
			ASSERT_THAT(AreEqual(VeyraShellModels::FormatQueueStatus(67.9).ToString(), FString(TEXT("In queue: 1:07. Estimate unavailable."))));

			// A member who does not lead sees the leader, and neither Find Match nor Cancel.
			Snapshot.AccountId = TEXT("66666666-7777-4888-8999-aaaaaaaaaaaa");
			Model = VeyraShellModels::DescribeParty(Snapshot, false, false, false);
			ASSERT_THAT(AreEqual(Model.Members[0].ToString(), FString(TEXT("DevOne (leader): Ready"))));
			ASSERT_THAT(IsFalse(Model.bOffersFindMatch || Model.bOffersCancel));
		}

		TEST_METHOD(ResultsModel)
		{
			FVeyraClientSnapshot Snapshot;
			Snapshot.State = EVeyraClientState::Results;
			FVeyraResultsModel Model = VeyraShellModels::DescribeResults(Snapshot);
			ASSERT_THAT(IsFalse(Model.bVerified, TEXT("no result is shown as pending, never made up")));

			VeyraBackendProtocol::FMatchOutcome Outcome;
			Outcome.MatchId = MatchId;
			Outcome.Mode = TEXT("custom_practice");
			Outcome.State = TEXT("ended");
			Outcome.VanguardId = TEXT("oriel");
			Outcome.bHasResult = true;
			Outcome.EndReason = TEXT("host_ended");
			Outcome.DurationSeconds = 42.5;
			Snapshot.Result = Outcome;
			Model = VeyraShellModels::DescribeResults(Snapshot);
			ASSERT_THAT(IsTrue(Model.bVerified));
			ASSERT_THAT(AreEqual(Model.Headline.ToString(), FString(TEXT("Match Over: No Winner"))));
			const FString Lines = FText::Join(FText::FromString(TEXT("|")), Model.Lines).ToString();
			ASSERT_THAT(IsTrue(Lines.Contains(TEXT("The host ended the match.")) && Lines.Contains(TEXT("Your Vanguard: Oriel"))
				&& Lines.Contains(TEXT("Mode: Custom Practice")) && Lines.Contains(TEXT("Duration: 0:43")), Lines));

			Outcome.bHasResult = false;
			Outcome.State = TEXT("failed");
			Outcome.FailureReason = TEXT("server_exited");
			Snapshot.Result = Outcome;
			Snapshot.Notice = TEXT("connection_lost");
			Model = VeyraShellModels::DescribeResults(Snapshot);
			ASSERT_THAT(AreEqual(Model.Headline.ToString(), FString(TEXT("The Match Did Not Finish"))));
			ASSERT_THAT(AreEqual(Model.Lines.Num(), 2, TEXT("why it failed, and why the player left it")));
		}

		TEST_METHOD(ResultsModelVictory)
		{
			VeyraBackendProtocol::FMatchOutcome Outcome;
			Outcome.MatchId = MatchId;
			Outcome.Mode = TEXT("casual_select");
			Outcome.State = TEXT("ended");
			Outcome.Side = TEXT("A");
			Outcome.bHasResult = true;
			Outcome.EndReason = TEXT("prime_well_destroyed");
			Outcome.Winner = TEXT("A");
			FVeyraClientSnapshot Snapshot;
			Snapshot.State = EVeyraClientState::Results;
			Snapshot.Result = Outcome;
			FVeyraResultsModel Model = VeyraShellModels::DescribeResults(Snapshot);
			ASSERT_THAT(AreEqual(Model.Headline.ToString(), FString(TEXT("Victory"))));
			ASSERT_THAT(IsTrue(FText::Join(FText::FromString(TEXT("|")), Model.Lines).ToString().Contains(TEXT("A Prime Well was destroyed."))));

			// A personal loss for absence is the player's own Defeat, beside its team's win (UX-51).
			Outcome.bPersonalLoss = true;
			Snapshot.Result = Outcome;
			Model = VeyraShellModels::DescribeResults(Snapshot);
			ASSERT_THAT(AreEqual(Model.Headline.ToString(), FString(TEXT("Defeat"))));
			ASSERT_THAT(IsTrue(Model.Lines[0].ToString().Contains(TEXT("Personal loss")) && Model.Lines[0].ToString().Contains(TEXT("Your team won"))));
			Outcome.bPersonalLoss = false;

			Outcome.Side = TEXT("B");
			Snapshot.Result = Outcome;
			Model = VeyraShellModels::DescribeResults(Snapshot);
			ASSERT_THAT(AreEqual(Model.Headline.ToString(), FString(TEXT("Defeat"))));
		}

		TEST_METHOD(AMatchReportIsTheScoreboardTheTeamSummaryAndDetailedStatistics)
		{
			VeyraBackendProtocol::FMatchOutcome Outcome;
			FString Problem;
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseMatchOutcome(ScoredOutcomeBody(), Outcome, Problem), Problem));
			// Seen from side B, whose players lost.
			Outcome.Side = TEXT("B");
			const FVeyraMatchReport Report = VeyraMatchReportModel::Describe(Outcome);
			ASSERT_THAT(IsTrue(Report.bHasScoreboard && Report.Teams.Num() == 2));
			const FVeyraReportTeam& Ours = Report.Teams[0];
			ASSERT_THAT(AreEqual(Ours.Title.ToString(), FString(TEXT("Your Team: Defeat"))));
			ASSERT_THAT(IsTrue(Ours.Lines.Num() == 2 && Ours.Lines[0].Vanguard.ToString() == TEXT("Oriel") && Ours.Lines[1].Name.ToString() == TEXT("Bot 1")));
			// The team's kills and Gold add up; its Flux Wells count each capture once (UX-53).
			ASSERT_THAT(AreEqual(Ours.Summary.ToString(), FString(TEXT("1 kills, 10,643 Gold earned, 1 Flux Wells secured"))));
			const FVeyraReportLine& Winner = Report.Teams[1].Lines[0];
			ASSERT_THAT(AreEqual(Report.Teams[1].Title.ToString(), FString(TEXT("Enemy Team: Victory"))));
			ASSERT_THAT(IsTrue(Winner.bYou && Winner.Name.ToString() == TEXT("DevOne (you)") && Winner.Kda.ToString() == TEXT("3 / 1 / 2")));
			ASSERT_THAT(IsTrue(Winner.Gold.ToString() == TEXT("5,321 Gold") && Winner.LastHits.ToString() == TEXT("80 minions, 4 monsters"), Winner.Gold.ToString()));
			ASSERT_THAT(IsTrue(Winner.Items.ToString().StartsWith(TEXT("Timing Coil | -")) && Winner.FluxSpells.ToString() == TEXT("Flux Spells: Blink, Mend"),
				Winner.Items.ToString() + TEXT(" / ") + Winner.FluxSpells.ToString()));

			// Detailed Statistics: a column for each player in the scoreboard's order, grouped as UX-50 approves.
			ASSERT_THAT(AreEqual(Report.Columns.Num(), 3));
			TArray<FString> Groups;
			for (const FVeyraReportGroup& Group : Report.Groups)
			{
				Groups.Add(Group.Title.ToString());
			}
			ASSERT_THAT(IsTrue(Groups == (TArray<FString>{ TEXT("Combat"), TEXT("Objectives"), TEXT("Economy"), TEXT("Vision") })));
			const FVeyraReportRow* Stun = Report.Groups[0].Rows.FindByPredicate([](const FVeyraReportRow& Row) { return Row.Label.ToString() == TEXT("Stun on Enemy Vanguards"); });
			ASSERT_THAT(IsTrue(Stun && Stun->Values.Num() == 3 && Stun->Values[1].ToString() == TEXT("1.3 s") && Stun->Values[2].ToString() == TEXT("2.5 s"),
				Stun ? FText::Join(FText::FromString(TEXT(",")), Stun->Values).ToString() : FString()));

			// Pending while unrecorded, and said plainly when never recorded (UX-50).
			Outcome.bHasScoreboard = false;
			ASSERT_THAT(IsTrue(!VeyraMatchReportModel::Describe(Outcome).bHasScoreboard && VeyraMatchReportModel::Describe(Outcome).Pending.ToString().Contains(TEXT("No statistics"))));
			Outcome.bHasResult = false;
			ASSERT_THAT(IsTrue(VeyraMatchReportModel::Describe(Outcome).Pending.ToString().Contains(TEXT("Pending"))));
		}

		TEST_METHOD(NamesAndTheSignature)
		{
			ASSERT_THAT(AreEqual(VeyraShellModels::NameOf(TEXT("custom_practice")).ToString(), FString(TEXT("Custom Practice"))));
			ASSERT_THAT(AreEqual(VeyraShellModels::NameOf(TEXT("cairn")).ToString(), FString(TEXT("Cairn"))));

			// A read that changes only the timer leaves the screen as it is.
			FVeyraClientSnapshot Snapshot = SelectSnapshot(FString(), FString());
			const FString Before = VeyraShellModels::Signature(Snapshot);
			Snapshot.PickEndsAt += 10.0;
			++Snapshot.Revision;
			ASSERT_THAT(AreEqual(VeyraShellModels::Signature(Snapshot), Before));
			Snapshot.Select.Seats[0].Hover = TEXT("cairn");
			ASSERT_THAT(IsFalse(VeyraShellModels::Signature(Snapshot) == Before));
		}

		TEST_METHOD(StyleSettings)
		{
			const TArray<FString> Problems = GetDefault<UVeyraShellStyleSettings>()->Validate();
			ASSERT_THAT(IsTrue(Problems.IsEmpty(), FString::Join(Problems, TEXT(" | "))));
			// A new object starts from the project's values, so these are cleared by hand.
			UVeyraShellStyleSettings* Missing = NewObject<UVeyraShellStyleSettings>(GetTransientPackage());
			Missing->BackgroundColor = FLinearColor::Transparent;
			Missing->MenuScrimColor = FLinearColor::Transparent;
			Missing->TitleFontSize = 0;
			Missing->CountdownFontSize = 0;
			Missing->CardWidth = 0.0f;
			Missing->MenuWidth = 0.0f;
			Missing->ShopHeight = 0.0f;
			Missing->FrameColor = FLinearColor::Transparent;
			Missing->SmallFontSize = 0;
			Missing->SplashWidth = 0.0f;
			Missing->VanguardArtFolder = TEXT("Veyra/UI/");
			Missing->DefaultPortrait.CropHeight = 1.5f;
			Missing->VanguardPortraits.Add(FVeyraVanguardPortrait{ TEXT("cairn"), FVector2D(0.5, 0.5), 0.5f });
			const FString Named = FString::Join(Missing->Validate(), TEXT(" | "));
			for (const TCHAR* Field : { TEXT("BackgroundColor"), TEXT("MenuScrimColor"), TEXT("TitleFontSize"), TEXT("CountdownFontSize"), TEXT("CardWidth"), TEXT("MenuWidth"),
					 TEXT("ShopHeight"), TEXT("FrameColor"), TEXT("SmallFontSize"), TEXT("SplashWidth"), TEXT("VanguardArtFolder"), TEXT("DefaultPortrait"),
					 TEXT("VanguardPortraits") })
			{
				ASSERT_THAT(IsTrue(Named.Contains(Field), FString::Printf(TEXT("%s is not named in: %s"), Field, *Named)));
			}
		}

		TEST_METHOD(PortraitsCropAroundTheFaceWithinTheIllustration)
		{
			const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
			const FVeyraVanguardPortrait* Oriel = Style.VanguardPortraits.FindByPredicate([](const FVeyraVanguardPortrait& Portrait) { return Portrait.Vanguard == TEXT("oriel"); });
			ASSERT_THAT(IsNotNull(Oriel));
			constexpr int32 Width = 1600;
			constexpr int32 Height = 900;
			// A square portrait: CropHeight of the height, around the face, pushed inside at the edges.
			const FBox2f Square = VeyraShellArt::Crop(TEXT("oriel"), Width, Height, 1.0f, /*bPortrait*/ true);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Square.GetSize().Y * Height, Oriel->CropHeight * Height, 0.5f)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Square.GetSize().X * Width, Square.GetSize().Y * Height, 0.5f), TEXT("square in pixels")));
			ASSERT_THAT(IsTrue(Square.Min.X >= 0.0f && Square.Min.Y >= 0.0f && Square.Max.X <= 1.0f && Square.Max.Y <= 1.0f));
			ASSERT_THAT(IsTrue(Square.IsInside(FVector2f(Oriel->Focus.X, Oriel->Focus.Y)), TEXT("the face is in it")));
			// A banner is as large as the illustration allows: the full width at 4:1.
			const FBox2f Banner = VeyraShellArt::Crop(TEXT("oriel"), Width, Height, 4.0f, /*bPortrait*/ false);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Banner.GetSize().X, 1.0f) && FMath::IsNearlyEqual(Banner.GetSize().Y * Height, Width / 4.0f, 0.5f)));
			// A Vanguard with no entry uses the default; no size shows the whole illustration.
			const FBox2f Default = VeyraShellArt::Crop(TEXT("test_vanguard"), Width, Height, 1.0f, true);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Default.GetSize().Y, Style.DefaultPortrait.CropHeight)));
			ASSERT_THAT(IsTrue(VeyraShellArt::Crop(TEXT("oriel"), 0, 0, 1.0f, true) == FBox2f(FVector2f::ZeroVector, FVector2f::UnitVector)));
			ASSERT_THAT(IsNull(VeyraShellArt::HeroOf(FString())));
			ASSERT_THAT(IsNull(VeyraShellArt::HeroOf(TEXT("test_vanguard")), TEXT("a developer Vanguard has no art")));
		}

		TEST_METHOD(InputSettings)
		{
			ASSERT_THAT(IsTrue(GetDefault<UVeyraUIInputSettings>()->Validate().IsEmpty()));
			UVeyraUIInputSettings* Missing = NewObject<UVeyraUIInputSettings>(GetTransientPackage());
			Missing->MatchMenuKey = FKey();
			ASSERT_THAT(IsFalse(Missing->Validate().IsEmpty()));
			UVeyraUIInputSettings* Shared = NewObject<UVeyraUIInputSettings>(GetTransientPackage());
			Shared->ShopKey = Shared->MatchMenuKey;
			ASSERT_THAT(IsTrue(FString::Join(Shared->Validate(), TEXT(" ")).Contains(TEXT("ShopKey")), TEXT("the shop needs its own key")));
		}
	};

	// Veyra.UI.ShellScreens.*: the screens built in C++, bound to a coordinator on a fake backend, and
	// clicked as the player would click them.
	TEST_CLASS(ShellScreens, "Veyra.UI")
	{
		FActorTestSpawner Spawner;
		/** Before the rig, which outlives none of what it was given. */
		FFakeAccountSettingsCache SettingsCache;
		FClientFlowTestRig Rig;
		UVeyraShellScreen* Screen = nullptr;

		AFTER_EACH()
		{
			if (Screen)
			{
				Screen->Unbind();
			}
		}

		/** A screen showing the rig's coordinator. */
		UVeyraShellScreen& ShowScreen()
		{
			Screen = CreateWidget<UVeyraShellScreen>(&Spawner.GetWorld());
			Screen->Bind(*Rig.Flow);
			return *Screen;
		}

		UVeyraShellButton* Button(const TCHAR* Label) const
		{
			return Screen->FindButton(FText::FromString(Label));
		}

		TEST_METHOD(TheStarterChoiceOffersEachStarter)
		{
			ASSERT_THAT(IsTrue(Rig.ReachStarterChoice()));
			ShowScreen();
			ASSERT_THAT(IsTrue(Screen->GetShownScreen() == EVeyraShellScreen::StarterChoice));
			ASSERT_THAT(IsTrue(LabelsOf(Screen->GetButtons()) == (TArray<FString>{ TEXT("Cairn"), TEXT("Qazharr"), TEXT("Oriel"), TEXT("Bryn") })));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("The tutorial is coming later"))));
			Button(TEXT("Oriel"))->Press();
			ASSERT_THAT(AreEqual(Rig.Backend.Find(TEXT("POST"), TEXT("/v1/me/starter"))->Body, FString(TEXT("{\"vanguardId\":\"oriel\"}"))));
			ASSERT_THAT(IsFalse(Button(TEXT("Cairn"))->GetIsEnabled(), TEXT("one choice at a time")));
		}

		TEST_METHOD(HomeLeadsToPlayAndPractice)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			ShowScreen();
			ASSERT_THAT(IsTrue(Screen->GetShownScreen() == EVeyraShellScreen::Shell && Screen->GetPage() == EVeyraShellPage::Home));
			ASSERT_THAT(IsTrue(Screen->IsFocusable(), TEXT("the shell's input mode gives the screen keyboard focus")));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Welcome, DevOne."))));
			Button(TEXT("Play"))->Press();
			ASSERT_THAT(IsTrue(Screen->GetPage() == EVeyraShellPage::Play));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Custom"))));
			Button(TEXT("Practice"))->Press();
			ASSERT_THAT(IsNotNull(Rig.Backend.Find(TEXT("POST"), TEXT("/v1/practice"))));
			ASSERT_THAT(IsFalse(Button(TEXT("Practice"))->GetIsEnabled(), TEXT("practice is starting")));
		}

		TEST_METHOD(ChampionSelectOwnsTheScreenAndLocksTheHover)
		{
			ASSERT_THAT(IsTrue(Rig.ReachSelect()));
			ShowScreen();
			ASSERT_THAT(IsTrue(Screen->GetShownScreen() == EVeyraShellScreen::ChampionSelect));
			// No navigation leaves a committed select (UX-4): the roster across the top, each Flux Spell slot's
			// tile, and Lock In.
			const TArray<FString> Expected = { TEXT("Cairn"), TEXT("Qazharr"), TEXT("Oriel"), TEXT("Bryn"), TEXT("Flux Spell 1"), TEXT("Flux Spell 2"), TEXT("Lock In") };
			ASSERT_THAT(IsTrue(LabelsOf(Screen->GetButtons()) == Expected, FString::Join(LabelsOf(Screen->GetButtons()), TEXT(", "))));
			ASSERT_THAT(IsFalse(Button(TEXT("Lock In"))->GetIsEnabled(), TEXT("nothing is hovered yet")));
			FString Text = Screen->DescribeText();
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("DevOne (you)")) && Text.Contains(TEXT("Waiting")) && Text.Contains(TEXT("Choose a Vanguard")), Text));
			ASSERT_THAT(IsNull(Screen->GetBackdrop(), TEXT("no Vanguard is shown yet")));

			Button(TEXT("Oriel"))->Press();
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("PUT"), TEXT("/v1/me/select/hover"), 200, SelectBody(TEXT("picking"), TEXT("oriel")))));
			Text = Screen->DescribeText();
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("Oriel")) && Text.Contains(TEXT("Not Locked In")), Text));
			ASSERT_THAT(IsTrue(Button(TEXT("Lock In"))->GetIsEnabled()));
			Button(TEXT("Lock In"))->Press();
			ASSERT_THAT(AreEqual(Rig.Backend.Find(TEXT("POST"), TEXT("/v1/me/select/lock"))->Body, FString(TEXT("{\"vanguardId\":\"oriel\"}"))));
		}

		TEST_METHOD(ADraftBansFromTheBenchInThePlayersTurn)
		{
			// The screen shows from Match Found on, as in a game, while the draft opens on the player's ban turn.
			ASSERT_THAT(IsTrue(Rig.ReachMatchFound()));
			ShowScreen();
			const TCHAR* const ABan = TEXT("{\"ban\":true,\"side\":\"A\",\"count\":1,\"done\":0}");
			const TCHAR* const Teammate = TEXT("\"hover\":null,\"locked\":null,\"acting\":false");
			ASSERT_THAT(IsTrue(Rig.Flow->AcceptMatch()));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("POST"), TEXT("/v1/me/match-found/accept"), 200,
				MatchFoundBody(TEXT("accepted"), TEXT("accepted"), 2, SelectId))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 200, NoMatch)));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200,
				DraftSelectBody(TEXT("banning"), ABan, TEXT("[]"), TEXT("\"hover\":null,\"locked\":null,\"acting\":true"), Teammate))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/vanguards"), 200, VanguardsBody)));
			ASSERT_THAT(IsTrue(Rig.Flow->CanIssue(EVeyraClientIntent::BanVanguard)));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Your turn to ban.")), Screen->DescribeText()));
			// The player's turn asked for their attention once, as it began (UX-31, UX-32).
			ASSERT_THAT(AreEqual(Screen->GetTurnAttentionCount(), 1));
			// Every released Vanguard is on the bench, Silt too, which the player does not own.
			UVeyraShellButton* Silt = Button(TEXT("Silt"));
			ASSERT_THAT(IsTrue(Silt && Silt->GetIsEnabled(), FString::Join(LabelsOf(Screen->GetButtons()), TEXT(", "))));
			ASSERT_THAT(IsFalse(Button(TEXT("Ban"))->GetIsEnabled(), TEXT("nothing is hovered yet")));
			Silt->Press();
			ASSERT_THAT(AreEqual(Rig.Backend.Find(TEXT("PUT"), TEXT("/v1/me/select/ban/hover"))->Body, FString(TEXT("{\"vanguardId\":\"silt\"}"))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("PUT"), TEXT("/v1/me/select/ban/hover"), 200,
				DraftSelectBody(TEXT("banning"), ABan, TEXT("[]"), TEXT("\"hover\":null,\"locked\":null,\"banHover\":\"silt\",\"acting\":true"), Teammate))));
			ASSERT_THAT(IsTrue(Button(TEXT("Ban"))->GetIsEnabled(), TEXT("the hovered ban can be banned")));
			ASSERT_THAT(AreEqual(Screen->GetTurnAttentionCount(), 1, TEXT("not again within the same turn")));
			Button(TEXT("Ban"))->Press();
			ASSERT_THAT(IsNotNull(Rig.Backend.Find(TEXT("POST"), TEXT("/v1/me/select/ban"))));

			// The next turn is the enemy's: a poll that changes only the turn and the bans rebuilds the screen.
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("POST"), TEXT("/v1/me/select/ban"), 200,
				DraftSelectBody(TEXT("banning"), TEXT("{\"ban\":true,\"side\":\"B\",\"count\":1,\"done\":0}"), TEXT("[{\"side\":\"A\",\"vanguardId\":\"silt\"}]"),
					TEXT("\"hover\":null,\"locked\":null,\"acting\":false"), Teammate))));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("The enemy team is banning.")) && Screen->DescribeText().Contains(TEXT("Bans: Silt")),
				Screen->DescribeText()));
			ASSERT_THAT(IsNull(Button(TEXT("Silt")), TEXT("the bench is the player's own again")));
			ASSERT_THAT(AreEqual(Screen->GetTurnAttentionCount(), 1, TEXT("the enemy's turn asks nothing of the player")));
		}

		TEST_METHOD(APollThatChangesNothingKeepsTheButtons)
		{
			ASSERT_THAT(IsTrue(Rig.ReachSelect()));
			ShowScreen();
			const UVeyraShellButton* Before = Button(TEXT("Oriel"));
			Rig.Advance(0.5);
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200, SelectBody(TEXT("picking"), FString(), FString(), FString(), FString(), 29.0))));
			ASSERT_THAT(IsTrue(Button(TEXT("Oriel")) == Before, TEXT("a click in progress is not interrupted")));
		}

		TEST_METHOD(ASpellSlotsTileOpensItsPickerAndAChoiceClosesIt)
		{
			ASSERT_THAT(IsTrue(Rig.ReachSelect()));
			ShowScreen();
			const TArray<FVeyraContentId>& Roster = UVeyraAbilitiesTuningSubsystem::Get().FluxSpells.Roster;
			ASSERT_THAT(IsTrue(Roster.Num() >= 2));
			ASSERT_THAT(IsNull(Button(*VeyraContentText::AbilityName(Roster[0]).ToString()), TEXT("the spells wait in their picker")));

			// The spell picker: None and every roster spell, each described, and the slot's threshold.
			Screen->FindButton(VeyraShellModels::SpellSlotTitle(1))->Press();
			ASSERT_THAT(AreEqual(Screen->GetOpenSpellSlot(), 1));
			TArray<FString> Expected = { TEXT("Cairn"), TEXT("Qazharr"), TEXT("Oriel"), TEXT("Bryn"), TEXT("Flux Spell 1"), TEXT("Flux Spell 2"), TEXT("Lock In"), TEXT("None") };
			for (const FVeyraContentId& Spell : Roster)
			{
				Expected.Add(VeyraContentText::AbilityName(Spell).ToString());
			}
			Expected.Add(TEXT("Close"));
			ASSERT_THAT(IsTrue(LabelsOf(Screen->GetButtons()) == Expected, FString::Join(LabelsOf(Screen->GetButtons()), TEXT(", "))));
			const FString Text = Screen->DescribeText();
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("75 permanent Team Flux")) && Text.Contains(VeyraContentText::AbilityDescription(Roster[1]).ToString()), Text));

			Button(*VeyraContentText::AbilityName(Roster[1]).ToString())->Press();
			ASSERT_THAT(AreEqual(Screen->GetOpenSpellSlot(), static_cast<int32>(INDEX_NONE), TEXT("a choice closes the picker")));
			const FString Chosen = FString::Printf(TEXT("[\"\",\"%s\"]"), *Roster[1].ToString());
			ASSERT_THAT(AreEqual(Rig.Backend.Find(TEXT("PUT"), TEXT("/v1/me/select/spells"))->Body, FString::Printf(TEXT("{\"fluxSpells\":%s}"), *Chosen)));
			// The tiles wait for the backend's answer, as every intent does.
			ASSERT_THAT(IsFalse(Screen->FindButton(VeyraShellModels::SpellSlotTitle(0))->GetIsEnabled()));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("PUT"), TEXT("/v1/me/select/spells"), 200,
				SelectBody(TEXT("picking"), FString(), FString(), FString(), FString(), 30.0, *Chosen))));

			// Close shuts it without choosing; the tile opens and shuts it too.
			UVeyraShellButton* Tile = Screen->FindButton(VeyraShellModels::SpellSlotTitle(0));
			ASSERT_THAT(IsTrue(Tile && Tile->GetIsEnabled()));
			Tile->Press();
			ASSERT_THAT(IsNotNull(Button(TEXT("Close"))));
			Button(TEXT("Close"))->Press();
			ASSERT_THAT(AreEqual(Screen->GetOpenSpellSlot(), static_cast<int32>(INDEX_NONE)));
			Screen->FindButton(VeyraShellModels::SpellSlotTitle(0))->Press();
			ASSERT_THAT(AreEqual(Screen->GetOpenSpellSlot(), 0));
			Screen->FindButton(VeyraShellModels::SpellSlotTitle(0))->Press();
			ASSERT_THAT(AreEqual(Screen->GetOpenSpellSlot(), static_cast<int32>(INDEX_NONE)));
		}

		TEST_METHOD(TheShownVanguardsArtFillsTheScreenAndItsAbilitiesLieOverIt)
		{
			ASSERT_THAT(IsTrue(Rig.ReachSelect()));
			ShowScreen();
			Button(TEXT("Oriel"))->Press();
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("PUT"), TEXT("/v1/me/select/hover"), 200, SelectBody(TEXT("picking"), TEXT("oriel")))));
			// The imported hero illustration (Game/Scripts/BuildVanguardArt.ps1) behind everything.
			UTexture2D* Hero = VeyraShellArt::HeroOf(TEXT("oriel"));
			ASSERT_THAT(IsNotNull(Hero, TEXT("every playable Vanguard's art is imported")));
			ASSERT_THAT(IsTrue(Screen->GetBackdrop() == Hero));

			Button(*UVeyraShellScreen::AbilitiesLabel(false).ToString())->Press();
			const FVeyraVanguardDefinition* Oriel = UVeyraVanguardsTuningSubsystem::FindVanguard(*FVeyraContentId::FromText(TEXT("oriel")));
			ASSERT_THAT(IsTrue(Oriel && !Oriel->Passive.IsEmpty() && !Oriel->Abilities.R.IsEmpty()));
			const FString Text = Screen->DescribeText();
			ASSERT_THAT(IsTrue(Text.Contains(VeyraContentText::PassiveName(Oriel->Passive[0]).ToString())
				&& Text.Contains(VeyraContentText::AbilityDescription(Oriel->Abilities.R[0]).ToString()), Text));
			ASSERT_THAT(IsNotNull(Button(*UVeyraShellScreen::AbilitiesLabel(true).ToString())));
		}

		TEST_METHOD(ReconnectOnlyOffersNothingButReconnect)
		{
			ASSERT_THAT(IsTrue(Rig.ReachReconnectOnly()));
			ShowScreen();
			ASSERT_THAT(IsTrue(Screen->GetShownScreen() == EVeyraShellScreen::ReconnectOnly));
			ASSERT_THAT(IsTrue(LabelsOf(Screen->GetButtons()) == TArray<FString>{ TEXT("Reconnect") }));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Match in Progress"))));
			Button(TEXT("Reconnect"))->Press();
			ASSERT_THAT(IsNotNull(Rig.Backend.Find(TEXT("GET"), TEXT("/v1/me/match"))));
		}

		TEST_METHOD(ResultsShowTheVerifiedResultThenContinue)
		{
			ASSERT_THAT(IsTrue(Rig.ReachResults()));
			ShowScreen();
			ASSERT_THAT(IsTrue(Screen->GetShownScreen() == EVeyraShellScreen::Results));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("The host ended the match.")), Screen->DescribeText()));
			Button(TEXT("Continue"))->Press();
			ASSERT_THAT(IsTrue(Rig.State() == EVeyraClientState::Loading));
			ASSERT_THAT(IsTrue(Screen->GetShownScreen() == EVeyraShellScreen::Status));
		}

		TEST_METHOD(ResultsShowTheScoreboardThenDetailedStatistics)
		{
			ASSERT_THAT(IsTrue(Rig.ReachResults(ScoredOutcomeBody())));
			ShowScreen();
			FString Text = Screen->DescribeText();
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("Victory")) && Text.Contains(TEXT("Your Team: Victory")) && Text.Contains(TEXT("Enemy Team: Defeat"))
				&& Text.Contains(TEXT("3 kills, 5,321 Gold earned, 1 Flux Wells secured")) && Text.Contains(TEXT("DevOne (you)")), Text));
			ASSERT_THAT(IsTrue(Screen->GetReportView() == EVeyraReportView::Scoreboard && Button(TEXT("Scoreboard")) && Button(TEXT("Detailed Statistics"))));
			Button(TEXT("Detailed Statistics"))->Press();
			Text = Screen->DescribeText();
			ASSERT_THAT(IsTrue(Screen->GetReportView() == EVeyraReportView::Details));
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("Combat")) && Text.Contains(TEXT("Objectives")) && Text.Contains(TEXT("Economy")) && Text.Contains(TEXT("Vision"))
				&& Text.Contains(TEXT("Damage to Towers")) && Text.Contains(TEXT("Gold from Minions")), Text));
			ASSERT_THAT(IsFalse(Text.Contains(TEXT("Your Team: Victory")), TEXT("one view at a time")));
			Button(TEXT("Continue"))->Press();
			ASSERT_THAT(IsTrue(Rig.State() == EVeyraClientState::Loading));
		}

		TEST_METHOD(MatchHistoryListsFiltersAndOpensARecord)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			ShowScreen();
			Button(TEXT("Match History"))->Press();
			ASSERT_THAT(IsTrue(Screen->GetPage() == EVeyraShellPage::History));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/matches"), 200,
				HistoryBody({ HistoryEntry(MatchId, TEXT("win")), HistoryEntry(OlderMatchId, TEXT("no_contest")) }, TEXT("\"more\"")))));
			FString Text = Screen->DescribeText();
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("Blind Pick")) && Text.Contains(TEXT("Victory")) && Text.Contains(TEXT("No Contest")) && Text.Contains(TEXT("25:11")), Text));
			ASSERT_THAT(IsNotNull(Button(TEXT("Load More"))));
			ASSERT_THAT(IsTrue(Button(TEXT("All Vanguards")) && Button(TEXT("Oriel")) && Button(TEXT("All Modes")) && Button(TEXT("Defeat")), TEXT("each filter's choices")));
			ASSERT_THAT(IsNotNull(Button(TEXT("Custom Practice")), TEXT("a mode with saved matches, though none is on the pages read")));

			// A filter reads the first page again (UX-64).
			Button(TEXT("Defeat"))->Press();
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/matches?outcome=loss"), 200, HistoryBody({}, TEXT("null")))));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("No completed matches fit these filters.")) && !Button(TEXT("Load More"))));
			Button(TEXT("All Outcomes"))->Press();
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/matches"), 200, HistoryBody({ HistoryEntry(MatchId, TEXT("win")) }, TEXT("null")))));

			// Opening one shows its saved Scoreboard and Detailed Statistics (UX-51).
			Button(TEXT("Open"))->Press();
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), FString(TEXT("/v1/me/matches/")) + MatchId, 200, ScoredOutcomeBody())));
			Text = Screen->DescribeText();
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("Your Team: Victory")) && Text.Contains(TEXT("DevOne (you)")), Text));
			Button(TEXT("Detailed Statistics"))->Press();
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Gold from Minions"))));
			Button(TEXT("Back to Match History"))->Press();
			ASSERT_THAT(IsTrue(Screen->GetPage() == EVeyraShellPage::History && Button(TEXT("Open")) != nullptr));
		}

		TEST_METHOD(ASettingsConflictAsksWhichToKeepOverTheScreen)
		{
			SettingsCache.Document.Revision = 3;
			SettingsCache.Change(TEXT("camera_move_speed"), TEXT("70"));
			Rig.Flow->SyncAccountSettings(SettingsCache);
			ASSERT_THAT(IsTrue(Rig.SignIn()));
			ShowScreen();
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/account/settings"), 200,
				TEXT("{\"schemaVersion\":1,\"revision\":5,\"values\":{\"camera_move_speed\":\"20\"}}"))));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Your settings changed on another device"))));
			UVeyraShellButton* ThisDevice = Screen->FindButton(UVeyraShellScreen::SettingsChoiceLabel(true));
			ASSERT_THAT(IsTrue(ThisDevice && Screen->FindButton(UVeyraShellScreen::SettingsChoiceLabel(false))));

			ThisDevice->Press();
			ASSERT_THAT(IsNotNull(Rig.Backend.Find(TEXT("PUT"), TEXT("/v1/account/settings")), TEXT("this device's settings go to the account")));
			ASSERT_THAT(IsNull(Screen->FindButton(UVeyraShellScreen::SettingsChoiceLabel(true)), TEXT("the choice is gone")));
			ASSERT_THAT(IsNotNull(Rig.Backend.Find(TEXT("GET"), TEXT("/v1/me/match")), TEXT("and the sign-in goes on")));
		}

		TEST_METHOD(AProblemOffersRetry)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("VeyraClientFlow: problem in Loading (backend_unreachable)"), ELogVerbosity::Warning,
				EAutomationExpectedMessageFlags::Contains, 1);
			ASSERT_THAT(IsTrue(Rig.SignIn()));
			ShowScreen();
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 0)));
			Rig.Advance(1.0);
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 0)));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Veyra's services did not answer."))));
			Button(TEXT("Retry"))->Press();
			ASSERT_THAT(IsNotNull(Rig.Backend.Find(TEXT("GET"), TEXT("/v1/me/match"))));
			ASSERT_THAT(IsNull(Button(TEXT("Retry"))));
		}

		TEST_METHOD(PlayQueuesTheParty)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/modes"), 200, ModesBody)));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/party"), 200, NoParty)));
			ShowScreen();
			Button(TEXT("Play"))->Press();
			// Mode cards: one to choose, one not yet available; no party panel before a mode is chosen. They show
			// in their categories, and Customs holds Practice and Custom Game (ADR-039 §6).
			ASSERT_THAT(IsTrue(Button(TEXT("Blind Pick"))->GetIsEnabled()));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Casual")) && Screen->DescribeText().Contains(TEXT("Customs"))
					&& Screen->DescribeText().Contains(TEXT("Ranked")),
				Screen->DescribeText()));
			ASSERT_THAT(IsNotNull(Button(TEXT("Practice"))));
			ASSERT_THAT(IsNotNull(Button(TEXT("Custom Game"))));
			ASSERT_THAT(IsFalse(Button(TEXT("Ranked Draft Pick"))->GetIsEnabled()));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Not yet available"))));
			ASSERT_THAT(IsNull(Button(TEXT("Ready"))));

			Button(TEXT("Blind Pick"))->Press();
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("PUT"), TEXT("/v1/party/mode"), 200, PartyBody(TEXT("idle"), false))));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("DevOne (you, leader): Not Ready")), Screen->DescribeText()));
			ASSERT_THAT(IsFalse(Button(TEXT("Find Match"))->GetIsEnabled(), TEXT("everyone must be Ready")));
			Button(TEXT("Ready"))->Press();
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("PUT"), TEXT("/v1/party/ready"), 200, PartyBody(TEXT("idle"), true))));
			ASSERT_THAT(IsNotNull(Button(TEXT("Unready"))));
			Button(TEXT("Find Match"))->Press();
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("POST"), TEXT("/v1/party/queue"), 200, PartyBody(TEXT("queued"), true))));

			// Queued: the time shows, Practice waits, and the leader may cancel.
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Estimate unavailable.")), Screen->DescribeText()));
			ASSERT_THAT(IsFalse(Button(TEXT("Practice"))->GetIsEnabled()));
			ASSERT_THAT(IsNull(Button(TEXT("Find Match"))));
			Button(TEXT("Cancel"))->Press();
			ASSERT_THAT(IsNotNull(Rig.Backend.Find(TEXT("DELETE"), TEXT("/v1/party/queue"))));
		}

		TEST_METHOD(MatchFoundBlocksTheShell)
		{
			ASSERT_THAT(IsTrue(Rig.ReachMatchFound()));
			ShowScreen();
			// Only Accept and Decline: no navigation, party controls or Quit until it is answered (UX §5).
			ASSERT_THAT(IsTrue(Screen->GetShownScreen() == EVeyraShellScreen::MatchFound));
			ASSERT_THAT(IsTrue(LabelsOf(Screen->GetButtons()) == (TArray<FString>{ TEXT("Accept"), TEXT("Decline") })));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("0 of 2 accepted"))));
			Button(TEXT("Accept"))->Press();
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("POST"), TEXT("/v1/me/match-found/accept"), 200, MatchFoundBody(TEXT("pending"), TEXT("accepted"), 1))));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Accepted. Waiting for the other players."))));
			ASSERT_THAT(IsFalse(Button(TEXT("Decline"))->GetIsEnabled()));
		}

		TEST_METHOD(ACasualSelectShowsTheTeamsAndOffersLeave)
		{
			ASSERT_THAT(IsTrue(Rig.ReachCasualSelect()));
			ShowScreen();
			const FString Text = Screen->DescribeText();
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("Your Team")) && Text.Contains(TEXT("Enemy Team")) && Text.Contains(TEXT("DevTwo")), Text));
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("BLIND PICK")), TEXT("the mode, in the corner, by its text-table name")));
			ASSERT_THAT(IsTrue(Button(TEXT("Leave"))->GetIsEnabled()));
			Button(TEXT("Leave"))->Press();
			ASSERT_THAT(IsNotNull(Rig.Backend.Find(TEXT("POST"), TEXT("/v1/me/select/leave"))));
		}

		TEST_METHOD(ASignInFailureOffersOnlyQuit)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("VeyraClientFlow: signing in failed"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
			Rig.Flow->Start();
			Rig.Host.bInputClosed = true;
			Rig.Flow->Tick();
			ShowScreen();
			ASSERT_THAT(IsTrue(Screen->GetShownScreen() == EVeyraShellScreen::Stopped));
			ASSERT_THAT(IsTrue(LabelsOf(Screen->GetButtons()) == TArray<FString>{ TEXT("Quit") }));
			Button(TEXT("Quit"))->Press();
			ASSERT_THAT(IsTrue(Rig.Host.bQuit));
		}
	};

	// Veyra.UI.ContentText.*: what players read about Vanguards, abilities and passives (ADR-010 §4).
	TEST_CLASS(ContentText, "Veyra.UI")
	{
		static FVeyraContentId IdOf(const TCHAR* Text)
		{
			return FVeyraContentId::FromText(Text).GetValue();
		}

		TEST_METHOD(EveryReleasedVanguardHasItsText)
		{
			const TArray<FString> Missing = VeyraContentText::FindMissingPlayableText();
			ASSERT_THAT(IsTrue(Missing.IsEmpty(), FString::Printf(TEXT("Game/Text/VeyraText.csv lacks %s"), *FString::Join(Missing, TEXT(", ")))));
		}

		TEST_METHOD(EveryItemHasItsText)
		{
			const TArray<FString> Missing = VeyraContentText::FindMissingItemText();
			ASSERT_THAT(IsTrue(Missing.IsEmpty(), FString::Printf(TEXT("Game/Text/VeyraText.csv lacks %s"), *FString::Join(Missing, TEXT(", ")))));
			ASSERT_THAT(AreEqual(VeyraContentText::ItemName(IdOf(TEXT("colossus_temper"))).ToString(), FString(TEXT("Colossus Temper"))));
			ASSERT_THAT(IsTrue(VeyraContentText::ItemDescription(IdOf(TEXT("iron_grip"))).IsEmpty(), TEXT("a plain component needs no description")));
		}

		TEST_METHOD(NamesComeFromTheTableAndDeveloperContentShowsItsId)
		{
			const FText Name = VeyraContentText::AbilityName(IdOf(TEXT("qazharr_heavy_hand")));
			ASSERT_THAT(IsTrue(Name.IsFromStringTable()));
			ASSERT_THAT(AreEqual(Name.ToString(), FString(TEXT("Heavy Hand"))));
			ASSERT_THAT(AreEqual(VeyraContentText::VanguardTitle(IdOf(TEXT("qazharr"))).ToString(), FString(TEXT("The Harbor Wolf"))));
			// A quoted field keeps its commas.
			ASSERT_THAT(IsTrue(VeyraContentText::AbilityDescription(IdOf(TEXT("cairn_immovable"))).ToString().Contains(TEXT("less far, but walks slower"))));
			ASSERT_THAT(AreEqual(VeyraShellModels::VanguardNameOf(TEXT("qazharr")).ToString(), FString(TEXT("Qazharr"))));

			// The developer test Vanguard has no text: its ID stands in, and it has no title or description.
			ASSERT_THAT(AreEqual(VeyraContentText::VanguardName(IdOf(TEXT("test_vanguard"))).ToString(), FString(TEXT("test_vanguard"))));
			ASSERT_THAT(IsTrue(VeyraContentText::VanguardTitle(IdOf(TEXT("test_vanguard"))).IsEmpty()));
			ASSERT_THAT(IsTrue(VeyraContentText::AbilityDescription(IdOf(TEXT("test_bolt"))).IsEmpty()));
			ASSERT_THAT(AreEqual(VeyraContentText::AbilityName(IdOf(TEXT("test_bolt"))).ToString(), FString(TEXT("test_bolt"))));
		}
	};

	// Veyra.UI.MatchMenu.*: the in-match menu (ADR-010 §4).
	TEST_CLASS(MatchMenu, "Veyra.UI")
	{
		FActorTestSpawner Spawner;

		TEST_METHOD(EndCustomMatchVisibility)
		{
			const APlayerState& Host = Spawner.SpawnActor<APlayerState>();
			const APlayerState& Other = Spawner.SpawnActor<APlayerState>();
			ASSERT_THAT(IsTrue(VeyraMatchMenuModel::CanEndCustomMatch(EVeyraMatchRules::Practice, &Host, &Host)));
			ASSERT_THAT(IsFalse(VeyraMatchMenuModel::CanEndCustomMatch(EVeyraMatchRules::Practice, &Host, &Other), TEXT("only the host")));
			ASSERT_THAT(IsFalse(VeyraMatchMenuModel::CanEndCustomMatch(EVeyraMatchRules::Standard, &Host, &Host), TEXT("only a hosted match")));
			ASSERT_THAT(IsTrue(VeyraMatchMenuModel::CanEndCustomMatch(EVeyraMatchRules::Custom, &Host, &Host), TEXT("a custom match's host too")));
			ASSERT_THAT(IsFalse(VeyraMatchMenuModel::CanEndCustomMatch(EVeyraMatchRules::Practice, nullptr, &Host), TEXT("before the host joins")));
		}

		TEST_METHOD(DeveloperEndVisibility)
		{
			// A standard match has no victory condition yet: outside Shipping a developer may end it.
			ASSERT_THAT(AreEqual(VeyraMatchMenuModel::OffersDeveloperEnd(EVeyraMatchRules::Standard), !UE_BUILD_SHIPPING));
			ASSERT_THAT(IsFalse(VeyraMatchMenuModel::OffersDeveloperEnd(EVeyraMatchRules::Practice), TEXT("practice has End Custom Match")));
		}

		TEST_METHOD(AStandardMatchsMenuStartsVotesBehindAConfirmation)
		{
			// A standard match's GameState: the menu offers its votes (ADR-019 §7).
			Spawner.SpawnActor<AVeyraGameState>();
			AVeyraPlayerController& Controller = Spawner.SpawnActor<AVeyraPlayerController>();
			UVeyraMatchMenu* Menu = CreateWidget<UVeyraMatchMenu>(&Spawner.GetWorld());
			bool bClosed = false;
			Menu->Show(Controller, [&bClosed] { bClosed = true; });
			const TArray<FString> Labels = LabelsOf(Menu->GetButtons());
			ASSERT_THAT(IsTrue(Labels.Contains(TEXT("Request Pause")) && Labels.Contains(TEXT("Surrender")) && Labels.Contains(TEXT("Remake"))));
			ASSERT_THAT(IsFalse(Labels.Contains(TEXT("Resume Early")), TEXT("only while paused")));
			ASSERT_THAT(IsFalse(Labels.Contains(TEXT("Vote Yes")), TEXT("no vote is open")));
			Menu->FindButton(FText::FromString(TEXT("Surrender")))->Press();
			ASSERT_THAT(IsTrue(LabelsOf(Menu->GetButtons()) == TArray<FString>{ TEXT("Vote to Surrender"), TEXT("Cancel") }, TEXT("behind a confirmation")));
			ASSERT_THAT(IsFalse(bClosed));
			ASSERT_THAT(IsFalse(VeyraMatchMenuModel::OffersVotes(EVeyraMatchRules::Practice), TEXT("a practice match's host ends it")));
			ASSERT_THAT(IsFalse(VeyraMatchMenuModel::OffersVotes(EVeyraMatchRules::Custom), TEXT("remake and pause are matchmade")));
			ASSERT_THAT(IsTrue(VeyraMatchMenuModel::OffersSurrender(true) && !VeyraMatchMenuModel::OffersSurrender(false), TEXT("surrender where it can be won")));
		}

		TEST_METHOD(OutsidePracticeTheMenuOffersResume)
		{
			AVeyraPlayerController& Controller = Spawner.SpawnActor<AVeyraPlayerController>();
			UVeyraMatchMenu* Menu = CreateWidget<UVeyraMatchMenu>(&Spawner.GetWorld());
			bool bClosed = false;
			Menu->Show(Controller, [&bClosed] { bClosed = true; });
			ASSERT_THAT(IsTrue(LabelsOf(Menu->GetButtons()) == TArray<FString>{ TEXT("Resume") }));
			ASSERT_THAT(IsTrue(Menu->IsFocusable(), TEXT("the open menu's input mode gives it keyboard focus")));
			Menu->FindButton(FText::FromString(TEXT("Resume")))->Press();
			ASSERT_THAT(IsTrue(bClosed));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
