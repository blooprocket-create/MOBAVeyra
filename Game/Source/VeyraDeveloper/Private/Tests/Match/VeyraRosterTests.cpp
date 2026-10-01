// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Hash/VeyraSha256.h"
#include "Join/VeyraMatchAssignment.h"
#include "Join/VeyraMatchHostSubsystem.h"
#include "Join/VeyraMatchRoster.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "Rules/VeyraMatchRules.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraJoinRules.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraMatchTests
{
	FVeyraContentId RosterContentId(const TCHAR* Text)
	{
		return FVeyraContentId::FromText(Text).GetValue();
	}

	/** A two-participant casual assignment whose tickets are "vjt_one" and "vjt_two", as Cairn and Oriel. */
	FVeyraMatchAssignment TwoParticipantAssignment()
	{
		FVeyraMatchAssignment Assignment;
		Assignment.MatchId = TEXT("match-1");
		Assignment.Mode = RosterContentId(TEXT("casual_select"));
		Assignment.Participants.Add({ TEXT("account-1"), TEXT("DevOne"), EVeyraTeam::A, VeyraHash::Sha256Hex(TEXT("vjt_one")), RosterContentId(TEXT("cairn")) });
		Assignment.Participants.Add({ TEXT("account-2"), TEXT("DevTwo"), EVeyraTeam::B, VeyraHash::Sha256Hex(TEXT("vjt_two")), RosterContentId(TEXT("oriel")) });
		return Assignment;
	}

	FString OptionsWithTicket(const TCHAR* Ticket)
	{
		return FString(TEXT("?Name=Someone?VeyraTuning=abc?")) + VeyraJoinRules::MakeTicketOption(Ticket) + TEXT("?Other=1");
	}

	// Veyra.Match.JoinTicket.*: a hosted match admits its roster by join ticket, once each
	// (ADR-007 §4).
	TEST_CLASS(JoinTicket, "Veyra.Match")
	{
		TEST_METHOD(AcceptsARosteredTicketAmongOtherOptions)
		{
			const FVeyraMatchRoster Roster(TwoParticipantAssignment());
			FString AccountId;
			ASSERT_THAT(IsTrue(VeyraJoinRules::CheckTicket(OptionsWithTicket(TEXT("vjt_two")), Roster, AccountId).IsEmpty()));
			ASSERT_THAT(AreEqual(AccountId, FString(TEXT("account-2"))));
		}

		TEST_METHOD(RefusesAMissingOrUnknownTicket)
		{
			const FVeyraMatchRoster Roster(TwoParticipantAssignment());
			FString AccountId;
			ASSERT_THAT(IsFalse(VeyraJoinRules::CheckTicket(TEXT("?Name=Someone?VeyraTuning=abc"), Roster, AccountId).IsEmpty()));
			ASSERT_THAT(IsFalse(VeyraJoinRules::CheckTicket(OptionsWithTicket(TEXT("vjt_three")), Roster, AccountId).IsEmpty()));
			// The hash of a real ticket is not a ticket.
			const FString HashAsTicket = VeyraHash::Sha256Hex(TEXT("vjt_one"));
			ASSERT_THAT(IsFalse(VeyraJoinRules::CheckTicket(OptionsWithTicket(*HashAsTicket), Roster, AccountId).IsEmpty()));
			ASSERT_THAT(IsTrue(AccountId.IsEmpty()));
		}

		TEST_METHOD(RefusesAConnectedParticipantAndAdmitsOneReturning)
		{
			FVeyraMatchRoster Roster(TwoParticipantAssignment());
			FString AccountId;
			Roster.MarkConnected(TEXT("account-1"));
			const FString WhileConnected = VeyraJoinRules::CheckTicket(OptionsWithTicket(TEXT("vjt_one")), Roster, AccountId);
			ASSERT_THAT(IsTrue(WhileConnected.Contains(TEXT("already connected"))));
			Roster.MarkDisconnected(TEXT("account-1"));
			ASSERT_THAT(IsTrue(VeyraJoinRules::CheckTicket(OptionsWithTicket(TEXT("vjt_one")), Roster, AccountId).IsEmpty(), TEXT("one who left may return")));
			ASSERT_THAT(AreEqual(AccountId, FString(TEXT("account-1"))));
		}

		TEST_METHOD(RefusalsNeverRepeatTheTicket)
		{
			FVeyraMatchRoster Roster(TwoParticipantAssignment());
			Roster.MarkConnected(TEXT("account-2"));
			FString AccountId;
			for (const TCHAR* Ticket : { TEXT("vjt_two"), TEXT("vjt_unknown") })
			{
				ASSERT_THAT(IsFalse(VeyraJoinRules::CheckTicket(OptionsWithTicket(Ticket), Roster, AccountId).Contains(TEXT("vjt_"))));
			}
		}
	};

	// Veyra.Match.Roster.*: who has joined and who is connected, and the result the roster reports.
	TEST_CLASS(Roster, "Veyra.Match")
	{
		TEST_METHOD(TracksJoinedAndConnected)
		{
			FVeyraMatchRoster Roster(TwoParticipantAssignment());
			ASSERT_THAT(AreEqual(Roster.NumConnected(), 0));
			Roster.MarkConnected(TEXT("account-1"));
			Roster.MarkConnected(TEXT("account-2"));
			Roster.MarkDisconnected(TEXT("account-2"));
			Roster.MarkConnected(TEXT("stranger"));
			ASSERT_THAT(AreEqual(Roster.NumConnected(), 1));

			const TArray<FVeyraParticipantResult> Results = Roster.BuildParticipantResults();
			ASSERT_THAT(AreEqual(Results.Num(), 2));
			ASSERT_THAT(IsTrue(Results[0].AccountId == TEXT("account-1") && Results[0].bJoined && Results[0].bConnectedAtEnd));
			ASSERT_THAT(IsTrue(Results[1].AccountId == TEXT("account-2") && Results[1].bJoined && !Results[1].bConnectedAtEnd));
		}

		TEST_METHOD(FindsParticipantsByTicketHashAndAccount)
		{
			const FVeyraMatchRoster Roster(TwoParticipantAssignment());
			const FVeyraAssignedParticipant* ByHash = Roster.FindByTicketHash(VeyraHash::Sha256Hex(TEXT("vjt_two")));
			ASSERT_THAT(IsTrue(ByHash && ByHash->AccountId == TEXT("account-2") && ByHash->Side == EVeyraTeam::B));
			ASSERT_THAT(IsTrue(Roster.FindByAccount(TEXT("account-1")) != nullptr));
			ASSERT_THAT(IsTrue(Roster.FindByAccount(TEXT("ACCOUNT-1")) == nullptr));
		}
	};

	// Veyra.Match.HostedAssignment.*: a server takes only an assignment this build can host.
	TEST_CLASS(HostedAssignment, "Veyra.Match")
	{
		UVeyraMatchHostSubsystem* Host = nullptr;

		BEFORE_EACH()
		{
			Host = UVeyraMatchHostSubsystem::Get();
			ASSERT_THAT(IsNotNull(Host));
		}

		AFTER_EACH()
		{
			Host->ClearAssignment();
		}

		bool IsRefused(const FVeyraMatchAssignment& Assignment, const TCHAR* Fragment)
		{
			const TArray<FString> Problems = Host->SetAssignment(Assignment);
			const bool bFound = Problems.ContainsByPredicate([Fragment](const FString& Problem) { return Problem.Contains(Fragment); });
			return bFound && !Host->GetAssignment().IsSet();
		}

		TEST_METHOD(TakesAValidAssignment)
		{
			ASSERT_THAT(IsTrue(Host->SetAssignment(TwoParticipantAssignment()).IsEmpty()));
			ASSERT_THAT(IsTrue(Host->GetAssignment().IsSet() && Host->GetAssignment()->MatchId == TEXT("match-1")));
		}

		TEST_METHOD(RefusesMalformedAssignments)
		{
			FVeyraMatchAssignment NoId = TwoParticipantAssignment();
			NoId.MatchId.Reset();
			ASSERT_THAT(IsTrue(IsRefused(NoId, TEXT("no match ID"))));

			FVeyraMatchAssignment Empty = TwoParticipantAssignment();
			Empty.Participants.Reset();
			ASSERT_THAT(IsTrue(IsRefused(Empty, TEXT("no participants"))));

			FVeyraMatchAssignment SameAccount = TwoParticipantAssignment();
			SameAccount.Participants[1].AccountId = SameAccount.Participants[0].AccountId;
			ASSERT_THAT(IsTrue(IsRefused(SameAccount, TEXT("appears twice"))));

			FVeyraMatchAssignment NoSide = TwoParticipantAssignment();
			NoSide.Participants[0].Side = EVeyraTeam::None;
			ASSERT_THAT(IsTrue(IsRefused(NoSide, TEXT("side must be A or B"))));

			FVeyraMatchAssignment UpperHash = TwoParticipantAssignment();
			UpperHash.Participants[0].TicketHash = UpperHash.Participants[0].TicketHash.ToUpper();
			ASSERT_THAT(IsTrue(IsRefused(UpperHash, TEXT("lowercase hex SHA-256"))));

			FVeyraMatchAssignment ShortHash = TwoParticipantAssignment();
			ShortHash.Participants[0].TicketHash.LeftChopInline(1);
			ASSERT_THAT(IsTrue(IsRefused(ShortHash, TEXT("lowercase hex SHA-256"))));
		}

		TEST_METHOD(RefusesASideLargerThanTheTeam)
		{
			FVeyraMatchAssignment Crowded = TwoParticipantAssignment();
			Crowded.Participants.Reset();
			const int32 MaxTeamSize = UVeyraMatchTuningSubsystem::Get().Teams.MaxTeamSize;
			for (int32 Index = 0; Index <= MaxTeamSize; ++Index)
			{
				const FString Ticket = FString::Printf(TEXT("vjt_%d"), Index);
				Crowded.Participants.Add({ FString::Printf(TEXT("account-%d"), Index), TEXT("Dev"), EVeyraTeam::A, VeyraHash::Sha256Hex(Ticket),
					RosterContentId(TEXT("cairn")) });
			}
			ASSERT_THAT(IsTrue(IsRefused(Crowded, TEXT("teams.maxTeamSize"))));
		}

		TEST_METHOD(RefusesAModeOrVanguardItCannotHost)
		{
			FVeyraMatchAssignment NoMode = TwoParticipantAssignment();
			NoMode.Mode = FVeyraContentId();
			ASSERT_THAT(IsTrue(IsRefused(NoMode, TEXT("no mode"))));

			FVeyraMatchAssignment NoVanguard = TwoParticipantAssignment();
			NoVanguard.Participants[1].VanguardId = FVeyraContentId();
			ASSERT_THAT(IsTrue(IsRefused(NoVanguard, TEXT("Vanguard is missing"))));

			FVeyraMatchAssignment Unknown = TwoParticipantAssignment();
			Unknown.Participants[0].VanguardId = RosterContentId(TEXT("no_such_vanguard"));
			ASSERT_THAT(IsTrue(IsRefused(Unknown, TEXT("does not define no_such_vanguard"))));
		}

		TEST_METHOD(APracticeMatchHasItsHostOnTheRoster)
		{
			FVeyraMatchAssignment Practice = TwoParticipantAssignment();
			Practice.Rules = EVeyraMatchRules::Practice;
			Practice.Participants.SetNum(1);
			Practice.HostAccountId = TEXT("account-1");
			ASSERT_THAT(IsTrue(Host->SetAssignment(Practice).IsEmpty()));
			Host->ClearAssignment();

			Practice.HostAccountId = TEXT("account-2");
			ASSERT_THAT(IsTrue(IsRefused(Practice, TEXT("host must be on its roster"))));
			Practice.HostAccountId.Reset();
			ASSERT_THAT(IsTrue(IsRefused(Practice, TEXT("host must be on its roster"))));

			FVeyraMatchAssignment HostedStandard = TwoParticipantAssignment();
			HostedStandard.HostAccountId = TEXT("account-1");
			ASSERT_THAT(IsTrue(IsRefused(HostedStandard, TEXT("only a practice or custom match has a host"))));
		}

		TEST_METHOD(OnlyAPracticeMatchHasBotsAndTheyTakePlaces)
		{
			FVeyraMatchAssignment Practice = TwoParticipantAssignment();
			Practice.Rules = EVeyraMatchRules::Practice;
			Practice.Participants.SetNum(1);
			Practice.HostAccountId = TEXT("account-1");
			Practice.Bots = { { EVeyraTeam::B, RosterContentId(TEXT("cairn")) }, { EVeyraTeam::B, RosterContentId(TEXT("bryn")) } };
			ASSERT_THAT(IsTrue(Host->SetAssignment(Practice).IsEmpty()));
			ASSERT_THAT(AreEqual(Host->GetAssignment()->Bots.Num(), 2));
			Host->ClearAssignment();

			FVeyraMatchAssignment NoSide = Practice;
			NoSide.Bots[0].Side = EVeyraTeam::None;
			ASSERT_THAT(IsTrue(IsRefused(NoSide, TEXT("bot 0: the side must be A or B"))));

			FVeyraMatchAssignment Unknown = Practice;
			Unknown.Bots[1].VanguardId = RosterContentId(TEXT("no_such_vanguard"));
			ASSERT_THAT(IsTrue(IsRefused(Unknown, TEXT("bot 1: "))));

			// A bot takes a place on its side like a player: the host and a full side of bots is too many.
			FVeyraMatchAssignment Crowded = Practice;
			Crowded.Bots.Reset();
			for (int32 Index = 0; Index < UVeyraMatchTuningSubsystem::Get().Teams.MaxTeamSize; ++Index)
			{
				Crowded.Bots.Add({ EVeyraTeam::A, RosterContentId(TEXT("cairn")) });
			}
			ASSERT_THAT(IsTrue(IsRefused(Crowded, TEXT("teams.maxTeamSize"))));

			FVeyraMatchAssignment StandardWithBots = TwoParticipantAssignment();
			StandardWithBots.Bots = Practice.Bots;
			ASSERT_THAT(IsTrue(IsRefused(StandardWithBots, TEXT("only a practice or custom match has bots beside humans"))));

			// A co-op match: its humans on one side, its enemy team on the other (ADR-038 §4).
			FVeyraMatchAssignment Coop = TwoParticipantAssignment();
			Coop.Participants.SetNum(1);
			Coop.Bots = Practice.Bots;
			for (FVeyraAssignedBot& Bot : Coop.Bots)
			{
				Bot.Side = Coop.Participants[0].Side == EVeyraTeam::A ? EVeyraTeam::B : EVeyraTeam::A;
			}
			ASSERT_THAT(IsTrue(Host->SetAssignment(Coop).IsEmpty(), TEXT("a co-op match's enemy team is welcome")));
			Host->ClearAssignment();
		}

		TEST_METHOD(ACustomMatchHasItsHostBotsOnEitherSideAndItsSettings)
		{
			FVeyraMatchAssignment Custom = TwoParticipantAssignment();
			Custom.Rules = EVeyraMatchRules::Custom;
			Custom.HostAccountId = TEXT("account-2");
			Custom.Bots = { { EVeyraTeam::A, RosterContentId(TEXT("oriel")) }, { EVeyraTeam::B, RosterContentId(TEXT("bryn")) } };
			FVeyraCustomSettings Settings;
			Settings.bVictoryEnabled = true;
			Settings.StartingGold = 2500.0;
			Custom.Custom = Settings;
			ASSERT_THAT(IsTrue(Host->SetAssignment(Custom).IsEmpty()));
			ASSERT_THAT(IsTrue(Host->GetAssignment()->Custom.IsSet() && Host->GetAssignment()->Custom->StartingGold.Get(0.0) == 2500.0));
			Host->ClearAssignment();

			FVeyraMatchAssignment Unset = Custom;
			Unset.Custom.Reset();
			ASSERT_THAT(IsTrue(IsRefused(Unset, TEXT("session settings"))));
			FVeyraMatchAssignment StandardWithSettings = TwoParticipantAssignment();
			StandardWithSettings.Custom = Settings;
			ASSERT_THAT(IsTrue(IsRefused(StandardWithSettings, TEXT("session settings"))));
			FVeyraMatchAssignment Negative = Custom;
			Negative.Custom->StartingGold = -1.0;
			ASSERT_THAT(IsTrue(IsRefused(Negative, TEXT("starting Gold"))));
		}
	};

	// Veyra.Match.MatchRules.*: who may end a custom match, which Vanguards a server hosts (ADR-010
	// §6–7), who wins, and how long the dead wait (Economy & Progression Bible §14).
	TEST_CLASS(MatchRules, "Veyra.Match")
	{
		TEST_METHOD(OnlyThePracticeHostEndsAPracticeMatch)
		{
			using namespace VeyraMatchRules;
			ASSERT_THAT(IsTrue(CheckEndCustomMatch(EVeyraMatchRules::Practice, EVeyraMatchPhase::Live, true) == EVeyraEndCustomMatchRefusal::None));
			ASSERT_THAT(IsTrue(CheckEndCustomMatch(EVeyraMatchRules::Practice, EVeyraMatchPhase::Preparation, true) == EVeyraEndCustomMatchRefusal::None));
			ASSERT_THAT(IsTrue(CheckEndCustomMatch(EVeyraMatchRules::Practice, EVeyraMatchPhase::Live, false) == EVeyraEndCustomMatchRefusal::NotHost));
			ASSERT_THAT(IsTrue(CheckEndCustomMatch(EVeyraMatchRules::Practice, EVeyraMatchPhase::Ended, true) == EVeyraEndCustomMatchRefusal::AlreadyEnded));
			ASSERT_THAT(IsTrue(CheckEndCustomMatch(EVeyraMatchRules::Standard, EVeyraMatchPhase::Live, true) == EVeyraEndCustomMatchRefusal::NotCustomMatch));
			ASSERT_THAT(IsTrue(CheckEndCustomMatch(EVeyraMatchRules::Custom, EVeyraMatchPhase::Live, true) == EVeyraEndCustomMatchRefusal::None, TEXT("a custom match's host too")));
			ASSERT_THAT(IsTrue(CheckEndCustomMatch(EVeyraMatchRules::Custom, EVeyraMatchPhase::Live, false) == EVeyraEndCustomMatchRefusal::NotHost));
		}

		TEST_METHOD(ShippingHostsOnlyPlayableVanguards)
		{
			using namespace VeyraMatchRules;
			const FVeyraContentId Cairn = RosterContentId(TEXT("cairn"));
			const FVeyraContentId TestVanguard = RosterContentId(TEXT("test_vanguard"));
			const FVeyraVanguardDefinition* Playable = UVeyraVanguardsTuningSubsystem::FindVanguard(Cairn);
			const FVeyraVanguardDefinition* Developer = UVeyraVanguardsTuningSubsystem::FindVanguard(TestVanguard);
			ASSERT_THAT(IsNotNull(Playable));
			ASSERT_THAT(IsNotNull(Developer));
			ASSERT_THAT(IsTrue(CheckAssignedVanguard(Cairn, Playable, /*bShipping*/ true).IsEmpty()));
			ASSERT_THAT(IsTrue(CheckAssignedVanguard(TestVanguard, Developer, /*bShipping*/ false).IsEmpty()));
			ASSERT_THAT(IsTrue(CheckAssignedVanguard(TestVanguard, Developer, /*bShipping*/ true).Contains(TEXT("developer Vanguard"))));
			ASSERT_THAT(IsFalse(CheckAssignedVanguard(RosterContentId(TEXT("no_such_vanguard")), nullptr, /*bShipping*/ false).IsEmpty()));
		}

		TEST_METHOD(StartingFluxSpellsAreRosterSpellsOrEmptyNoneTwice)
		{
			using namespace VeyraMatchRules;
			const TArray<FVeyraContentId> Roster = { RosterContentId(TEXT("blink")), RosterContentId(TEXT("mend")) };
			ASSERT_THAT(IsTrue(CheckAssignedFluxSpells({ RosterContentId(TEXT("blink")), RosterContentId(TEXT("mend")) }, Roster).IsEmpty()));
			ASSERT_THAT(IsTrue(CheckAssignedFluxSpells({ FVeyraContentId(), RosterContentId(TEXT("mend")) }, Roster).IsEmpty(), TEXT("an empty slot")));
			ASSERT_THAT(IsTrue(CheckAssignedFluxSpells({}, Roster).IsEmpty(), TEXT("none at all")));
			ASSERT_THAT(IsTrue(CheckAssignedFluxSpells({ RosterContentId(TEXT("scorch")) }, Roster).Contains(TEXT("roster"))));
			ASSERT_THAT(IsTrue(CheckAssignedFluxSpells({ RosterContentId(TEXT("mend")), RosterContentId(TEXT("mend")) }, Roster).Contains(TEXT("two slots"))));
			ASSERT_THAT(IsFalse(CheckAssignedFluxSpells({ FVeyraContentId(), FVeyraContentId(), FVeyraContentId() }, Roster).IsEmpty(), TEXT("more than two slots")));
		}

		TEST_METHOD(APrimeWellWinsOnlyALiveStandardMatch)
		{
			using namespace VeyraMatchRules;
			ASSERT_THAT(IsTrue(DoesPrimeWellWin(HasVictory(EVeyraMatchRules::Standard, {}), EVeyraMatchPhase::Live)));
			ASSERT_THAT(IsFalse(DoesPrimeWellWin(HasVictory(EVeyraMatchRules::Practice, {}), EVeyraMatchPhase::Live), TEXT("practice has no victory condition")));
			ASSERT_THAT(IsFalse(DoesPrimeWellWin(HasVictory(EVeyraMatchRules::Standard, {}), EVeyraMatchPhase::Ended), TEXT("an ended match stays ended")));
			FVeyraCustomSettings Custom;
			ASSERT_THAT(IsFalse(HasVictory(EVeyraMatchRules::Custom, Custom), TEXT("a custom match with victory off")));
			Custom.bVictoryEnabled = true;
			ASSERT_THAT(IsTrue(HasVictory(EVeyraMatchRules::Custom, Custom), TEXT("and with it on")));
			ASSERT_THAT(IsTrue(HasHost(EVeyraMatchRules::Custom) && HasHost(EVeyraMatchRules::Practice) && !HasHost(EVeyraMatchRules::Standard)));
			ASSERT_THAT(IsTrue(AllowsBuyback(EVeyraMatchRules::Custom) && AllowsBuyback(EVeyraMatchRules::Standard) && !AllowsBuyback(EVeyraMatchRules::Practice)));
		}

		TEST_METHOD(OnlyAWonMatchHasAWinner)
		{
			using namespace VeyraMatchResults;
			ASSERT_THAT(IsTrue(IsWinnerConsistent(EVeyraMatchEndReason::PrimeWellDestroyed, EVeyraTeam::B)));
			ASSERT_THAT(IsFalse(IsWinnerConsistent(EVeyraMatchEndReason::PrimeWellDestroyed, EVeyraTeam::None)));
			ASSERT_THAT(IsTrue(IsWinnerConsistent(EVeyraMatchEndReason::HostEnded, EVeyraTeam::None)));
			ASSERT_THAT(IsFalse(IsWinnerConsistent(EVeyraMatchEndReason::DeveloperRequest, EVeyraTeam::A)));
		}

		TEST_METHOD(TheRespawnTimerGrowsWithLevelAndMatchTime)
		{
			// Fixture values in the curve's shape (Economy & Progression Bible §14).
			using namespace VeyraMatchRules;
			FVeyraRespawnTuning Respawn;
			Respawn.SecondsByLevel = { 6.0, 10.0, 20.0 };
			Respawn.Elapsed.StartSeconds = 600.0;
			Respawn.Elapsed.FractionPerMinute = 0.1;
			Respawn.Elapsed.MaxFraction = 0.25;
			ASSERT_THAT(IsTrue(RespawnDelaySeconds(1, 0.0, Respawn) == 6.0));
			ASSERT_THAT(IsTrue(RespawnDelaySeconds(2, 600.0, Respawn) == 10.0, TEXT("nothing added until the curve's start")));
			ASSERT_THAT(IsTrue(RespawnDelaySeconds(18, 0.0, Respawn) == 20.0, TEXT("a level past the curve uses its last entry")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(RespawnDelaySeconds(2, 720.0, Respawn), 12.0), TEXT("two minutes past: +20%")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(RespawnDelaySeconds(2, 3600.0, Respawn), 12.5), TEXT("capped at +25%")));
		}

		TEST_METHOD(TheCommittedRespawnCurveCoversEveryLevel)
		{
			// Each level up to the cap has its own timer, rising with the level.
			const TArray<double>& Seconds = UVeyraMatchTuningSubsystem::Get().Respawn.SecondsByLevel;
			ASSERT_THAT(AreEqual(UVeyraProgressionTuningSubsystem::Get().MaxLevel, Seconds.Num()));
			for (int32 Index = 1; Index < Seconds.Num(); ++Index)
			{
				ASSERT_THAT(IsTrue(Seconds[Index] >= Seconds[Index - 1], FString::Printf(TEXT("/respawn/secondsByLevel/%d"), Index)));
			}
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
