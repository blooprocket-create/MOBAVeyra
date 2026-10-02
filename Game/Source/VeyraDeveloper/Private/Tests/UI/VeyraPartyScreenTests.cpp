// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Blueprint/UserWidget.h"
#include "Components/ActorTestSpawner.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellModels.h"
#include "Shell/VeyraShellScreen.h"
#include "Tests/Services/VeyraClientFlowTestRig.h"

namespace VeyraPartyScreenTests
{
	using namespace VeyraClientFlowTests;

	/** The coordinator's snapshot in the shell, the party read from PartyBody and DevTwo a friend. */
	FVeyraClientSnapshot ShellSnapshot(const FString& PartyBody)
	{
		FVeyraClientSnapshot Snapshot;
		Snapshot.State = EVeyraClientState::Shell;
		Snapshot.AccountId = AccountId;
		FString Problem;
		VeyraBackendProtocol::ParseParty(PartyBody, Snapshot.Party, Problem);
		Snapshot.Social.bLoaded = true;
		Snapshot.Social.Friends.Friends.Add({ FriendId, TEXT("DevTwo") });
		return Snapshot;
	}

	// Veyra.UI.PartyPanelScreen.*: the party panel's member cards and the friends panel's party, block and
	// request actions (ADR-044), from the coordinator's snapshot, and clicked as the player would click them.
	TEST_CLASS(PartyPanelScreen, "Veyra.UI")
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

		/** The shell's screen over the coordinator, DevTwo a friend and the party read as PartyBody. */
		bool ShowShell(const FString& PartyBody, const FString& Friends = FriendsBody(FriendList()), bool bPartyInvited = false)
		{
			if (!Rig.ReachShell())
			{
				return false;
			}
			Screen = CreateWidget<UVeyraShellScreen>(&Spawner.GetWorld());
			Screen->Bind(*Rig.Flow);
			return Rig.ReadSocialAs(Friends, /*bLobbyInvited*/ false, bPartyInvited) && Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/party"), 200, PartyBody);
		}

		UVeyraShellButton* Button(const FText& Label) const
		{
			return Screen->FindButton(Label);
		}

		bool Press(const FText& Label)
		{
			UVeyraShellButton* Found = Button(Label);
			if (!Found || !Found->GetIsEnabled())
			{
				return false;
			}
			Found->Press();
			return true;
		}

		TEST_METHOD(TheLeadersCardsOfferActionsAndThePanelItsPrivacy)
		{
			FVeyraPartyPermissions Permissions;
			Permissions.bCanKick = Permissions.bCanTransfer = Permissions.bCanSetPrivacy = Permissions.bCanLeave = true;
			const FVeyraPartyModel Model =
				VeyraShellModels::DescribeParty(ShellSnapshot(PartyOfTwoBody(TEXT("idle"), /*bYouLead*/ true)), true, false, false, Permissions);
			ASSERT_THAT(IsTrue(Model.Cards.Num() == 2 && Model.Members.Num() == 2));
			ASSERT_THAT(IsTrue(Model.Cards[0].bYou && Model.Cards[0].bLeader && !Model.Cards[0].bOffersActions, TEXT("the player's own card has none")));
			ASSERT_THAT(IsTrue(Model.Cards[1].bOffersActions && Model.Cards[1].bCanMakeLeader && Model.Cards[1].bCanRemove && Model.Cards[1].Name == TEXT("DevTwo")));
			ASSERT_THAT(AreEqual(Model.Privacy.ToString(), FString(TEXT("Private: members invite friends."))));
			ASSERT_THAT(IsTrue(Model.bOffersPrivacy && Model.bCanSetPrivacy && Model.PrivacyTarget == VeyraBackendProtocol::EPartyPrivacy::Public));
			ASSERT_THAT(AreEqual(Model.PrivacyLabel.ToString(), FString(TEXT("Make Party Public"))));
			ASSERT_THAT(IsTrue(Model.bCanLeave));

			// A member sees the cards and the privacy, but no leader's controls.
			const FVeyraPartyModel Member =
				VeyraShellModels::DescribeParty(ShellSnapshot(PartyOfTwoBody(TEXT("idle"), false, TEXT("public"))), true, false, false, Permissions);
			ASSERT_THAT(IsFalse(Member.Cards[1].bOffersActions || Member.bOffersPrivacy));
			ASSERT_THAT(AreEqual(Member.Privacy.ToString(), FString(TEXT("Public: friends may join."))));
		}

