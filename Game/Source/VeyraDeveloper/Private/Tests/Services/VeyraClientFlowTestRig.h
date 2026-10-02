// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Backend/VeyraBackendTransport.h"
#include "Client/VeyraClientFlow.h"
#include "Templates/UniquePtr.h"

/**
 * The client-state coordinator on a fake backend and engine, for tests of the flow and of the screens
 * that show it. Each backend request waits until the test answers it; bodies are the player routes'
 * as the backend writes them.
 */
namespace VeyraClientFlowTests
{
	inline const TCHAR* const AccountId = TEXT("11111111-2222-4333-8444-555555555555");
	inline const TCHAR* const SelectId = TEXT("22222222-3333-4444-8555-666666666666");
	inline const TCHAR* const MatchId = TEXT("33333333-4444-4555-8666-777777777777");
	inline const TCHAR* const PartyId = TEXT("44444444-5555-4666-8777-888888888888");
	inline const TCHAR* const FoundId = TEXT("55555555-6666-4777-8888-999999999999");
	inline const TCHAR* const Server = TEXT("127.0.0.1:7780");
	/** The matchmade mode, and one that has no matchmaker yet, as the local backend offers them. */
	inline const TCHAR* const CasualMode = TEXT("casual_select");
	inline const TCHAR* const UnmatchedMode = TEXT("ranked");

	/** A value in a credential's format: its prefix and 43 base64url characters. It is not a credential. */
	inline FString ExampleCredential(const TCHAR* Prefix, TCHAR Fill)
	{
		return FString(Prefix) + FString::ChrN(43, Fill);
	}

	inline FString GameSession() { return ExampleCredential(TEXT("vgs_"), TEXT('B')); }
	inline FString Ticket() { return ExampleCredential(TEXT("vjt_"), TEXT('C')); }

	inline FString Quoted(const FString& Text)
	{
		return Text.IsEmpty() ? FString(TEXT("null")) : TEXT("\"") + Text + TEXT("\"");
	}

	inline FString ErrorBody(const TCHAR* Code)
	{
		return FString::Printf(TEXT("{\"error\":\"%s\"}"), Code);
	}

	inline FString SessionBody()
	{
		return FString::Printf(TEXT("{\"token\":\"%s\",\"expiresAt\":\"2026-09-27T12:00:00Z\",\"account\":{\"id\":\"%s\",\"displayName\":\"DevOne\"}}"), *GameSession(), AccountId);
	}

	inline const TCHAR* const NoMatch = TEXT("{\"match\":null}");

	inline FString StartingMatch()
	{
		return FString::Printf(TEXT("{\"match\":{\"id\":\"%s\",\"mode\":\"custom_practice\",\"rules\":\"practice\",\"state\":\"allocating\",\"side\":\"A\",")
							   TEXT("\"vanguardId\":\"oriel\",\"server\":null,\"ticket\":null}}"),
			MatchId);
	}

	inline FString ReadyMatch()
	{
		return FString::Printf(TEXT("{\"match\":{\"id\":\"%s\",\"mode\":\"custom_practice\",\"rules\":\"practice\",\"state\":\"ready\",\"side\":\"A\",")
							   TEXT("\"vanguardId\":\"oriel\",\"server\":{\"host\":\"127.0.0.1\",\"port\":7780},\"ticket\":\"%s\"}}"),
			MatchId, *Ticket());
	}

	inline const TCHAR* const NoSelect = TEXT("{\"select\":null}");

	/** FluxSpells is the seat's spells as JSON, such as ["blink",""]. */
	inline FString SelectBody(const TCHAR* State, const FString& Hover = FString(), const FString& Locked = FString(), const FString& StartedMatch = FString(),
		const FString& CancelReason = FString(), double Remaining = 30.0, const TCHAR* FluxSpells = TEXT("[\"\",\"\"]"))
	{
		return FString::Printf(TEXT("{\"select\":{\"id\":\"%s\",\"kind\":\"practice\",\"mode\":\"custom_practice\",\"state\":\"%s\",")
							   TEXT("\"deadline\":\"2026-09-27T12:00:30Z\",\"remainingSeconds\":%g,\"pickSeconds\":30,")
							   TEXT("\"seats\":[{\"displayName\":\"DevOne\",\"side\":\"A\",\"you\":true,\"hover\":%s,\"locked\":%s,\"fluxSpells\":%s}],")
							   TEXT("\"matchId\":%s,\"cancelReason\":%s}}"),
			SelectId, State, Remaining, *Quoted(Hover), *Quoted(Locked), FluxSpells, *Quoted(StartedMatch), *Quoted(CancelReason));
	}

