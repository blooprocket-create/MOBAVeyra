// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Blueprint/UserWidget.h"
#include "Components/ActorTestSpawner.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellModels.h"
#include "Shell/VeyraShellScreen.h"
#include "Tests/Services/VeyraClientFlowTestRig.h"

namespace VeyraLobbyScreenTests
{
	using namespace VeyraClientFlowTests;
	using VeyraBackendProtocol::ELobbySeatKind;

	/** The player's lobby with DevTwo on side B and a Beginner Cairn bot beside them. */
	FString FullLobbyBody(const TCHAR* Host = AccountId)
	{
		return LobbyBody({ LobbySeat(TEXT("A"), 0, TEXT("human"), AccountId, TEXT("DevOne"), FCString::Strcmp(Host, AccountId) == 0), LobbySeat(TEXT("A"), 1),
							 LobbySeat(TEXT("B"), 0, TEXT("human"), FriendId, TEXT("DevTwo"), FCString::Strcmp(Host, FriendId) == 0),
							 LobbySeat(TEXT("B"), 1, TEXT("bot"), TEXT(""), TEXT(""), false, TEXT("cairn"), TEXT("beginner")) },
			Host, TEXT("open"), /*bVictory*/ true, TEXT("null"));
	}

	/** The coordinator's snapshot in a lobby read from Body. */
	FVeyraClientSnapshot LobbySnapshot(const FString& Body)
	{
		FVeyraClientSnapshot Snapshot;
		Snapshot.State = EVeyraClientState::Lobby;
		Snapshot.AccountId = AccountId;
		FString Problem;
		VeyraBackendProtocol::ParseLobby(Body, Snapshot.Lobby, Problem);
		return Snapshot;
	}