		TEST_METHOD(TheFriendsPanelOffersInvitesJoinsBlocksAndCancels)
		{
			FVeyraClientSnapshot Snapshot = ShellSnapshot(NoParty);
			Snapshot.Social.PartyInvites.Add({ PartyInviteId, FriendPartyId, { FriendId, TEXT("DevTwo") } });
			Snapshot.Social.Friends.Outgoing.Add({ InviteId, TEXT("DevThree") });
			Snapshot.Social.Blocked.Add({ LobbyId, TEXT("DevFour") });
			FVeyraSocialPermissions Permissions;
			Permissions.bCanInviteToParty = Permissions.bCanAcceptPartyInvite = Permissions.bCanBlock = Permissions.bCanUnblock = Permissions.bCanCancelRequest = true;
			FVeyraFriendsModel Model = VeyraShellModels::DescribeFriends(Snapshot, true, true, false, false, false, Permissions);
			ASSERT_THAT(IsTrue(Model.PartyInvitations.Num() == 1 && Model.bCanJoinPartyInvitations));
			ASSERT_THAT(AreEqual(Model.PartyInvitations[0].Line.ToString(), FString(TEXT("DevTwo invites you to their party."))));
			ASSERT_THAT(IsTrue(Model.Friends[0].bOffersPartyInvite && Model.Friends[0].bCanPartyInvite && !Model.Friends[0].bOffersJoinParty && Model.Friends[0].bCanBlock));
			ASSERT_THAT(IsTrue(Model.Pending.Num() == 1 && Model.Pending[0].Id == InviteId && Model.bCanCancelRequests));
			ASSERT_THAT(IsTrue(Model.Blocked.Num() == 1 && Model.Blocked[0].Name.ToString() == TEXT("DevFour") && Model.bCanUnblock));

			// A friend in the party is not invited again; a friend's Public party with room is joined (ADR-044 §3).
			Snapshot = ShellSnapshot(PartyOfTwoBody(TEXT("idle"), true));
			Snapshot.Social.Friends.JoinableParties.Add({ FriendId, FriendPartyId });
			Model = VeyraShellModels::DescribeFriends(Snapshot, true, true, false, false, false, Permissions);
			ASSERT_THAT(IsFalse(Model.Friends[0].bOffersPartyInvite));
			ASSERT_THAT(IsTrue(Model.Friends[0].bOffersJoinParty));
			// In the lobby, neither: the lobby's host invites into the lobby.
			Snapshot.State = EVeyraClientState::Lobby;
			Model = VeyraShellModels::DescribeFriends(Snapshot, true, true, false, false, false, Permissions);
			ASSERT_THAT(IsFalse(Model.Friends[0].bOffersPartyInvite || Model.Friends[0].bOffersJoinParty));
		}

		TEST_METHOD(EveryPartyAndSocialChangeRebuildsTheScreen)
		{
			const FVeyraClientSnapshot Before = ShellSnapshot(PartyOfTwoBody(TEXT("idle"), true));
			const FString Shown = VeyraShellModels::Signature(Before);
			FVeyraClientSnapshot After = ShellSnapshot(PartyOfTwoBody(TEXT("idle"), true, TEXT("public")));
			ASSERT_THAT(IsTrue(VeyraShellModels::Signature(After) != Shown, TEXT("privacy")));
			After = Before;
			After.Social.PartyInvites.Add({ PartyInviteId, FriendPartyId, { FriendId, TEXT("DevTwo") } });
			ASSERT_THAT(IsTrue(VeyraShellModels::Signature(After) != Shown, TEXT("a party invitation")));
			After = Before;
			After.Social.Friends.JoinableParties.Add({ FriendId, FriendPartyId });
			ASSERT_THAT(IsTrue(VeyraShellModels::Signature(After) != Shown, TEXT("a joinable party")));
			After = Before;
			After.Social.Blocked.Add({ InviteId, TEXT("DevThree") });
			ASSERT_THAT(IsTrue(VeyraShellModels::Signature(After) != Shown, TEXT("a block")));
		}