	inline FString ProfileBody(bool bCompleted)
	{
		return FString::Printf(TEXT("{\"account\":{\"id\":\"%s\",\"displayName\":\"DevOne\"},\"tutorial\":{\"completed\":%s,\"starterVanguardId\":%s}}"), AccountId,
			bCompleted ? TEXT("true") : TEXT("false"), bCompleted ? TEXT("\"oriel\"") : TEXT("null"));
	}

	inline const TCHAR* const VanguardsBody = TEXT("{\"owned\":[],\"rotation\":[\"cairn\",\"qazharr\",\"oriel\",\"bryn\"],")
											  TEXT("\"available\":[\"cairn\",\"qazharr\",\"oriel\",\"bryn\"],\"starters\":[\"cairn\",\"qazharr\",\"oriel\",\"bryn\"],")
										  TEXT("\"released\":[\"cairn\",\"qazharr\",\"oriel\",\"bryn\",\"silt\"]}");

	inline FString OutcomeBody(const TCHAR* State, bool bWithResult)
	{
		const FString Result = bWithResult
			? FString(TEXT("{\"endReason\":\"host_ended\",\"winner\":null,\"durationSeconds\":42.5,\"joined\":true,\"connectedAtEnd\":true}"))
			: FString(TEXT("null"));
		return FString::Printf(TEXT("{\"match\":{\"id\":\"%s\",\"mode\":\"custom_practice\",\"rules\":\"practice\",\"state\":\"%s\",\"side\":\"A\",")
							   TEXT("\"vanguardId\":\"oriel\",\"failureReason\":null,\"result\":%s}}"),
			MatchId, State, *Result);
	}

	/** One line of a verified scoreboard, as the backend returns it (ADR-017 §5). */
	inline FString ScoreboardLine(const TCHAR* Side, const TCHAR* Name, const TCHAR* Vanguard, bool bYou, int32 Kills, double StunSeconds)
	{
		return FString::Printf(TEXT("{\"side\":\"%s\",\"name\":\"%s\",\"vanguardId\":\"%s\",\"you\":%s,\"statistics\":{\"kills\":%d,\"deaths\":1,\"assists\":2,")
								   TEXT("\"level\":9,\"minionKills\":80,\"jungleKills\":4,\"wellsSecured\":1,\"wellFinalHits\":0,\"wardsPlaced\":3,\"wardsDestroyed\":1,\"buybacks\":0,")
								   TEXT("\"vanguardDamage\":4200.4,\"damageShielded\":0,\"selfHealing\":150,\"teammateHealing\":0,\"goldEarned\":5321.9,")
								   TEXT("\"towerDamage\":1800,\"wellDamage\":600,\"damageDealt\":{\"physical\":9000,\"magic\":0,\"true\":45},")
								   TEXT("\"damageTaken\":{\"physical\":3000,\"magic\":1000,\"true\":0},\"crowdControl\":{\"stun\":%g,\"slow\":0,\"total\":%g},")
								   TEXT("\"goldBySource\":{\"starting\":500,\"kills\":600,\"assists\":100,\"minions\":2400,\"jungle\":80,\"objectives\":400,")
								   TEXT("\"wards\":30,\"passive\":1211.9}},\"items\":[\"timing_coil\",\"\",\"\",\"\",\"\",\"\"],\"fluxSpells\":[\"blink\",\"mend\"]}"),
			Side, Name, Vanguard, bYou ? TEXT("true") : TEXT("false"), Kills, StunSeconds, StunSeconds);
	}

	/** An ended casual match the player won on side A, with its scoreboard and one Flux Well each side secured. */
	inline FString ScoredOutcomeBody()
	{
		const FString Result = FString::Printf(TEXT("{\"endReason\":\"prime_well_destroyed\",\"winner\":\"A\",\"durationSeconds\":1510.5,\"joined\":true,")
												   TEXT("\"connectedAtEnd\":true,\"players\":[%s,%s,%s],\"wells\":[{\"site\":0,\"side\":\"A\",\"atSeconds\":600},")
												   TEXT("{\"site\":1,\"side\":\"B\",\"atSeconds\":900}]}"),
			*ScoreboardLine(TEXT("A"), TEXT("DevOne"), TEXT("cairn"), true, 3, 2.5), *ScoreboardLine(TEXT("B"), TEXT("DevTwo"), TEXT("oriel"), false, 1, 0.0),
			*ScoreboardLine(TEXT("B"), TEXT("Bot 1"), TEXT("bryn"), false, 0, 1.3));
		return FString::Printf(TEXT("{\"match\":{\"id\":\"%s\",\"mode\":\"casual_select\",\"rules\":\"standard\",\"state\":\"ended\",\"side\":\"A\",")
								   TEXT("\"vanguardId\":\"cairn\",\"failureReason\":null,\"result\":%s}}"),
			MatchId, *Result);
	}