	// Veyra.UI.CustomLobbyScreen.*: the custom lobby's screen and the friends panel (ADR-021), from the
	// coordinator's snapshot, and clicked as the player would click them.
	TEST_CLASS(CustomLobbyScreen, "Veyra.UI")
	{
		FActorTestSpawner Spawner;
		FClientFlowTestRig Rig;
		UVeyraShellScreen* Screen = nullptr;

		AFTER_EACH()
		{
			if (Screen)
			{
				Screen->Unbind();
			}
		}

		UVeyraShellScreen& ShowScreen()
		{
			Screen = CreateWidget<UVeyraShellScreen>(&Spawner.GetWorld());
			Screen->Bind(*Rig.Flow);
			return *Screen;
		}

		UVeyraShellButton* Button(const FText& Label) const
		{
			return Screen->FindButton(Label);
		}

		TEST_METHOD(TheHostSeesBothSidesAndTheirActions)
		{
			const FVeyraClientSnapshot Snapshot = LobbySnapshot(FullLobbyBody());
			ASSERT_THAT(IsTrue(Snapshot.Lobby.IsSet()));
			const FVeyraLobbyModel Model = VeyraShellModels::DescribeLobby(Snapshot, /*bHosts*/ true, /*bCanStart*/ true, /*bCanLeave*/ true);
			ASSERT_THAT(IsTrue(Model.bHost && Model.SideA.Num() == 2 && Model.SideB.Num() == 2));
			ASSERT_THAT(AreEqual(Model.Title.ToString(), FString(TEXT("DevOne's Lobby"))));
			const FVeyraLobbySeatModel& You = Model.SideA[0];
			ASSERT_THAT(IsTrue(You.bYou && !You.bCanKick && You.Detail.ToString() == TEXT("Host")));
			ASSERT_THAT(IsFalse(You.bCanSwitchSide, TEXT("side B is full")));
			// An empty seat takes a bot; a guest can be moved across or removed; a bot changed or removed.
			ASSERT_THAT(IsTrue(Model.SideA[1].Kind == ELobbySeatKind::Empty && Model.SideA[1].bCanSetBot));
			const FVeyraLobbySeatModel& Guest = Model.SideB[0];
			ASSERT_THAT(IsTrue(Guest.bCanKick && Guest.bCanSwitchSide && Guest.SwitchToSide == TEXT("A") && Guest.SwitchToIndex == 1));
			const FVeyraLobbySeatModel& Bot = Model.SideB[1];
			ASSERT_THAT(IsTrue(Bot.bCanSetBot && Bot.bCanRemoveBot && Bot.Name.ToString() == TEXT("Cairn") && Bot.Detail.ToString() == TEXT("Beginner Bot")));
			ASSERT_THAT(IsTrue(Model.bVictoryEnabled && Model.bCanToggleVictory && Model.Victory.ToString().Contains(TEXT("Prime Well"))));
			// The game's own Gold first, then the style's choices within the lobby's range.
			ASSERT_THAT(IsTrue(Model.GoldChoices.Num() >= 2 && !Model.GoldChoices[0].Gold.IsSet() && Model.GoldChoices[0].bChosen));
			ASSERT_THAT(AreEqual(Model.GoldChoices[0].Label.ToString(), FString(TEXT("Default Gold"))));
			for (const FVeyraGoldChoiceModel& Choice : Model.GoldChoices)
			{
				ASSERT_THAT(IsTrue(!Choice.Gold.IsSet() || (Choice.Gold.GetValue() >= 0.0 && Choice.Gold.GetValue() <= 20000.0)));
			}
		}

		TEST_METHOD(AGuestSeesTheLobbyButChangesNothing)
		{
			const FVeyraClientSnapshot Snapshot = LobbySnapshot(FullLobbyBody(FriendId));
			const FVeyraLobbyModel Model = VeyraShellModels::DescribeLobby(Snapshot, /*bHosts*/ false, /*bCanStart*/ false, /*bCanLeave*/ true);
			ASSERT_THAT(IsFalse(Model.bHost));
			ASSERT_THAT(AreEqual(Model.Title.ToString(), FString(TEXT("DevTwo's Lobby"))));
			for (const TArray<FVeyraLobbySeatModel>* Side : { &Model.SideA, &Model.SideB })
			{
				for (const FVeyraLobbySeatModel& Seat : *Side)
				{
					ASSERT_THAT(IsFalse(Seat.bCanSetBot || Seat.bCanRemoveBot || Seat.bCanKick || Seat.bCanSwitchSide));
				}
			}
			ASSERT_THAT(IsTrue(Model.bCanLeave && !Model.bCanToggleVictory && Model.Status.ToString().Contains(TEXT("host"))));
		}

		TEST_METHOD(TheBotPickerMarksWhatTheSideAlreadyPlays)
		{
			const FVeyraClientSnapshot Snapshot = LobbySnapshot(FullLobbyBody());
			// Side B's bot plays Cairn: another bot there may not, but side A's may (ADR-021 §8).
			const FVeyraBotPickerModel SideB = VeyraShellModels::DescribeBotPicker(Snapshot, TEXT("B"), 0);
			const FVeyraBotChoiceModel* Cairn = SideB.Vanguards.FindByPredicate([](const FVeyraBotChoiceModel& Choice) { return Choice.VanguardId == TEXT("cairn"); });
			ASSERT_THAT(IsTrue(Cairn && Cairn->bTaken));
			const FVeyraBotPickerModel SideA = VeyraShellModels::DescribeBotPicker(Snapshot, TEXT("A"), 1);
			ASSERT_THAT(IsFalse(SideA.Vanguards.ContainsByPredicate([](const FVeyraBotChoiceModel& Choice) { return Choice.bTaken; })));
			// Its own bot is the chosen one, not a taken one.
			const FVeyraBotPickerModel Own = VeyraShellModels::DescribeBotPicker(Snapshot, TEXT("B"), 1);
			ASSERT_THAT(IsTrue(Own.Vanguards.ContainsByPredicate([](const FVeyraBotChoiceModel& Choice) { return Choice.VanguardId == TEXT("cairn") && Choice.bChosen && !Choice.bTaken; })));
			ASSERT_THAT(IsTrue(SideA.Difficulties.Num() == 2 && SideA.Difficulties[0].Value.ToString() == TEXT("Beginner")));
		}

		TEST_METHOD(ACustomSelectShowsItsBotsAndTakesVanguardsPerSide)
		{
			FVeyraClientSnapshot Snapshot;
			Snapshot.State = EVeyraClientState::Selecting;
			FString Problem;
			TOptional<VeyraBackendProtocol::FSelect> Select;
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseSelect(CustomSelectBody(TEXT("picking")), Select, Problem), Problem));
			Snapshot.Select = *Select;
			// DevTwo, on the other side, has locked Oriel.
			Snapshot.Select.Seats[1].Locked = TEXT("oriel");
			Snapshot.AvailableVanguards = { TEXT("cairn"), TEXT("oriel"), TEXT("bryn") };
			const FVeyraSelectModel Model = VeyraShellModels::DescribeSelect(Snapshot, 60.0, true, true, true);
			ASSERT_THAT(IsTrue(Model.bTeams && Model.bOffersLeave && Model.Seats.Num() == 3));
			const FVeyraSelectSeatModel& Bot = Model.Seats[2];
			ASSERT_THAT(IsTrue(!Bot.bAlly && Bot.Status == EVeyraSeatStatus::LockedIn && Bot.VanguardId == TEXT("cairn") && Bot.Name.ToString() == TEXT("Beginner Bot")));
			// Both sides may play one Vanguard: the enemy's locks and bots take nothing from the player's side.
			for (const FVeyraSelectCardModel& Card : Model.Cards)
			{
				ASSERT_THAT(IsFalse(Card.bTaken, Card.VanguardId));
			}
		}

		TEST_METHOD(TheFriendsPanelInvitesOnlyFromTheHostsLobby)
		{
			FVeyraClientSnapshot Snapshot = LobbySnapshot(HostedLobbyBody());
			Snapshot.Social.bLoaded = true;
			Snapshot.Social.Friends.Friends.Add({ FriendId, TEXT("DevTwo") });
			Snapshot.Social.Friends.Incoming.Add({ TEXT("66666666-7777-4888-8999-aaaaaaaaaaaa"), TEXT("DevThree") });
			Snapshot.Social.Feedback = TEXT("friend_requested");
			Snapshot.Social.FeedbackName = TEXT("DevFour");
			FVeyraFriendsModel Model = VeyraShellModels::DescribeFriends(Snapshot, true, true, false, false, true);
			ASSERT_THAT(IsTrue(Model.Friends.Num() == 1 && Model.Friends[0].bOffersInvite && Model.Friends[0].bCanInvite));
			ASSERT_THAT(IsTrue(Model.Requests.Num() == 1 && Model.Requests[0].Line.ToString().Contains(TEXT("DevThree"))));
			ASSERT_THAT(AreEqual(Model.Feedback.ToString(), FString(TEXT("Friend request sent to DevFour."))));

			// A friend in the lobby already is not invited again; in the shell nobody is.
			Snapshot = LobbySnapshot(FullLobbyBody());
			Snapshot.Social.Friends.Friends.Add({ FriendId, TEXT("DevTwo") });
			Model = VeyraShellModels::DescribeFriends(Snapshot, true, true, false, false, true);
			ASSERT_THAT(IsTrue(Model.Friends[0].bOffersInvite && !Model.Friends[0].bCanInvite));
			Snapshot.State = EVeyraClientState::Shell;
			Model = VeyraShellModels::DescribeFriends(Snapshot, true, true, false, false, true);
			ASSERT_THAT(IsFalse(Model.Friends[0].bOffersInvite));
			// A block is never revealed (Parties & Social Bible §6).
			ASSERT_THAT(IsFalse(VeyraShellModels::DescribeSocialFeedback(TEXT("blocked"), TEXT("DevTwo")).ToString().Contains(TEXT("block"))));
		}

		TEST_METHOD(PlayOpensACustomGameLobby)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			ShowScreen();
			Screen->FindButton(FText::FromString(TEXT("Play")))->Press();
			UVeyraShellButton* Custom = Screen->FindButton(FText::FromString(TEXT("Custom Game")));
			ASSERT_THAT(IsTrue(Custom && Custom->GetIsEnabled()));
			Custom->Press();
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("POST"), TEXT("/v1/lobby"), 200, HostedLobbyBody())));
			ASSERT_THAT(IsTrue(Screen->GetShownScreen() == EVeyraShellScreen::Lobby));
			ASSERT_THAT(IsNotNull(Button(VeyraShellModels::AddBotLabel(TEXT("A"), 1))));
			ASSERT_THAT(IsNotNull(Button(VeyraShellModels::AddBotLabel(TEXT("B"), 0))));
			ASSERT_THAT(IsNotNull(Screen->FindButton(FText::FromString(TEXT("Start Game")))));
			ASSERT_THAT(IsNotNull(Screen->FindButton(FText::FromString(TEXT("Leave Lobby")))));
			ASSERT_THAT(IsNull(Screen->FindButton(FText::FromString(TEXT("Match History"))), TEXT("the lobby holds the player until they leave it")));
			const FString Text = Screen->DescribeText();
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("DevOne's Lobby")) && Text.Contains(TEXT("Side A")) && Text.Contains(TEXT("Side B")), Text));
		}

		TEST_METHOD(AddBotOpensThePickerAndAChoiceSeatsTheBot)
		{
			ASSERT_THAT(IsTrue(Rig.ReachLobby()));
			ShowScreen();
			Button(VeyraShellModels::AddBotLabel(TEXT("B"), 1))->Press();
			ASSERT_THAT(IsTrue(Screen->IsBotPickerOpen()));
			Screen->FindButton(FText::FromString(TEXT("Intermediate")))->Press();
			UVeyraShellButton* Oriel = Button(VeyraShellModels::BotChoiceLabel(TEXT("oriel")));
			ASSERT_THAT(IsTrue(Oriel && Oriel->GetIsEnabled()));
			Oriel->Press();
			ASSERT_THAT(IsFalse(Screen->IsBotPickerOpen(), TEXT("a choice closes the picker")));
			ASSERT_THAT(AreEqual(Rig.Backend.Find(TEXT("PUT"), TEXT("/v1/lobby/seats/B/1/bot"))->Body,
				FString(TEXT("{\"vanguardId\":\"oriel\",\"difficulty\":\"intermediate\"}"))));
		}

		TEST_METHOD(TheFriendsPanelAsksByNameAndJoinsAnInvitation)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			ShowScreen();
			ASSERT_THAT(IsTrue(Rig.ReadSocial(/*bInvited*/ true)));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("DevTwo invites you to a custom game."))));
			Screen->SetFriendNameDraft(TEXT("DevThree"));
			Screen->FindButton(FText::FromString(TEXT("Add Friend")))->Press();
			ASSERT_THAT(IsNotNull(Rig.Backend.Find(TEXT("GET"), TEXT("/v1/accounts?displayName=DevThree"))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/accounts?displayName=DevThree"), 404, ErrorBody(TEXT("account_not_found")))));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("No player is named DevThree."))));

			Button(VeyraShellModels::JoinLobbyLabel(TEXT("DevTwo")))->Press();
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("POST"), FString::Printf(TEXT("/v1/lobby/invites/%s/accept"), InviteId), 200, GuestLobbyBody())));
			ASSERT_THAT(IsTrue(Screen->GetShownScreen() == EVeyraShellScreen::Lobby));
			ASSERT_THAT(IsNull(Screen->FindButton(FText::FromString(TEXT("Start Game"))), TEXT("the host starts it")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
