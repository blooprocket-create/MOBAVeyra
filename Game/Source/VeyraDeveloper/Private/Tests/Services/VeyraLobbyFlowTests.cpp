// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "Tests/Services/VeyraClientFlowTestRig.h"

namespace VeyraClientFlowTests
{
	// Veyra.Services.CustomLobbyFlow.*: the coordinator's custom lobby (ADR-021) and friends panel, driven
	// through the fake backend as the shell's screens and the backend drive them.
	TEST_CLASS(CustomLobbyFlow, "Veyra.Services")
	{
		FClientFlowTestRig Rig;
		FFlowTestBackend& Backend = Rig.Backend;
		TUniquePtr<FVeyraClientFlow>& Flow = Rig.Flow;

		EVeyraClientState State() const { return Rig.State(); }
		const FVeyraClientSnapshot& Snapshot() const { return Flow->GetSnapshot(); }

		/** Answers the reads that find where the player is, up to the profile; the lobby's read waits. */
		bool ResumeToProfile()
		{
			return State() == EVeyraClientState::Loading && Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 200, NoMatch)
				&& Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200, NoSelect) && Backend.Answer(TEXT("GET"), TEXT("/v1/me/profile"), 200, ProfileBody(true));
		}

		/** From the player's lobby into its champion select, once its host starts it. */
		bool ReachCustomSelect()
		{
			return Rig.ReachLobby() && Flow->LaunchLobby() && Backend.Answer(TEXT("POST"), TEXT("/v1/lobby/launch"), 200, HostedLobbyBody(TEXT("selecting")))
				&& Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200, CustomSelectBody(TEXT("picking")))
				&& Backend.Answer(TEXT("GET"), TEXT("/v1/me/vanguards"), 200, VanguardsBody) && State() == EVeyraClientState::Selecting;
		}

		TEST_METHOD(ARestartResumesIntoTheLobby)
		{
			ASSERT_THAT(IsTrue(Rig.SignIn()));
			ASSERT_THAT(IsTrue(ResumeToProfile()));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Loading, TEXT("the lobby is read before the shell shows")));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/lobby"), 200, HostedLobbyBody())));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Lobby));
			ASSERT_THAT(IsTrue(Snapshot().Lobby.IsSet() && Snapshot().Lobby->Seats.Num() == 4 && Snapshot().Lobby->HostAccountId == AccountId));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::SetLobbyBot) && Flow->CanIssue(EVeyraClientIntent::LaunchLobby)));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::StartPractice), TEXT("the lobby holds the player until they leave it")));
		}

		TEST_METHOD(ABackendWithoutLobbiesLeadsToTheShell)
		{
			// Custom lobbies switched off: the backend serves no lobby routes.
			ASSERT_THAT(IsTrue(Rig.SignIn()));
			ASSERT_THAT(IsTrue(ResumeToProfile()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/lobby"), 404, TEXT("404 page not found"))));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Shell));
			ASSERT_THAT(IsFalse(Snapshot().Problem.IsSet()));
		}

		TEST_METHOD(TheHostInvitesSeatsABotSetsTheRulesAndStarts)
		{
			ASSERT_THAT(IsTrue(Rig.ReachLobby()));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::InviteToLobby)));
			ASSERT_THAT(IsFalse(Flow->InviteToLobby(AccountId), TEXT("only a friend is on offer")));
			ASSERT_THAT(IsTrue(Flow->InviteToLobby(FriendId)));
			ASSERT_THAT(AreEqual(Backend.Find(TEXT("POST"), TEXT("/v1/lobby/invites"))->Body, FString::Printf(TEXT("{\"accountId\":\"%s\"}"), FriendId)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/lobby/invites"), 200, TEXT("{\"id\":\"99999999-aaaa-4bbb-8ccc-dddddddddddd\",\"expiresAt\":\"2026-09-29T12:02:00Z\"}"))));
			ASSERT_THAT(AreEqual(Snapshot().Social.Feedback, FString(TEXT("lobby_invited"))));
			ASSERT_THAT(AreEqual(Snapshot().Social.FeedbackName, FString(TEXT("DevTwo"))));

			// A bot: any Vanguard and difficulty the lobby offers, in a seat no human holds (§2–§3).
			ASSERT_THAT(IsFalse(Flow->SetLobbyBot(TEXT("A"), 0, TEXT("cairn"), TEXT("beginner")), TEXT("the host sits there")));
			ASSERT_THAT(IsFalse(Flow->SetLobbyBot(TEXT("B"), 1, TEXT("raska"), TEXT("beginner")), TEXT("not offered to bots")));
			ASSERT_THAT(IsFalse(Flow->SetLobbyBot(TEXT("B"), 1, TEXT("cairn"), TEXT("expert"))));
			ASSERT_THAT(IsTrue(Flow->SetLobbyBot(TEXT("B"), 1, TEXT("cairn"), TEXT("beginner"))));
			ASSERT_THAT(AreEqual(Backend.Find(TEXT("PUT"), TEXT("/v1/lobby/seats/B/1/bot"))->Body, FString(TEXT("{\"vanguardId\":\"cairn\",\"difficulty\":\"beginner\"}"))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("PUT"), TEXT("/v1/lobby/seats/B/1/bot"), 200,
				HostedLobbyBody(TEXT("open"), LobbySeat(TEXT("B"), 1, TEXT("bot"), TEXT(""), TEXT(""), false, TEXT("cairn"), TEXT("beginner"))))));
			ASSERT_THAT(IsTrue(Snapshot().Lobby->Seats[3].Kind == VeyraBackendProtocol::ELobbySeatKind::Bot));

			// The session's rules, within the lobby's range (§4).
			ASSERT_THAT(IsFalse(Flow->SetLobbySettings(true, 30000.0)));
			ASSERT_THAT(IsTrue(Flow->SetLobbySettings(true, 1500.0)));
			ASSERT_THAT(AreEqual(Backend.Find(TEXT("PUT"), TEXT("/v1/lobby/settings"))->Body, FString(TEXT("{\"victoryEnabled\":true,\"startingGold\":1500}"))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("PUT"), TEXT("/v1/lobby/settings"), 200, HostedLobbyBody())));

			// The host starts it: its champion select opens for everyone, the bots locked from the start.
			ASSERT_THAT(IsTrue(Flow->LaunchLobby()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/lobby/launch"), 200, HostedLobbyBody(TEXT("selecting")))));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::LeaveLobby), TEXT("fixed while its select runs")));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200, CustomSelectBody(TEXT("picking")))));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Selecting));
			ASSERT_THAT(IsTrue(Snapshot().Select.Bots.Num() == 1 && Snapshot().Select.Bots[0].VanguardId == TEXT("cairn")));
		}

		TEST_METHOD(ARefusedLobbyChangeIsAProblemButNotAStop)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("VeyraClientFlow: problem in Lobby (vanguard_taken)"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
			ASSERT_THAT(IsTrue(Rig.ReachLobby()));
			ASSERT_THAT(IsTrue(Flow->SetLobbyBot(TEXT("B"), 0, TEXT("oriel"), TEXT("intermediate"))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("PUT"), TEXT("/v1/lobby/seats/B/0/bot"), 409, ErrorBody(TEXT("vanguard_taken")))));
			ASSERT_THAT(AreEqual(Snapshot().Problem->Code, FString(TEXT("vanguard_taken"))));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Lobby));
			// The lobby goes on being read.
			Rig.Advance(1.0);
			ASSERT_THAT(IsNotNull(Backend.Find(TEXT("GET"), TEXT("/v1/lobby"))));
		}

		TEST_METHOD(ACancelledCustomSelectReturnsToTheLobby)
		{
			ASSERT_THAT(IsTrue(ReachCustomSelect()));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::LeaveSelect), TEXT("a custom select may be left, back to the lobby")));
			Rig.Advance(0.5);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200, NoSelect)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), SelectPath(), 200, CustomSelectBody(TEXT("cancelled"), TEXT("timed_out")))));
			// Where the player is comes afresh: the lobby, open again.
			ASSERT_THAT(IsTrue(ResumeToProfile()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/lobby"), 200, HostedLobbyBody())));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Lobby));
			ASSERT_THAT(AreEqual(Snapshot().Notice, FString(TEXT("timed_out"))));
		}

		TEST_METHOD(LeavingACustomSelectReturnsEveryoneToTheLobby)
		{
			ASSERT_THAT(IsTrue(ReachCustomSelect()));
			ASSERT_THAT(IsTrue(Flow->LeaveSelect()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/me/select/leave"), 200, CustomSelectBody(TEXT("cancelled"), TEXT("left")))));
			ASSERT_THAT(IsTrue(ResumeToProfile()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/lobby"), 200, HostedLobbyBody())));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Lobby));
			ASSERT_THAT(AreEqual(Snapshot().Notice, FString(TEXT("you_left_custom"))));
		}

		TEST_METHOD(AGuestJoinsByInvitationAndFollowsTheStart)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::AcceptLobbyInvite), TEXT("no invitation yet")));
			ASSERT_THAT(IsTrue(Rig.ReadSocial(/*bInvited*/ true)));
			ASSERT_THAT(IsFalse(Flow->AcceptLobbyInvite(TEXT("not-an-invitation"))));
			ASSERT_THAT(IsTrue(Flow->AcceptLobbyInvite(InviteId)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), FString::Printf(TEXT("/v1/lobby/invites/%s/accept"), InviteId), 200, GuestLobbyBody())));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Lobby));
			// Only the host changes the lobby (Custom Matches Bible §1); a guest may leave.
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::SetLobbyBot)));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::LaunchLobby)));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::LeaveLobby)));

			// The host's start shows on the next read.
			Rig.Advance(1.0);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/lobby"), 200, GuestLobbyBody(TEXT("selecting")))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200, CustomSelectBody(TEXT("picking")))));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Selecting));
		}

		TEST_METHOD(AnExpiredInvitationIsSaidInThePanel)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			ASSERT_THAT(IsTrue(Rig.ReadSocial(/*bInvited*/ true)));
			ASSERT_THAT(IsTrue(Flow->AcceptLobbyInvite(InviteId)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), FString::Printf(TEXT("/v1/lobby/invites/%s/accept"), InviteId), 404, ErrorBody(TEXT("invite_not_found")))));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Shell));
			ASSERT_THAT(IsFalse(Snapshot().Problem.IsSet(), TEXT("social refusals are the panel's, not the screen's")));
			ASSERT_THAT(AreEqual(Snapshot().Social.Feedback, FString(TEXT("invite_not_found"))));
			ASSERT_THAT(IsNotNull(Backend.Find(TEXT("GET"), TEXT("/v1/friends")), TEXT("the lists are read again")));
		}

		TEST_METHOD(ARemovedGuestIsBackInTheShell)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			ASSERT_THAT(IsTrue(Rig.ReadSocial(/*bInvited*/ true)));
			ASSERT_THAT(IsTrue(Flow->AcceptLobbyInvite(InviteId)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), FString::Printf(TEXT("/v1/lobby/invites/%s/accept"), InviteId), 200, GuestLobbyBody())));
			Rig.Advance(1.0);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/lobby"), 200, NoLobby)));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Shell));
			ASSERT_THAT(AreEqual(Snapshot().Notice, FString(TEXT("lobby_gone"))));
			ASSERT_THAT(IsFalse(Snapshot().Lobby.IsSet()));
		}

		TEST_METHOD(LeavingTheLobbyReturnsToTheShell)
		{
			ASSERT_THAT(IsTrue(Rig.ReachLobby()));
			ASSERT_THAT(IsTrue(Flow->LeaveLobby()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/lobby/leave"), 204)));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Shell));
			ASSERT_THAT(IsTrue(Snapshot().Notice.IsEmpty()));
		}

		TEST_METHOD(TheHostMovesAndRemovesPlayers)
		{
			ASSERT_THAT(IsTrue(Rig.ReachLobby()));
			ASSERT_THAT(IsFalse(Flow->KickFromLobby(AccountId), TEXT("not the host themself")));
			ASSERT_THAT(IsFalse(Flow->MoveInLobby(AccountId, TEXT("A"), 0), TEXT("an empty seat only")));
			ASSERT_THAT(IsTrue(Flow->MoveInLobby(AccountId, TEXT("B"), 0)));
			ASSERT_THAT(AreEqual(Backend.Find(TEXT("PUT"), FString::Printf(TEXT("/v1/lobby/members/%s/seat"), AccountId))->Body,
				FString(TEXT("{\"side\":\"B\",\"index\":0}"))));
		}

		TEST_METHOD(AFriendRequestGoesByName)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			ASSERT_THAT(IsFalse(Flow->SendFriendRequest(TEXT("  "))));
			ASSERT_THAT(IsTrue(Flow->SendFriendRequest(TEXT(" DevTwo "))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/accounts?displayName=DevTwo"), 200, AccountJson(FriendId, TEXT("DevTwo")))));
			ASSERT_THAT(AreEqual(Backend.Find(TEXT("POST"), TEXT("/v1/friends/requests"))->Body, FString::Printf(TEXT("{\"accountId\":\"%s\"}"), FriendId)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/friends/requests"), 200, TEXT("{\"outcome\":\"requested\"}"))));
			ASSERT_THAT(AreEqual(Snapshot().Social.Feedback, FString(TEXT("friend_requested"))));

			// A name that is nobody's, and a friend already: the panel says so, and nothing stops.
			ASSERT_THAT(IsTrue(Flow->SendFriendRequest(TEXT("Nobody Here"))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/accounts?displayName=Nobody%20Here"), 404, ErrorBody(TEXT("account_not_found")))));
			ASSERT_THAT(AreEqual(Snapshot().Social.Feedback, FString(TEXT("account_not_found"))));
			ASSERT_THAT(IsTrue(Flow->SendFriendRequest(TEXT("DevTwo"))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/accounts?displayName=DevTwo"), 200, AccountJson(FriendId, TEXT("DevTwo")))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/friends/requests"), 409, ErrorBody(TEXT("already_friends")))));
			ASSERT_THAT(AreEqual(Snapshot().Social.Feedback, FString(TEXT("already_friends"))));
			ASSERT_THAT(IsFalse(Snapshot().Problem.IsSet()));

			// The player's own name is no one to ask.
			ASSERT_THAT(IsTrue(Flow->SendFriendRequest(TEXT("DevOne"))));
			ASSERT_THAT(AreEqual(Snapshot().Social.Feedback, FString(TEXT("cannot_target_self"))));
		}

		TEST_METHOD(AnsweringARequestReadsTheFriendsAgain)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/friends"), 200, FriendsBody(TEXT("[]"), FriendList()))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/lobby/invites"), 200, InvitesBody(false))));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::AnswerFriendRequest)));
			ASSERT_THAT(IsTrue(Flow->AnswerFriendRequest(FriendId, /*bAccept*/ true)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), FString::Printf(TEXT("/v1/friends/requests/%s/accept"), FriendId), 204)));
			ASSERT_THAT(AreEqual(Snapshot().Social.Feedback, FString(TEXT("friend_added"))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/friends"), 200, FriendsBody(FriendList()))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/lobby/invites"), 200, InvitesBody(false))));
			ASSERT_THAT(IsTrue(Snapshot().Social.Friends.Friends.Num() == 1 && Snapshot().Social.Friends.Incoming.IsEmpty()));
		}

		TEST_METHOD(AFailedReadOfTheFriendsNeverStopsTheShell)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			// No answer at all, then a server error: the panel waits for a later read.
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/friends"), 0)));
			ASSERT_THAT(IsFalse(Snapshot().Problem.IsSet()));
			ASSERT_THAT(IsFalse(Snapshot().Social.bLoaded));
			Rig.Advance(3.0);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/friends"), 500)));
			ASSERT_THAT(IsFalse(Snapshot().Problem.IsSet()));
			Rig.Advance(3.0);
			ASSERT_THAT(IsTrue(Rig.ReadSocial()));
			ASSERT_THAT(IsTrue(Snapshot().Social.Friends.Friends.Num() == 1));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