	/** Another completed match of the player's, for Match History. */
	inline const TCHAR* const OlderMatchId = TEXT("66666666-7777-4888-8999-aaaaaaaaaaaa");

	/** One completed match as Match History lists it. */
	inline FString HistoryEntry(const TCHAR* Id, const TCHAR* Outcome)
	{
		return FString::Printf(TEXT("{\"id\":\"%s\",\"mode\":\"casual_select\",\"rules\":\"standard\",\"endedAt\":\"2026-09-29T10:03:12.123456Z\",")
								   TEXT("\"durationSeconds\":1510.5,\"side\":\"A\",\"vanguardId\":\"cairn\",\"outcome\":\"%s\"}"),
			Id, Outcome);
	}

	/** A page of Match History; Next is JSON: a quoted cursor, or null. Modes is JSON too: every mode with a saved match. */
	inline FString HistoryBody(const TArray<FString>& Entries, const TCHAR* Next, const TCHAR* Modes = TEXT("[\"casual_select\",\"custom_practice\"]"))
	{
		return FString::Printf(TEXT("{\"matches\":[%s],\"next\":%s,\"modes\":%s}"), *FString::Join(Entries, TEXT(",")), Next, Modes);
	}

	/** A matchmade select: the player on side A, an opponent on side B. */
	inline FString CasualSelectBody(const TCHAR* State, const FString& CancelReason = FString())
	{
		return FString::Printf(TEXT("{\"select\":{\"id\":\"%s\",\"kind\":\"casual\",\"mode\":\"casual_select\",\"state\":\"%s\",")
							   TEXT("\"deadline\":\"2026-09-27T12:01:00Z\",\"remainingSeconds\":60,\"pickSeconds\":60,")
							   TEXT("\"seats\":[{\"displayName\":\"DevOne\",\"side\":\"A\",\"you\":true,\"hover\":null,\"locked\":null},")
							   TEXT("{\"displayName\":\"DevTwo\",\"side\":\"B\",\"you\":false,\"hover\":null,\"locked\":null}],")
							   TEXT("\"matchId\":null,\"cancelReason\":%s}}"),
			SelectId, State, *Quoted(CancelReason));
	}

	inline const TCHAR* const ModesBody = TEXT("{\"modes\":[{\"id\":\"casual_select\",\"category\":\"casual\",\"enabled\":true,\"humanPlayersPerTeam\":1,\"matchmaking\":\"casualSelect\"},")
										  TEXT("{\"id\":\"ranked\",\"category\":\"ranked\",\"enabled\":true,\"humanPlayersPerTeam\":5,\"matchmaking\":\"notImplemented\"}]}");

	inline const TCHAR* const NoParty = TEXT("{\"party\":null}");

	/** The player's party of one, which they lead. */
	inline FString PartyBody(const TCHAR* Status, bool bReady, double QueuedSeconds = 0.0)
	{
		return FString::Printf(TEXT("{\"party\":{\"id\":\"%s\",\"mode\":\"casual_select\",\"privacy\":\"private\",\"status\":\"%s\",\"queuedSeconds\":%g,")
							   TEXT("\"members\":[{\"accountId\":\"%s\",\"displayName\":\"DevOne\",\"ready\":%s,\"leader\":true}]}}"),
			PartyId, Status, QueuedSeconds, AccountId, bReady ? TEXT("true") : TEXT("false"));
	}

	inline const TCHAR* const NoMatchFound = TEXT("{\"matchFound\":null}");