		TEST_METHOD(MakePartyLeaderAsksFirstNamingTheRecipient)
		{
			ASSERT_THAT(IsTrue(ShowShell(PartyOfTwoBody(TEXT("idle"), /*bYouLead*/ true))));
			ASSERT_THAT(IsNull(Button(VeyraShellModels::MakeLeaderLabel(TEXT("DevTwo"))), TEXT("a card's actions wait until it is selected")));
			ASSERT_THAT(IsTrue(Press(VeyraShellModels::PartyMemberLabel(TEXT("DevTwo")))));
			ASSERT_THAT(AreEqual(Screen->GetOpenCard(), UVeyraShellScreen::MemberCardKey(FriendId)));
			ASSERT_THAT(IsNull(Button(VeyraShellModels::BlockLabel(TEXT("DevTwo"))), TEXT("DevTwo's friend card stays closed")));
			ASSERT_THAT(IsNotNull(Button(VeyraShellModels::RemoveFromPartyLabel(TEXT("DevTwo")))));
			ASSERT_THAT(IsTrue(Press(VeyraShellModels::MakeLeaderLabel(TEXT("DevTwo")))));
			ASSERT_THAT(IsNull(Rig.Backend.Find(TEXT("PUT"), TEXT("/v1/party/leader")), TEXT("nothing is sent before the player confirms")));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Make DevTwo the party leader?"))));

			// Cancel withdraws the question; confirming sends it, and closes the card.
			ASSERT_THAT(IsTrue(Press(VeyraShellModels::CancelConfirmLabel())));
			ASSERT_THAT(IsNull(Button(VeyraShellModels::ConfirmLeaderLabel(TEXT("DevTwo")))));
			ASSERT_THAT(IsTrue(Press(VeyraShellModels::MakeLeaderLabel(TEXT("DevTwo")))));
			ASSERT_THAT(IsTrue(Press(VeyraShellModels::ConfirmLeaderLabel(TEXT("DevTwo")))));
			ASSERT_THAT(AreEqual(Rig.Backend.Find(TEXT("PUT"), TEXT("/v1/party/leader"))->Body, FString::Printf(TEXT("{\"accountId\":\"%s\"}"), FriendId)));
			ASSERT_THAT(IsTrue(Screen->GetOpenCard().IsEmpty()));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("PUT"), TEXT("/v1/party/leader"), 200, PartyOfTwoBody(TEXT("idle"), /*bYouLead*/ false))));
			ASSERT_THAT(IsNull(Button(VeyraShellModels::PartyMemberLabel(TEXT("DevTwo"))), TEXT("a member's cards offer nothing")));
			ASSERT_THAT(IsTrue(Press(VeyraShellModels::LeavePartyLabel())));
			ASSERT_THAT(IsNotNull(Rig.Backend.Find(TEXT("POST"), TEXT("/v1/party/leave"))));
		}

		TEST_METHOD(AFriendIsInvitedAndAnInvitationJoinedFromThePanel)
		{
			ASSERT_THAT(IsTrue(ShowShell(NoParty, FriendsBody(FriendList()), /*bPartyInvited*/ true)));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("DevTwo invites you to their party."))));
			ASSERT_THAT(IsTrue(Press(VeyraShellModels::PartyInviteLabel(TEXT("DevTwo")))));
			ASSERT_THAT(IsNotNull(Rig.Backend.Find(TEXT("POST"), TEXT("/v1/party/invites"))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("POST"), TEXT("/v1/party/invites"), 403, ErrorBody(TEXT("not_friends")))));
			ASSERT_THAT(IsTrue(Press(VeyraShellModels::AcceptPartyInviteLabel(TEXT("DevTwo")))));
			ASSERT_THAT(IsNotNull(Rig.Backend.Find(TEXT("POST"), FString::Printf(TEXT("/v1/party/invites/%s/accept"), PartyInviteId))));
		}

		TEST_METHOD(BlockAsksFirstFromTheFriendsCard)
		{
			ASSERT_THAT(IsTrue(ShowShell(NoParty)));
			ASSERT_THAT(IsNull(Button(VeyraShellModels::BlockLabel(TEXT("DevTwo"))), TEXT("on the friend's card, once selected")));
			ASSERT_THAT(IsTrue(Press(VeyraShellModels::FriendCardLabel(TEXT("DevTwo")))));
			ASSERT_THAT(IsNotNull(Button(VeyraShellModels::RemoveFriendLabel(TEXT("DevTwo")))));
			ASSERT_THAT(IsTrue(Press(VeyraShellModels::BlockLabel(TEXT("DevTwo")))));
			ASSERT_THAT(IsNull(Rig.Backend.Find(TEXT("PUT"), FString::Printf(TEXT("/v1/blocks/%s"), FriendId)), TEXT("nothing is sent before the player confirms")));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Block DevTwo?"))));
			ASSERT_THAT(IsTrue(Press(VeyraShellModels::ConfirmBlockLabel(TEXT("DevTwo")))));
			ASSERT_THAT(IsNotNull(Rig.Backend.Find(TEXT("PUT"), FString::Printf(TEXT("/v1/blocks/%s"), FriendId))));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
