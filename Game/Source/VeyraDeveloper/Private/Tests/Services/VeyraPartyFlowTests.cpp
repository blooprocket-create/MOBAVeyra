// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "Tests/Services/VeyraClientFlowTestRig.h"

namespace VeyraClientFlowTests
{
	// Veyra.Services.PartySocialFlow.*: the coordinator's party and social intents (ADR-044), driven
	// through the fake backend as the shell's party and friends panels drive them.
	TEST_CLASS(PartySocialFlow, "Veyra.Services")
	{
		FClientFlowTestRig Rig;
		FFlowTestBackend& Backend = Rig.Backend;
		TUniquePtr<FVeyraClientFlow>& Flow = Rig.Flow;

		const FVeyraClientSnapshot& Snapshot() const { return Flow->GetSnapshot(); }

		/** The shell with DevTwo a friend and the party read as PartyBody. */
		bool ReachShellWithParty(const FString& PartyBody, const FString& Friends = FriendsBody(FriendList()), bool bPartyInvited = false)
		{
			return Rig.ReachShell() && Rig.ReadSocialAs(Friends, /*bLobbyInvited*/ false, bPartyInvited) && Backend.Answer(TEXT("GET"), TEXT("/v1/party"), 200, PartyBody);
		}

		static FString JoinableFriendsBody()
		{
			return FString::Printf(TEXT("{\"friends\":%s,\"incomingRequests\":[],\"outgoingRequests\":[],\"joinableParties\":{\"%s\":\"%s\"}}"), *FriendList(), FriendId,
				FriendPartyId);
		}

		TEST_METHOD(ThePartyIsManagedFromTheShellAlone)
		{
			// Never from a lobby, Match Found or champion select (UX-77); the friends panel's own actions in the lobby too.
			for (const EVeyraClientIntent Intent : { EVeyraClientIntent::InviteToParty, EVeyraClientIntent::AcceptPartyInvite, EVeyraClientIntent::JoinFriendParty,
					 EVeyraClientIntent::LeaveParty, EVeyraClientIntent::KickFromParty, EVeyraClientIntent::TransferPartyLeader, EVeyraClientIntent::SetPartyPrivacy })
			{
				ASSERT_THAT(IsTrue(FVeyraClientFlow::IsIntentAllowed(EVeyraClientState::Shell, Intent), LexToString(Intent)));
				for (const EVeyraClientState Elsewhere : { EVeyraClientState::Lobby, EVeyraClientState::MatchFound, EVeyraClientState::Selecting, EVeyraClientState::InMatch })
				{
					ASSERT_THAT(IsFalse(FVeyraClientFlow::IsIntentAllowed(Elsewhere, Intent), LexToString(Intent)));
				}
			}
			for (const EVeyraClientIntent Intent : { EVeyraClientIntent::DeclinePartyInvite, EVeyraClientIntent::BlockPlayer, EVeyraClientIntent::UnblockPlayer,
					 EVeyraClientIntent::CancelFriendRequest })
			{
				ASSERT_THAT(IsTrue(FVeyraClientFlow::IsIntentAllowed(EVeyraClientState::Lobby, Intent), LexToString(Intent)));
				ASSERT_THAT(IsFalse(FVeyraClientFlow::IsIntentAllowed(EVeyraClientState::Selecting, Intent), LexToString(Intent)));
			}
		}

		TEST_METHOD(InvitingAFriendWithoutAPartyMakesOne)
		{
			ASSERT_THAT(IsTrue(ReachShellWithParty(NoParty)));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::InviteToParty)));
			ASSERT_THAT(IsFalse(Flow->InviteToParty(AccountId), TEXT("only a friend is on offer")));
			ASSERT_THAT(IsTrue(Flow->InviteToParty(FriendId)));
			ASSERT_THAT(AreEqual(Backend.Find(TEXT("POST"), TEXT("/v1/party/invites"))->Body, FString::Printf(TEXT("{\"accountId\":\"%s\"}"), FriendId)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/party/invites"), 200, TEXT("{\"id\":\"bbbbbbbb-cccc-4ddd-8eee-ffffffffffff\",\"expiresAt\":\"2026-09-29T12:02:00Z\"}"))));
			ASSERT_THAT(AreEqual(Snapshot().Social.Feedback, FString(TEXT("party_invited"))));
			ASSERT_THAT(AreEqual(Snapshot().Social.FeedbackName, FString(TEXT("DevTwo"))));
			// The mode-less party the invitation made is read at once (UX-9), and the lists again.
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/party"), 200, PartyBody(TEXT("idle"), false))));
			ASSERT_THAT(IsTrue(Snapshot().Party.IsSet()));
			ASSERT_THAT(IsNotNull(Backend.Find(TEXT("GET"), TEXT("/v1/friends"))));

			// A refusal is the panel's, never the screen's.
			ASSERT_THAT(IsTrue(Flow->InviteToParty(FriendId)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/party/invites"), 403, ErrorBody(TEXT("blocked")))));
			ASSERT_THAT(AreEqual(Snapshot().Social.Feedback, FString(TEXT("blocked"))));
			ASSERT_THAT(IsFalse(Snapshot().Problem.IsSet()));
		}

		TEST_METHOD(AnInvitationJoinsTheFriendsParty)
		{
			ASSERT_THAT(IsTrue(ReachShellWithParty(NoParty, FriendsBody(FriendList()), /*bPartyInvited*/ true)));
			ASSERT_THAT(IsTrue(Snapshot().Social.PartyInvites.Num() == 1 && Snapshot().Social.PartyInvites[0].Inviter.DisplayName == TEXT("DevTwo")));
			ASSERT_THAT(IsFalse(Flow->AcceptPartyInvite(TEXT("not-an-invitation"))));
			ASSERT_THAT(IsTrue(Flow->AcceptPartyInvite(PartyInviteId)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), FString::Printf(TEXT("/v1/party/invites/%s/accept"), PartyInviteId), 200,
				PartyOfTwoBody(TEXT("idle"), /*bYouLead*/ false, TEXT("private"), FriendPartyId))));
			ASSERT_THAT(IsTrue(Snapshot().Party.IsSet() && Snapshot().Party->Id == FriendPartyId && Snapshot().Party->Members.Num() == 2));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::KickFromParty), TEXT("DevTwo leads it")));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::LeaveParty)));

			// An invitation into a party that filled meanwhile: the panel says so.
			ASSERT_THAT(IsTrue(Rig.ReadSocialAs(FriendsBody(FriendList()), false, /*bPartyInvited*/ true)));
			ASSERT_THAT(IsTrue(Flow->AcceptPartyInvite(PartyInviteId)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), FString::Printf(TEXT("/v1/party/invites/%s/accept"), PartyInviteId), 409, ErrorBody(TEXT("party_full")))));
			ASSERT_THAT(AreEqual(Snapshot().Social.Feedback, FString(TEXT("party_full"))));
			ASSERT_THAT(IsFalse(Snapshot().Problem.IsSet()));
			ASSERT_THAT(IsTrue(Snapshot().Party->Id == FriendPartyId, TEXT("still in the party it joined")));
		}

		TEST_METHOD(DecliningAnInvitationReadsTheListsAgain)
		{
			ASSERT_THAT(IsTrue(ReachShellWithParty(NoParty, FriendsBody(FriendList()), /*bPartyInvited*/ true)));
			ASSERT_THAT(IsTrue(Flow->DeclinePartyInvite(PartyInviteId)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), FString::Printf(TEXT("/v1/party/invites/%s/decline"), PartyInviteId), 204)));
			ASSERT_THAT(IsTrue(Rig.ReadSocialAs(FriendsBody(FriendList()))));
			ASSERT_THAT(IsTrue(Snapshot().Social.PartyInvites.IsEmpty()));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::DeclinePartyInvite)));
		}

		TEST_METHOD(TheLeaderHandsOverLeadership)
		{
			ASSERT_THAT(IsTrue(ReachShellWithParty(PartyOfTwoBody(TEXT("idle"), /*bYouLead*/ true))));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::TransferPartyLeader) && Flow->CanIssue(EVeyraClientIntent::KickFromParty)));
			ASSERT_THAT(IsFalse(Flow->TransferPartyLeader(AccountId), TEXT("not the leader themself")));
			ASSERT_THAT(IsFalse(Flow->TransferPartyLeader(InviteId), TEXT("only a member")));
			ASSERT_THAT(IsTrue(Flow->TransferPartyLeader(FriendId)));
			ASSERT_THAT(AreEqual(Backend.Find(TEXT("PUT"), TEXT("/v1/party/leader"))->Body, FString::Printf(TEXT("{\"accountId\":\"%s\"}"), FriendId)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("PUT"), TEXT("/v1/party/leader"), 200, PartyOfTwoBody(TEXT("idle"), /*bYouLead*/ false))));
			// The new leader holds the leader's controls now (UX-11).
			for (const EVeyraClientIntent Intent : { EVeyraClientIntent::KickFromParty, EVeyraClientIntent::TransferPartyLeader, EVeyraClientIntent::SetPartyPrivacy,
					 EVeyraClientIntent::FindMatch })
			{
				ASSERT_THAT(IsFalse(Flow->CanIssue(Intent), LexToString(Intent)));
			}
		}

		TEST_METHOD(TheLeaderRemovesAMember)
		{
			ASSERT_THAT(IsTrue(ReachShellWithParty(PartyOfTwoBody(TEXT("idle"), /*bYouLead*/ true))));
			ASSERT_THAT(IsFalse(Flow->KickFromParty(AccountId)));
			ASSERT_THAT(IsTrue(Flow->KickFromParty(FriendId)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("DELETE"), FString::Printf(TEXT("/v1/party/members/%s"), FriendId), 200, PartyBody(TEXT("idle"), false))));
			ASSERT_THAT(IsTrue(Snapshot().Party->Members.Num() == 1));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::KickFromParty), TEXT("no one else is left")));
		}

		TEST_METHOD(CardsWaitInTheQueueButMembersMayStillLeave)
		{
			ASSERT_THAT(IsTrue(ReachShellWithParty(PartyOfTwoBody(TEXT("queued"), /*bYouLead*/ true))));
			// A queued party's membership is locked (Parties & Social Bible §2; UX-10).
			for (const EVeyraClientIntent Intent : { EVeyraClientIntent::KickFromParty, EVeyraClientIntent::TransferPartyLeader, EVeyraClientIntent::InviteToParty })
			{
				ASSERT_THAT(IsFalse(Flow->CanIssue(Intent), LexToString(Intent)));
			}
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::LeaveParty)));
			ASSERT_THAT(IsTrue(Flow->LeaveParty()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/party/leave"), 204)));
			ASSERT_THAT(IsFalse(Snapshot().Party.IsSet()));
			ASSERT_THAT(IsNotNull(Backend.Find(TEXT("GET"), TEXT("/v1/friends")), TEXT("the friends' parties are read again")));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::LeaveParty)));
		}

		TEST_METHOD(TheLeaderMakesThePartyPublic)
		{
			ASSERT_THAT(IsTrue(ReachShellWithParty(PartyOfTwoBody(TEXT("idle"), /*bYouLead*/ true))));
			ASSERT_THAT(IsFalse(Flow->SetPartyPrivacy(VeyraBackendProtocol::EPartyPrivacy::Private), TEXT("it is Private already")));
			ASSERT_THAT(IsTrue(Flow->SetPartyPrivacy(VeyraBackendProtocol::EPartyPrivacy::Public)));
			ASSERT_THAT(AreEqual(Backend.Find(TEXT("PUT"), TEXT("/v1/party/privacy"))->Body, FString(TEXT("{\"privacy\":\"public\"}"))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("PUT"), TEXT("/v1/party/privacy"), 200, PartyOfTwoBody(TEXT("idle"), true, TEXT("public")))));
			ASSERT_THAT(IsTrue(Snapshot().Party->Privacy == VeyraBackendProtocol::EPartyPrivacy::Public));
		}

		TEST_METHOD(AFriendsPublicPartyIsJoinedDirectly)
		{
			ASSERT_THAT(IsTrue(ReachShellWithParty(NoParty, JoinableFriendsBody())));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::JoinFriendParty)));
			ASSERT_THAT(IsFalse(Flow->JoinFriendParty(AccountId)));
			ASSERT_THAT(IsTrue(Flow->JoinFriendParty(FriendId)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), FString::Printf(TEXT("/v1/parties/%s/join"), FriendPartyId), 200,
				PartyOfTwoBody(TEXT("idle"), false, TEXT("public"), FriendPartyId))));
			ASSERT_THAT(IsTrue(Snapshot().Party.IsSet() && Snapshot().Party->Id == FriendPartyId));

			// A party that closed meanwhile: the panel says so.
			ASSERT_THAT(IsTrue(Rig.ReadSocialAs(JoinableFriendsBody())));
			ASSERT_THAT(IsTrue(Flow->JoinFriendParty(FriendId)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), FString::Printf(TEXT("/v1/parties/%s/join"), FriendPartyId), 403, ErrorBody(TEXT("party_not_joinable")))));
			ASSERT_THAT(AreEqual(Snapshot().Social.Feedback, FString(TEXT("party_not_joinable"))));
			ASSERT_THAT(IsFalse(Snapshot().Problem.IsSet()));
		}

		TEST_METHOD(BlockingAPartyMateRereadsThePartyAndUnblockingFollows)
		{
			ASSERT_THAT(IsTrue(ReachShellWithParty(PartyOfTwoBody(TEXT("idle"), /*bYouLead*/ true))));
			ASSERT_THAT(IsFalse(Flow->BlockPlayer(InviteId), TEXT("a friend or a requester only")));
			ASSERT_THAT(IsTrue(Flow->BlockPlayer(FriendId)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("PUT"), FString::Printf(TEXT("/v1/blocks/%s"), FriendId), 204)));
			ASSERT_THAT(AreEqual(Snapshot().Social.Feedback, FString(TEXT("player_blocked"))));
			// A block takes them out of the party (Parties & Social Bible §6), which is read at once.
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/party"), 200, PartyBody(TEXT("idle"), false))));
			ASSERT_THAT(IsTrue(Snapshot().Party->Members.Num() == 1));
			ASSERT_THAT(IsTrue(Rig.ReadSocialAs(FriendsBody(), false, false, FriendList())));
			ASSERT_THAT(IsTrue(Snapshot().Social.Blocked.Num() == 1 && Snapshot().Social.Friends.Friends.IsEmpty()));

			ASSERT_THAT(IsTrue(Flow->UnblockPlayer(FriendId)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("DELETE"), FString::Printf(TEXT("/v1/blocks/%s"), FriendId), 204)));
			ASSERT_THAT(AreEqual(Snapshot().Social.Feedback, FString(TEXT("player_unblocked"))));
		}

		TEST_METHOD(ARequesterMayBeBlockedAndASentRequestWithdrawn)
		{
			ASSERT_THAT(IsTrue(ReachShellWithParty(NoParty, FriendsBody(TEXT("[]"), FriendList()))));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::BlockPlayer), TEXT("blocking a sender stops their requests")));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::CancelFriendRequest), TEXT("the player sent none")));

			// The next poll of the lists finds the player's own request.
			Rig.Advance(3.0);
			ASSERT_THAT(IsTrue(Rig.ReadSocialAs(FriendsBody(TEXT("[]"), TEXT("[]"), FriendList()))));
			ASSERT_THAT(IsTrue(Flow->CancelFriendRequest(FriendId)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("DELETE"), FString::Printf(TEXT("/v1/friends/requests/%s"), FriendId), 204)));
			ASSERT_THAT(AreEqual(Snapshot().Social.Feedback, FString(TEXT("friend_request_cancelled"))));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