	/** A 1v1 match found, as the player sees it. */
	inline FString MatchFoundBody(const TCHAR* State, const TCHAR* You, int32 Accepted, const FString& OpenedSelect = FString(), const FString& AbandonReason = FString(),
		double Remaining = 15.0)
	{
		return FString::Printf(TEXT("{\"matchFound\":{\"id\":\"%s\",\"mode\":\"casual_select\",\"state\":\"%s\",\"deadline\":\"2026-09-27T12:00:15Z\",")
							   TEXT("\"remainingSeconds\":%g,\"accepted\":%d,\"total\":2,\"you\":\"%s\",\"selectId\":%s,\"abandonReason\":%s}}"),
			FoundId, State, Remaining, Accepted, You, *Quoted(OpenedSelect), *Quoted(AbandonReason));
	}

	// The custom lobby (ADR-021) and the friends panel.
	inline const TCHAR* const LobbyId = TEXT("77777777-8888-4999-8aaa-bbbbbbbbbbbb");
	inline const TCHAR* const InviteId = TEXT("99999999-aaaa-4bbb-8ccc-dddddddddddd");
	/** DevTwo: the player's friend in the lobby tests. */
	inline const TCHAR* const FriendId = TEXT("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee");

	inline const TCHAR* const NoLobby = TEXT("{\"lobby\":null}");

	/** A lobby seat as the backend writes it: Kind is "empty", "human" or "bot", and the fields that do not apply are empty. */
	inline FString LobbySeat(const TCHAR* Side, int32 Index, const TCHAR* Kind = TEXT("empty"), const TCHAR* Account = TEXT(""), const TCHAR* Name = TEXT(""),
		bool bHost = false, const TCHAR* Vanguard = TEXT(""), const TCHAR* Difficulty = TEXT(""))
	{
		return FString::Printf(TEXT("{\"side\":\"%s\",\"index\":%d,\"kind\":\"%s\",\"accountId\":\"%s\",\"displayName\":\"%s\",\"host\":%s,\"vanguardId\":\"%s\",\"difficulty\":\"%s\"}"),
			Side, Index, Kind, Account, Name, bHost ? TEXT("true") : TEXT("false"), Vanguard, Difficulty);
	}

	/** A lobby of two seats a side: Seats are A's then B's; StartingGold is JSON, a number or null. */
	inline FString LobbyBody(const TArray<FString>& Seats, const TCHAR* Host = AccountId, const TCHAR* Status = TEXT("open"), bool bVictory = false,
		const TCHAR* StartingGold = TEXT("null"))
	{
		return FString::Printf(TEXT("{\"lobby\":{\"id\":\"%s\",\"hostAccountId\":\"%s\",\"status\":\"%s\",\"playersPerSide\":2,")
								   TEXT("\"settings\":{\"victoryEnabled\":%s,\"startingGold\":%s},\"startingGoldRange\":{\"min\":0,\"max\":20000},\"seats\":[%s],")
								   TEXT("\"botVanguards\":[\"bryn\",\"cairn\",\"oriel\",\"qazharr\"],\"botDifficulties\":[\"beginner\",\"intermediate\"]}}"),
			LobbyId, Host, Status, bVictory ? TEXT("true") : TEXT("false"), StartingGold, *FString::Join(Seats, TEXT(",")));
	}

	/** The player's own lobby: hosting from side A's first seat, with B1 as given. */
	inline FString HostedLobbyBody(const TCHAR* Status = TEXT("open"), const FString& SeatB1 = LobbySeat(TEXT("B"), 1))
	{
		return LobbyBody({ LobbySeat(TEXT("A"), 0, TEXT("human"), AccountId, TEXT("DevOne"), true), LobbySeat(TEXT("A"), 1), LobbySeat(TEXT("B"), 0), SeatB1 },
			AccountId, Status);
	}

	/** DevTwo's lobby, with the player seated on side B as a guest. */
	inline FString GuestLobbyBody(const TCHAR* Status = TEXT("open"))
	{
		return LobbyBody({ LobbySeat(TEXT("A"), 0, TEXT("human"), FriendId, TEXT("DevTwo"), true), LobbySeat(TEXT("A"), 1),
							 LobbySeat(TEXT("B"), 0, TEXT("human"), AccountId, TEXT("DevOne")), LobbySeat(TEXT("B"), 1) },
			FriendId, Status);
	}

	inline FString AccountJson(const TCHAR* Id, const TCHAR* Name)
	{
		return FString::Printf(TEXT("{\"id\":\"%s\",\"displayName\":\"%s\"}"), Id, Name);
	}

	/** GET /v1/friends: each list is JSON. */
	inline FString FriendsBody(const FString& Friends = TEXT("[]"), const FString& Incoming = TEXT("[]"), const FString& Outgoing = TEXT("[]"))
	{
		return FString::Printf(TEXT("{\"friends\":%s,\"incomingRequests\":%s,\"outgoingRequests\":%s}"), *Friends, *Incoming, *Outgoing);
	}

