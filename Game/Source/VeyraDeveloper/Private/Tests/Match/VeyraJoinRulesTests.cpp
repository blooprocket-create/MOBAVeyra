// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Engine/Engine.h"
#include "Greybox/VeyraGreyboxLayout.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "Tuning/VeyraTuning.h"
#include "VeyraJoinRules.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraMatchTests
{
	// Veyra.Match.JoinRules.*: a client joins only with the same tuning as the server (ADR-006 §6).
	TEST_CLASS(JoinRules, "Veyra.Match")
	{
		const FBlake3Hash ServerHash = FBlake3::HashBuffer(TEXT("server"), sizeof(TEXT("server")));
		const FBlake3Hash OtherHash = FBlake3::HashBuffer(TEXT("other"), sizeof(TEXT("other")));

		TEST_METHOD(AcceptsTheSameHash)
		{
			const FString Options = TEXT("?Name=Player") + FString(TEXT("?")) + VeyraJoinRules::MakeTuningHashOption(ServerHash);
			ASSERT_THAT(IsTrue(VeyraJoinRules::CheckTuningHash(Options, ServerHash).IsEmpty()));
		}

		TEST_METHOD(AcceptsTheHashInEitherCase)
		{
			const FString Options = FString::Printf(TEXT("?%s=%s"), VeyraJoinRules::TuningHashOption, *LexToString(ServerHash).ToUpper());
			ASSERT_THAT(IsTrue(VeyraJoinRules::CheckTuningHash(Options, ServerHash).IsEmpty()));
		}

		TEST_METHOD(RefusesADifferentHash)
		{
			const FString Options = TEXT("?") + VeyraJoinRules::MakeTuningHashOption(OtherHash);
			ASSERT_THAT(IsFalse(VeyraJoinRules::CheckTuningHash(Options, ServerHash).IsEmpty()));
		}

		TEST_METHOD(RefusesAMissingHash)
		{
			ASSERT_THAT(IsFalse(VeyraJoinRules::CheckTuningHash(TEXT("?Name=Player"), ServerHash).IsEmpty()));
			ASSERT_THAT(IsFalse(VeyraJoinRules::CheckTuningHash(FString::Printf(TEXT("?%s="), VeyraJoinRules::TuningHashOption), ServerHash).IsEmpty()));
		}

		TEST_METHOD(DevelopmentBuildsAcceptDirectConnections)
		{
			// Shipping refuses them without an assignment (ADR-007 §9); tests run in development builds.
			ASSERT_THAT(IsTrue(VeyraJoinRules::CheckDirectConnect(/*bServerHasAssignment*/ false).IsEmpty()));
			ASSERT_THAT(IsTrue(VeyraJoinRules::CheckDirectConnect(/*bServerHasAssignment*/ true).IsEmpty()));
		}

		TEST_METHOD(CompositeHashCoversEveryLoadedDomain)
		{
			const TMap<FString, FBlake3Hash> Domains = VeyraTuning::GetLoadedDomainHashes();
			const FBlake3Hash* Combat = Domains.Find(UVeyraCombatTuningSubsystem::Domain);
			const FBlake3Hash* Match = Domains.Find(UVeyraMatchTuningSubsystem::Domain);
			ASSERT_THAT(IsTrue(Combat && *Combat == GEngine->GetEngineSubsystem<UVeyraCombatTuningSubsystem>()->GetDocumentHash()));
			ASSERT_THAT(IsTrue(Match && *Match == GEngine->GetEngineSubsystem<UVeyraMatchTuningSubsystem>()->GetDocumentHash()));

			TArray<FString> Names;
			Domains.GetKeys(Names);
			Names.Sort();
			FString Lines;
			for (const FString& Name : Names)
			{
				Lines += Name + TEXT("=") + LexToString(Domains[Name]) + TEXT("\n");
			}
			const FTCHARToUTF8 Utf8(*Lines);
			ASSERT_THAT(IsTrue(VeyraTuning::GetCompositeHash() == FBlake3::HashBuffer(Utf8.Get(), Utf8.Length())));
		}
	};

	// Veyra.Match.MatchTuning.*: the committed Match.json loads.
	TEST_CLASS(MatchTuning, "Veyra.Match")
	{
		TEST_METHOD(CommittedFileLoads)
		{
			ASSERT_THAT(IsTrue(GEngine->GetEngineSubsystem<UVeyraMatchTuningSubsystem>()->IsLoaded()));
		}

		TEST_METHOD(ATeamVotePassesOnAMajorityOfAFullTeam)
		{
			FVeyraMatchTuning Tuning = UVeyraMatchTuningSubsystem::Get();
			ASSERT_THAT(IsTrue(UVeyraMatchTuningSubsystem::ValidateRules(Tuning).IsEmpty()));
			Tuning.Votes.Surrender.YesVotes = Tuning.Teams.MaxTeamSize / 2;
			ASSERT_THAT(IsTrue(UVeyraMatchTuningSubsystem::ValidateRules(Tuning).ContainsByPredicate(
				[](const FString& Error) { return Error.StartsWith(TEXT("/votes/surrender/yesVotes")); }), TEXT("half a team is no majority")));
			Tuning.Votes.Surrender.YesVotes = Tuning.Teams.MaxTeamSize + 1;
			ASSERT_THAT(IsFalse(UVeyraMatchTuningSubsystem::ValidateRules(Tuning).IsEmpty(), TEXT("nor more votes than a team holds")));
		}
	};

	// Veyra.Match.GreyboxLayout.*: the grey-box test map's layout file is valid.
	TEST_CLASS(GreyboxLayout, "Veyra.Match")
	{
		TEST_METHOD(CommittedLayoutLoads)
		{
			FVeyraGreyboxLayout Layout;
			const VeyraTuning::FErrors Errors = VeyraGreybox::LoadLayout(Layout);
			for (const FString& Error : Errors)
			{
				TestRunner->AddError(Error);
			}
			ASSERT_THAT(IsTrue(Errors.IsEmpty()));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
