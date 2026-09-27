// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Hash/VeyraSha256.h"
#include "Join/VeyraMatchHostSubsystem.h"
#include "Join/VeyraMatchRoster.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "VeyraJoinRules.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraMatchTests
{
	/** A two-participant assignment whose tickets are "vjt_one" and "vjt_two". */
	FVeyraMatchAssignment TwoParticipantAssignment()
	{
		FVeyraMatchAssignment Assignment;
		Assignment.MatchId = TEXT("match-1");
		Assignment.Participants.Add({ TEXT("account-1"), TEXT("DevOne"), EVeyraTeam::A, VeyraHash::Sha256Hex(TEXT("vjt_one")) });
		Assignment.Participants.Add({ TEXT("account-2"), TEXT("DevTwo"), EVeyraTeam::B, VeyraHash::Sha256Hex(TEXT("vjt_two")) });
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

		TEST_METHOD(RefusesAConnectedOrReturningParticipant)
		{
			FVeyraMatchRoster Roster(TwoParticipantAssignment());
			FString AccountId;
			Roster.MarkConnected(TEXT("account-1"));
			const FString WhileConnected = VeyraJoinRules::CheckTicket(OptionsWithTicket(TEXT("vjt_one")), Roster, AccountId);
			ASSERT_THAT(IsTrue(WhileConnected.Contains(TEXT("already connected"))));
			Roster.MarkDisconnected(TEXT("account-1"));
			const FString AfterLeaving = VeyraJoinRules::CheckTicket(OptionsWithTicket(TEXT("vjt_one")), Roster, AccountId);
			ASSERT_THAT(IsTrue(AfterLeaving.Contains(TEXT("rejoining waits for reconnect"))));
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
				Crowded.Participants.Add({ FString::Printf(TEXT("account-%d"), Index), TEXT("Dev"), EVeyraTeam::A, VeyraHash::Sha256Hex(Ticket) });
			}
			ASSERT_THAT(IsTrue(IsRefused(Crowded, TEXT("teams.maxTeamSize"))));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