	/** DevTwo, as a one-account list. */
	inline FString FriendList() { return TEXT("[") + AccountJson(FriendId, TEXT("DevTwo")) + TEXT("]"); }

	/** GET /v1/lobby/invites, with DevTwo's invitation or none. */
	inline FString InvitesBody(bool bInvited)
	{
		const FString Invite = FString::Printf(TEXT("{\"id\":\"%s\",\"lobbyId\":\"%s\",\"inviter\":%s,\"expiresAt\":\"2026-09-29T12:02:00Z\"}"), InviteId, LobbyId,
			*AccountJson(FriendId, TEXT("DevTwo")));
		return FString::Printf(TEXT("{\"invites\":[%s]}"), bInvited ? *Invite : TEXT(""));
	}

	/**
	 * A Draft Pick select (ADR-042): the player and DevThree on side A, DevTwo on side B, in Phase with
	 * Turn (JSON, or null) and Bans (JSON). YouSeat and TeammateSeat are the two side A seats' details
	 * after their names and sides, such as "\"hover\":null,\"locked\":null,\"acting\":true".
	 */
	inline FString DraftSelectBody(const TCHAR* Phase, const TCHAR* Turn, const TCHAR* Bans, const TCHAR* YouSeat, const TCHAR* TeammateSeat)
	{
		return FString::Printf(TEXT("{\"select\":{\"id\":\"%s\",\"kind\":\"draft\",\"mode\":\"draft_pick\",\"state\":\"picking\",\"phase\":\"%s\",")
							   TEXT("\"turn\":%s,\"bans\":%s,\"deadline\":\"2026-09-27T12:00:30Z\",\"remainingSeconds\":30,\"pickSeconds\":30,")
							   TEXT("\"seats\":[{\"displayName\":\"DevOne\",\"side\":\"A\",\"you\":true,%s},")
							   TEXT("{\"displayName\":\"DevThree\",\"side\":\"A\",\"you\":false,%s},")
							   TEXT("{\"displayName\":\"DevTwo\",\"side\":\"B\",\"you\":false,\"hover\":null,\"locked\":null}],")
							   TEXT("\"matchId\":null,\"cancelReason\":null}}"),
			SelectId, Phase, Turn, Bans, YouSeat, TeammateSeat);
	}

	/** A custom lobby's select: the player on side A, DevTwo on side B beside a Beginner Cairn bot. */
	inline FString CustomSelectBody(const TCHAR* State, const FString& CancelReason = FString())
	{
		return FString::Printf(TEXT("{\"select\":{\"id\":\"%s\",\"kind\":\"custom\",\"mode\":\"custom_game\",\"state\":\"%s\",")
								   TEXT("\"deadline\":\"2026-09-27T12:01:00Z\",\"remainingSeconds\":60,\"pickSeconds\":60,")
								   TEXT("\"seats\":[{\"displayName\":\"DevOne\",\"side\":\"A\",\"you\":true,\"hover\":null,\"locked\":null},")
								   TEXT("{\"displayName\":\"DevTwo\",\"side\":\"B\",\"you\":false,\"hover\":null,\"locked\":null}],")
								   TEXT("\"bots\":[{\"side\":\"B\",\"vanguardId\":\"cairn\",\"difficulty\":\"beginner\"}],\"matchId\":null,\"cancelReason\":%s}}"),
			SelectId, State, *Quoted(CancelReason));
	}

	inline FString MatchOutcomePath() { return FString(TEXT("/v1/me/matches/")) + MatchId; }
	inline FString SelectPath() { return FString(TEXT("/v1/me/selects/")) + SelectId; }

	/** A backend that holds every request until the test answers it. */
	class FFlowTestBackend final : public IVeyraBackendTransport
	{
	public:
		struct FRequest
		{
			FString Verb;
			FString Path;
			FString Credential;
			FString Body;
			FVeyraBackendCallback OnDone;
		};

		TArray<FRequest> Pending;

		virtual void Get(const FString& Path, const FString& Credential, FVeyraBackendCallback OnDone) override
		{
			Pending.Add({ TEXT("GET"), Path, Credential, FString(), MoveTemp(OnDone) });
		}

		virtual void Post(const FString& Path, const FString& Credential, const FString& Body, FVeyraBackendCallback OnDone) override
		{
			Pending.Add({ TEXT("POST"), Path, Credential, Body, MoveTemp(OnDone) });
		}

		virtual void Put(const FString& Path, const FString& Credential, const FString& Body, FVeyraBackendCallback OnDone) override
		{
			Pending.Add({ TEXT("PUT"), Path, Credential, Body, MoveTemp(OnDone) });
		}

		virtual void Delete(const FString& Path, const FString& Credential, FVeyraBackendCallback OnDone) override
		{
			Pending.Add({ TEXT("DELETE"), Path, Credential, FString(), MoveTemp(OnDone) });
		}

		const FRequest* Find(const TCHAR* Verb, const FString& Path) const
		{
			return Pending.FindByPredicate([Verb, &Path](const FRequest& Request) { return Request.Verb == Verb && Request.Path == Path; });
		}

		/** Answers the oldest pending request with this verb and path; a status of 0 is no answer at all. False if none is pending. */
		bool Answer(const TCHAR* Verb, const FString& Path, int32 Status, const FString& Body = FString())
		{
			const int32 Index = Pending.IndexOfByPredicate([Verb, &Path](const FRequest& Request) { return Request.Verb == Verb && Request.Path == Path; });
			if (Index == INDEX_NONE)
			{
				return false;
			}
			FRequest Request = MoveTemp(Pending[Index]);
			Pending.RemoveAt(Index);
			FVeyraBackendResponse Response;
			Response.bAnswered = Status > 0;
			Response.Status = Status;
			Response.Body = Body;
			Request.OnDone(Response);
			return true;
		}
	};

	/** The player's account settings on this client, as the settings subsystem keeps them, in memory. */
	class FFakeAccountSettingsCache final : public IVeyraAccountSettingsCache
	{
	public:
		FString Account;
		FVeyraAccountSettingsDocument Document;
		uint32 Changes = 0;

		/** The player changes a setting here. */
		void Change(const FString& Id, const FString& Value)
		{
			Document.Values.Add(Id, Value);
			Document.bUnsent = true;
			++Changes;
		}

		virtual void UseAccount(const FString& SignedIn) override { Account = SignedIn; }
		virtual FVeyraAccountSettingsDocument GetDocument() const override { return Document; }
		virtual void TakeDocument(const FVeyraAccountSettingsDocument& Taken) override
		{
			Document = Taken;
			Document.bUnsent = false;
		}
		virtual void MarkSent(int64 Revision, uint32 SentChangeCount) override
		{
			Document.Revision = Revision;
			Document.bUnsent = SentChangeCount != Changes;
		}
		virtual bool HasUnsentChanges() const override { return Document.bUnsent; }
		virtual uint32 GetChangeCount() const override { return Changes; }
	};

	/** An engine that records what the flow asks of it. */
	class FFlowTestHost final : public IVeyraClientFlowHost
	{
	public:
		double Clock = 100.0;
		TArray<FString> Input;
		bool bInputClosed = false;
		TArray<FString> HandshakeLines;
		/** "match <address>" or "front end". */
		TArray<FString> Travels;
		FString JoinTicket;
		bool bQuit = false;

		virtual double Now() const override { return Clock; }

		virtual EVeyraPipeRead PollLaunchCode(FString& OutLine) override
		{
			if (!Input.IsEmpty())
			{
				OutLine = Input[0];
				Input.RemoveAt(0);
				return EVeyraPipeRead::Line;
			}
			return bInputClosed ? EVeyraPipeRead::EndOfInput : EVeyraPipeRead::Pending;
		}

		virtual void WriteHandshake(const FString& Line) override { HandshakeLines.Add(Line); }

		virtual bool TravelToMatch(const FString& Address, const FString& InTicket) override
		{
			Travels.Add(TEXT("match ") + Address);
			JoinTicket = InTicket;
			return true;
		}

		virtual void TravelToFrontEnd() override
		{
			Travels.Add(TEXT("front end"));
			JoinTicket.Reset();
		}

		virtual void QuitGame() override { bQuit = true; }
	};

	/** A coordinator on the fakes, with the steps most tests start from. */
	struct FClientFlowTestRig
	{
		FFlowTestBackend Backend;
		FFlowTestHost Host;
		TUniquePtr<FVeyraClientFlow> Flow;

		/** Fixture value: how long a player watches its match end. */
		static constexpr double EndingShowSeconds = 6.0;

		FClientFlowTestRig()
		{
			// Fixture values: short waits, and two attempts at an unanswered request.
			FVeyraClientFlowConfig Config;
			Config.BuildVersion = TEXT("0.1.0");
			Config.LaunchCodeReadTimeoutSeconds = 30.0;
			Config.RequestAttempts = 2;
			Config.RetryIntervalSeconds = 1.0;
			Config.SelectPollIntervalSeconds = 0.5;
			Config.MatchPollIntervalSeconds = 1.0;
			Config.MatchWaitTimeoutSeconds = 60.0;
			Config.ResultPollIntervalSeconds = 1.0;
			Config.ResultWaitTimeoutSeconds = 10.0;
			Config.ReconnectPollIntervalSeconds = 5.0;
			Config.PartyPollIntervalSeconds = 1.0;
			Config.MatchFoundPollIntervalSeconds = 0.5;
			Config.LobbyPollIntervalSeconds = 1.0;
			Config.SocialPollIntervalSeconds = 3.0;
			Config.EndingShowSeconds = EndingShowSeconds;
			Config.AccountSettings.SendDelaySeconds = 1.5;
			Config.AccountSettings.RetrySeconds = 15.0;
			Flow = MakeUnique<FVeyraClientFlow>(Backend, Host, Config);
		}

		EVeyraClientState State() const { return Flow->GetSnapshot().State; }

		void Advance(double Seconds)
		{
			Host.Clock += Seconds;
			Flow->Tick();
		}

		/** With the flow syncing account settings, sign-in answers their read with this document. */
		TOptional<FString> AccountSettingsAnswer;

		/**
		 * Starts, reads a launch code and redeems it: the flow then asks for the player's match, or, when
		 * it syncs account settings and the rig has no answer for them, for those first.
		 */
		bool SignIn()
		{
			Flow->Start();
			Host.Input.Add(ExampleCredential(TEXT("vlc_"), TEXT('A')));
			Flow->Tick();
			return Backend.Answer(TEXT("POST"), TEXT("/v1/game-sessions"), 200, SessionBody()) && State() == EVeyraClientState::Loading
				&& (!AccountSettingsAnswer.IsSet() || Backend.Answer(TEXT("GET"), TEXT("/v1/account/settings"), 200, *AccountSettingsAnswer));
		}

		/** Signs in with no match and no select, and a profile whose tutorial is Completed or not. */
		bool ReachProfile(bool bCompleted)
		{
			return SignIn() && Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 200, NoMatch) && Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200, NoSelect)
				&& Backend.Answer(TEXT("GET"), TEXT("/v1/me/profile"), 200, ProfileBody(bCompleted));
		}

		/**
		 * Signs in with no match, no select, the tutorial completed and no lobby. The shell's reads of the
		 * modes, the party and the friends wait.
		 */
		bool ReachShell()
		{
			return ReachProfile(true) && Backend.Answer(TEXT("GET"), TEXT("/v1/lobby"), 200, NoLobby) && State() == EVeyraClientState::Shell;
		}

		/** From the shell, the friends read: DevTwo a friend, and with DevTwo's lobby invitation or not. */
		bool ReadSocial(bool bInvited = false)
		{
			return Backend.Answer(TEXT("GET"), TEXT("/v1/friends"), 200, FriendsBody(FriendList()))
				&& Backend.Answer(TEXT("GET"), TEXT("/v1/lobby/invites"), 200, InvitesBody(bInvited)) && Flow->GetSnapshot().Social.bLoaded;
		}

		/**
		 * From the shell, its friends read, into a new lobby the player hosts. The lobby's own read of the
		 * friends waits.
		 */
		bool ReachLobby()
		{
			return ReachShell() && ReadSocial() && Flow->CreateLobby() && Backend.Answer(TEXT("POST"), TEXT("/v1/lobby"), 200, HostedLobbyBody())
				&& State() == EVeyraClientState::Lobby;
		}

		/** From the shell into the queue: the modes read, no party, then casual select chosen, Ready, and Find Match. */
		bool ReachQueue()
		{
			return ReachShell() && Backend.Answer(TEXT("GET"), TEXT("/v1/modes"), 200, ModesBody) && Backend.Answer(TEXT("GET"), TEXT("/v1/party"), 200, NoParty)
				&& Flow->SelectMode(CasualMode) && Backend.Answer(TEXT("PUT"), TEXT("/v1/party/mode"), 200, PartyBody(TEXT("idle"), false))
				&& Flow->SetReady(true) && Backend.Answer(TEXT("PUT"), TEXT("/v1/party/ready"), 200, PartyBody(TEXT("idle"), true)) && Flow->FindMatch()
				&& Backend.Answer(TEXT("POST"), TEXT("/v1/party/queue"), 200, PartyBody(TEXT("queued"), true)) && State() == EVeyraClientState::Shell;
		}

		/** The queued party's next read shows a match found, which waits for the player's answer. */
		bool ReachMatchFound()
		{
			if (!ReachQueue())
			{
				return false;
			}
			Advance(1.0);
			return Backend.Answer(TEXT("GET"), TEXT("/v1/party"), 200, PartyBody(TEXT("found"), true, 1.0))
				&& Backend.Answer(TEXT("GET"), TEXT("/v1/me/match-found"), 200, MatchFoundBody(TEXT("pending"), TEXT("pending"), 0))
				&& State() == EVeyraClientState::MatchFound;
		}

		/** Everyone accepts the match found: its casual select opens, with the available Vanguards read. */
		bool ReachCasualSelect()
		{
			return ReachMatchFound() && Flow->AcceptMatch()
				&& Backend.Answer(TEXT("POST"), TEXT("/v1/me/match-found/accept"), 200, MatchFoundBody(TEXT("accepted"), TEXT("accepted"), 2, SelectId))
				&& Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 200, NoMatch)
				&& Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200, CasualSelectBody(TEXT("picking")))
				&& Backend.Answer(TEXT("GET"), TEXT("/v1/me/vanguards"), 200, VanguardsBody) && State() == EVeyraClientState::Selecting;
		}

		/** Everyone accepts the match found: its draft opens as Body says, with the available and released Vanguards read. */
		bool ReachDraftSelect(const FString& Body)
		{
			return ReachMatchFound() && Flow->AcceptMatch()
				&& Backend.Answer(TEXT("POST"), TEXT("/v1/me/match-found/accept"), 200, MatchFoundBody(TEXT("accepted"), TEXT("accepted"), 2, SelectId))
				&& Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 200, NoMatch) && Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200, Body)
				&& Backend.Answer(TEXT("GET"), TEXT("/v1/me/vanguards"), 200, VanguardsBody) && State() == EVeyraClientState::Selecting;
		}

		/** Signs in as a new player, with the starters read. */
		bool ReachStarterChoice()
		{
			return ReachProfile(false) && Backend.Answer(TEXT("GET"), TEXT("/v1/me/vanguards"), 200, VanguardsBody) && State() == EVeyraClientState::StarterChoice;
		}

		/** From the shell into a practice select, with the available Vanguards read. */
		bool ReachSelect()
		{
			return ReachShell() && Flow->StartPractice() && Backend.Answer(TEXT("POST"), TEXT("/v1/practice"), 201, SelectBody(TEXT("picking")))
				&& Backend.Answer(TEXT("GET"), TEXT("/v1/me/vanguards"), 200, VanguardsBody) && State() == EVeyraClientState::Selecting;
		}

		/** Locks Oriel; the match's server is ready at once, and the game joins it. */
		bool ReachMatch()
		{
			if (!ReachSelect() || !Flow->LockVanguard(TEXT("oriel"))
				|| !Backend.Answer(TEXT("POST"), TEXT("/v1/me/select/lock"), 200, SelectBody(TEXT("started"), TEXT("oriel"), TEXT("oriel"), MatchId))
				|| !Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 200, ReadyMatch()) || State() != EVeyraClientState::Connecting)
			{
				return false;
			}
			Flow->NotifyWorld(EVeyraClientWorld::Match);
			return State() == EVeyraClientState::InMatch;
		}

		/** From a match that ended to its verified result. */
		bool ReachResults(const FString& Outcome = OutcomeBody(TEXT("ended"), true))
		{
			if (!ReachMatch())
			{
				return false;
			}
			Flow->NotifyMatchPhase(EVeyraMatchPhase::Ended);
			Advance(EndingShowSeconds);
			Flow->NotifyWorld(EVeyraClientWorld::FrontEnd);
			return Backend.Answer(TEXT("GET"), MatchOutcomePath(), 200, Outcome) && State() == EVeyraClientState::Results;
		}

		/** Signs in to find a live match: Reconnect-only. */
		bool ReachReconnectOnly()
		{
			return SignIn() && Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 200, ReadyMatch()) && State() == EVeyraClientState::ReconnectOnly;
		}
	};
}
