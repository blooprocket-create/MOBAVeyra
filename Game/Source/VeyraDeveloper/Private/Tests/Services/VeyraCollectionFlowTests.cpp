// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "Backend/VeyraProgressionProtocol.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Tests/Services/VeyraClientFlowTestRig.h"

namespace VeyraClientFlowTests
{
	// Fixture answers, independent of the committed backend configuration.
	inline FString ProgressionJson(int32 Level, int64 Flux, int64 RefinedFlux)
	{
		return FString::Printf(TEXT("{\"level\":%d,\"levelXp\":40,\"levelNeed\":170,\"lifetimeXp\":1210,\"flux\":%lld,\"refinedFlux\":%lld}"), Level, Flux, RefinedFlux);
	}

	inline FString ProgressionAnswer(int32 Level, int64 Flux, int64 RefinedFlux)
	{
		return FString::Printf(TEXT("{\"progression\":%s}"), *ProgressionJson(Level, Flux, RefinedFlux));
	}

	inline FString CollectionEntryJson(const TCHAR* Vanguard, bool bOwned, const TCHAR* Source, bool bRotation, int32 MasteryLevel)
	{
		return FString::Printf(TEXT("{\"vanguardId\":\"%s\",\"owned\":%s,\"source\":%s,\"rotation\":%s,\"price\":{\"flux\":3000,\"refinedFlux\":550},")
								   TEXT("\"purchasable\":%s,\"mastery\":{\"level\":%d,\"levelPoints\":120,\"levelNeed\":500,\"lifetimePoints\":1620,\"emoteTier\":2}}"),
			Vanguard, bOwned ? TEXT("true") : TEXT("false"), Source ? *FString::Printf(TEXT("\"%s\""), Source) : TEXT("null"), bRotation ? TEXT("true") : TEXT("false"),
			bOwned ? TEXT("false") : TEXT("true"), MasteryLevel);
	}

	/** Cairn owned as the starter, Oriel in rotation, Bryn neither. */
	inline FString CollectionAnswer(bool bBrynOwned = false)
	{
		return FString::Printf(TEXT("{\"vanguards\":[%s,%s,%s]}"), *CollectionEntryJson(TEXT("cairn"), true, TEXT("starter"), false, 3),
			*CollectionEntryJson(TEXT("oriel"), false, nullptr, true, 1), *CollectionEntryJson(TEXT("bryn"), bBrynOwned, bBrynOwned ? TEXT("purchase") : nullptr, false, 1));
	}

	inline FString PurchaseAnswer(int64 Flux)
	{
		return FString::Printf(TEXT("{\"purchase\":{\"purchaseId\":\"x\",\"vanguardId\":\"bryn\",\"currency\":\"flux\",\"price\":3000},\"progression\":%s}"),
			*ProgressionJson(7, Flux, 0));
	}

	inline FString PurchaseIdOf(const FString& Body)
	{
		TSharedPtr<FJsonObject> Root;
		FString Id;
		return FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(Body), Root) && Root->TryGetStringField(TEXT("purchaseId"), Id) ? Id : FString();
	}

	// Veyra.Services.CollectionFlow.*: the account's level and balances, the Collection and purchases
	// (ADR-045 §7), driven through the fake backend as the shell drives them.
	TEST_CLASS(CollectionFlow, "Veyra.Services")
	{
		FClientFlowTestRig Rig;
		FFlowTestBackend& Backend = Rig.Backend;
		TUniquePtr<FVeyraClientFlow>& Flow = Rig.Flow;

		const FVeyraClientSnapshot& Snapshot() const { return Flow->GetSnapshot(); }

		bool ReachCollection()
		{
			return Rig.ReachShell() && Flow->LoadCollection() && Backend.Answer(TEXT("GET"), TEXT("/v1/me/collection"), 200, CollectionAnswer())
				&& Snapshot().Collection.bLoaded;
		}

		TEST_METHOD(TheShellReadsTheLevelAndBalancesAndKeepsThemThroughAFailedRead)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			ASSERT_THAT(IsFalse(Snapshot().Progression.IsSet(), TEXT("before the read")));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/progression"), 200, ProgressionAnswer(7, 1200, 250))));
			ASSERT_THAT(IsTrue(Snapshot().Progression.IsSet() && Snapshot().Progression->Level == 7 && Snapshot().Progression->Flux == 1200
				&& Snapshot().Progression->RefinedFlux == 250 && Snapshot().Progression->LevelNeed == 170));
			// The Collection reads them again; a backend without progression changes nothing, and stops nothing.
			ASSERT_THAT(IsTrue(Flow->LoadCollection()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/progression"), 404, ErrorBody(TEXT("not_found")))));
			ASSERT_THAT(IsTrue(Snapshot().Progression->Level == 7 && !Snapshot().Problem.IsSet()));
		}

		TEST_METHOD(TheCollectionListsEveryReleasedVanguardWithItsMastery)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::PurchaseVanguard), TEXT("nothing to buy before the Collection is read")));
			ASSERT_THAT(IsTrue(Flow->LoadCollection()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/collection"), 200, CollectionAnswer())));
			const TArray<VeyraBackendProtocol::FCollectionEntry>& Vanguards = Snapshot().Collection.Vanguards;
			ASSERT_THAT(AreEqual(3, Vanguards.Num()));
			ASSERT_THAT(IsTrue(Vanguards[0].VanguardId == TEXT("cairn") && Vanguards[0].bOwned && Vanguards[0].Source == TEXT("starter") && !Vanguards[0].bPurchasable
				&& Vanguards[0].Mastery.Level == 3 && Vanguards[0].Mastery.EmoteTier == 2));
			ASSERT_THAT(IsTrue(!Vanguards[1].bOwned && Vanguards[1].bRotation && Vanguards[1].Source.IsEmpty() && Vanguards[1].bPurchasable));
			ASSERT_THAT(IsTrue(Vanguards[2].PriceFlux == 3000 && Vanguards[2].PriceRefinedFlux == 550));
			ASSERT_THAT(IsFalse(Flow->PurchaseVanguard(TEXT("cairn"), VeyraBackendProtocol::ECurrency::Flux), TEXT("an owned Vanguard")));
			ASSERT_THAT(IsFalse(Flow->PurchaseVanguard(TEXT("nobody"), VeyraBackendProtocol::ECurrency::Flux), TEXT("one the Collection does not list")));
		}

		TEST_METHOD(APurchaseUpdatesTheBalancesAndReadsTheCollectionAgain)
		{
			ASSERT_THAT(IsTrue(ReachCollection()));
			ASSERT_THAT(IsTrue(Flow->PurchaseVanguard(TEXT("bryn"), VeyraBackendProtocol::ECurrency::Flux)));
			const FFlowTestBackend::FRequest* Request = Backend.Find(TEXT("POST"), TEXT("/v1/me/purchases"));
			ASSERT_THAT(IsNotNull(Request));
			ASSERT_THAT(IsTrue(Request->Body.Contains(TEXT("\"vanguardId\":\"bryn\"")) && Request->Body.Contains(TEXT("\"currency\":\"flux\""))
				&& PurchaseIdOf(Request->Body).Len() == 36));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/me/purchases"), 200, PurchaseAnswer(1000))));
			ASSERT_THAT(IsTrue(Snapshot().Progression.IsSet() && Snapshot().Progression->Flux == 1000));
			ASSERT_THAT(AreEqual(Snapshot().Collection.Feedback, FString(TEXT("vanguard_purchased"))));
			ASSERT_THAT(AreEqual(Snapshot().Collection.FeedbackVanguard, FString(TEXT("bryn"))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/collection"), 200, CollectionAnswer(/*bBrynOwned*/ true))));
			ASSERT_THAT(IsTrue(Snapshot().Collection.Vanguards[2].bOwned && Snapshot().Collection.Vanguards[2].Source == TEXT("purchase")));
		}

		TEST_METHOD(ARefusedPurchaseShowsInTheCollectionNotAsTheScreensProblem)
		{
			ASSERT_THAT(IsTrue(ReachCollection()));
			ASSERT_THAT(IsTrue(Flow->PurchaseVanguard(TEXT("bryn"), VeyraBackendProtocol::ECurrency::RefinedFlux)));
			ASSERT_THAT(IsTrue(Backend.Find(TEXT("POST"), TEXT("/v1/me/purchases"))->Body.Contains(TEXT("\"currency\":\"refinedFlux\""))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/me/purchases"), 409, ErrorBody(TEXT("insufficient_balance")))));
			ASSERT_THAT(AreEqual(Snapshot().Collection.Feedback, FString(TEXT("insufficient_balance"))));
			ASSERT_THAT(IsFalse(Snapshot().Problem.IsSet()));
			ASSERT_THAT(IsTrue(Backend.Find(TEXT("GET"), TEXT("/v1/me/collection")) != nullptr, TEXT("read again")));
		}

		TEST_METHOD(APurchaseWhoseAnswerWasLostKeepsItsIdWhenAskedAgain)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("VeyraClientFlow: problem in Shell (backend_unreachable)"), ELogVerbosity::Warning,
				EAutomationExpectedMessageFlags::Contains, 1);
			ASSERT_THAT(IsTrue(ReachCollection()));
			ASSERT_THAT(IsTrue(Flow->PurchaseVanguard(TEXT("bryn"), VeyraBackendProtocol::ECurrency::Flux)));
			const FString FirstId = PurchaseIdOf(Backend.Find(TEXT("POST"), TEXT("/v1/me/purchases"))->Body);
			// No answer twice: the flow gives up and shows the problem.
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/me/purchases"), 0)));
			Rig.Advance(1.0);
			ASSERT_THAT(AreEqual(PurchaseIdOf(Backend.Find(TEXT("POST"), TEXT("/v1/me/purchases"))->Body), FirstId, TEXT("its own retry")));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/me/purchases"), 0)));
			ASSERT_THAT(IsTrue(Snapshot().Problem.IsSet()));
			// Asking again for the same Vanguard and currency is the same purchase, which never spends twice.
			ASSERT_THAT(IsTrue(Flow->PurchaseVanguard(TEXT("bryn"), VeyraBackendProtocol::ECurrency::Flux)));
			ASSERT_THAT(AreEqual(PurchaseIdOf(Backend.Find(TEXT("POST"), TEXT("/v1/me/purchases"))->Body), FirstId));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/me/purchases"), 409, ErrorBody(TEXT("insufficient_balance")))));
			// Once answered, the next purchase is a new one.
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/collection"), 200, CollectionAnswer())));
			ASSERT_THAT(IsTrue(Flow->PurchaseVanguard(TEXT("bryn"), VeyraBackendProtocol::ECurrency::Flux)));
			ASSERT_THAT(IsTrue(PurchaseIdOf(Backend.Find(TEXT("POST"), TEXT("/v1/me/purchases"))->Body) != FirstId));
		}

		TEST_METHOD(AResultWithoutItsRewardsWaitsForThem)
		{
			Rig.ShellProgression = ProgressionAnswer(7, 1200, 250);
			ASSERT_THAT(IsTrue(Rig.ReachResults(ScoredOutcomeBody())));
			ASSERT_THAT(IsTrue(Snapshot().RewardsWait == EVeyraRewardsWait::Pending));
			// Still none on the next read; the one after brings them.
			Rig.Advance(FClientFlowTestRig::ResultPollSeconds);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), MatchOutcomePath(), 200, ScoredOutcomeBody())));
			ASSERT_THAT(IsTrue(Snapshot().RewardsWait == EVeyraRewardsWait::Pending));
			Rig.Advance(FClientFlowTestRig::ResultPollSeconds);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), MatchOutcomePath(), 200, RewardedOutcomeBody())));
			ASSERT_THAT(IsTrue(Snapshot().RewardsWait == EVeyraRewardsWait::None && Snapshot().Result.IsSet() && Snapshot().Result->Rewards.IsSet()
				&& Snapshot().Result->Rewards->AccountXP == 181));
			Rig.Advance(FClientFlowTestRig::ResultPollSeconds);
			ASSERT_THAT(IsNull(Backend.Find(TEXT("GET"), MatchOutcomePath()), TEXT("nothing more to ask for")));
		}

		TEST_METHOD(RewardsThatNeverArriveAreLateNotAProblem)
		{
			Rig.ShellProgression = ProgressionAnswer(7, 1200, 250);
			ASSERT_THAT(IsTrue(Rig.ReachResults(ScoredOutcomeBody())));
			const int32 MostReads = FMath::CeilToInt32(FClientFlowTestRig::ResultWaitSeconds / FClientFlowTestRig::ResultPollSeconds) + 1;
			for (int32 Read = 0; Read < MostReads && Snapshot().RewardsWait == EVeyraRewardsWait::Pending; ++Read)
			{
				Rig.Advance(FClientFlowTestRig::ResultPollSeconds);
				ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), MatchOutcomePath(), 503, ErrorBody(TEXT("unavailable")))));
			}
			ASSERT_THAT(IsTrue(Snapshot().RewardsWait == EVeyraRewardsWait::Late));
			ASSERT_THAT(IsTrue(Rig.State() == EVeyraClientState::Results && !Snapshot().Problem.IsSet(), TEXT("the results screen stays")));
		}

		TEST_METHOD(WithoutProgressionAResultWaitsForNoRewards)
		{
			ASSERT_THAT(IsTrue(Rig.ReachResults(ScoredOutcomeBody())));
			ASSERT_THAT(IsTrue(Snapshot().RewardsWait == EVeyraRewardsWait::None));
			Rig.Advance(FClientFlowTestRig::ResultPollSeconds);
			ASSERT_THAT(IsNull(Backend.Find(TEXT("GET"), MatchOutcomePath())));
		}

		TEST_METHOD(TheCollectionIsTheShellsAlone)
		{
			for (const EVeyraClientIntent Intent : { EVeyraClientIntent::LoadCollection, EVeyraClientIntent::PurchaseVanguard })
			{
				ASSERT_THAT(IsTrue(FVeyraClientFlow::IsIntentAllowed(EVeyraClientState::Shell, Intent), LexToString(Intent)));
				for (const EVeyraClientState Elsewhere : { EVeyraClientState::Lobby, EVeyraClientState::MatchFound, EVeyraClientState::Selecting, EVeyraClientState::InMatch,
						 EVeyraClientState::ReconnectOnly })
				{
					ASSERT_THAT(IsFalse(FVeyraClientFlow::IsIntentAllowed(Elsewhere, Intent), LexToString(Intent)));
				}
			}
		}
	};

	// Veyra.Services.ProgressionProtocol.*: the progression routes' answers and a match's rewards, as the
	// client reads them (ADR-045 §7).
	TEST_CLASS(ProgressionProtocol, "Veyra.Services")
	{
		TEST_METHOD(ReadsProgressionAndRefusesWhatIsNotWhole)
		{
			VeyraBackendProtocol::FProgression Progression;
			FString Problem;
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseProgression(ProgressionAnswer(31, 400, 250), Progression, Problem)));
			ASSERT_THAT(IsTrue(Progression.Level == 31 && Progression.LevelXP == 40 && Progression.LifetimeXP == 1210 && Progression.RefinedFlux == 250));
			for (const TCHAR* Bad : { TEXT("{\"progression\":{\"level\":0,\"levelXp\":0,\"levelNeed\":1,\"lifetimeXp\":0,\"flux\":0,\"refinedFlux\":0}}"),
					 TEXT("{\"progression\":{\"level\":2,\"levelXp\":1.5,\"levelNeed\":1,\"lifetimeXp\":0,\"flux\":0,\"refinedFlux\":0}}"),
					 TEXT("{\"progression\":{\"level\":2,\"levelXp\":1,\"levelNeed\":1,\"lifetimeXp\":0,\"flux\":-4,\"refinedFlux\":0}}"), TEXT("{}") })
			{
				ASSERT_THAT(IsFalse(VeyraBackendProtocol::ParseProgression(Bad, Progression, Problem), Bad));
			}
		}

		TEST_METHOD(ReadsTheCollectionAndAPurchase)
		{
			TArray<VeyraBackendProtocol::FCollectionEntry> Vanguards;
			FString Problem;
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseCollection(CollectionAnswer(), Vanguards, Problem) && Vanguards.Num() == 3));
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::ParseCollection(TEXT("{\"vanguards\":[{\"vanguardId\":\"Not An Id\"}]}"), Vanguards, Problem)));
			VeyraBackendProtocol::FProgression After;
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParsePurchase(PurchaseAnswer(1000), After, Problem) && After.Flux == 1000));
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::ParsePurchase(ProgressionAnswer(1, 0, 0), After, Problem), TEXT("no purchase")));
			ASSERT_THAT(AreEqual(VeyraBackendProtocol::BuildPurchaseBody(TEXT("abc-123-def"), TEXT("bryn"), VeyraBackendProtocol::ECurrency::RefinedFlux),
				FString(TEXT("{\"purchaseId\":\"abc-123-def\",\"vanguardId\":\"bryn\",\"currency\":\"refinedFlux\"}"))));
		}

		TEST_METHOD(AMatchOutcomeCarriesItsRewardsOnceAdjudicated)
		{
			const FString Scored = ScoredOutcomeBody();
			const FString WithRewards = Scored.LeftChop(2)
				+ TEXT(",\"rewards\":{\"reason\":null,\"accountXp\":181,\"levelBefore\":6,\"levelAfter\":7,\"flux\":400,\"refinedFlux\":0,")
				  TEXT("\"vanguardId\":\"cairn\",\"masteryPoints\":477,\"masteryBefore\":2,\"masteryAfter\":3}}}");
			VeyraBackendProtocol::FMatchOutcome Outcome;
			FString Problem;
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseMatchOutcome(WithRewards, Outcome, Problem), Problem));
			ASSERT_THAT(IsTrue(Outcome.Rewards.IsSet() && Outcome.Rewards->Reason.IsEmpty() && Outcome.Rewards->AccountXP == 181 && Outcome.Rewards->LevelAfter == 7
				&& Outcome.Rewards->VanguardId == TEXT("cairn") && Outcome.Rewards->MasteryAfter == 3));
			// Null until adjudicated, and absent from an older backend.
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseMatchOutcome(Scored.LeftChop(2) + TEXT(",\"rewards\":null}}"), Outcome, Problem) && !Outcome.Rewards.IsSet()));
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseMatchOutcome(Scored, Outcome, Problem) && !Outcome.Rewards.IsSet()));
			const FString Withheld = Scored.LeftChop(2)
				+ TEXT(",\"rewards\":{\"reason\":\"coop_level\",\"accountXp\":0,\"levelBefore\":12,\"levelAfter\":12,\"flux\":0,\"refinedFlux\":0,")
				  TEXT("\"vanguardId\":\"cairn\",\"masteryPoints\":300,\"masteryBefore\":1,\"masteryAfter\":1}}}");
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseMatchOutcome(Withheld, Outcome, Problem) && Outcome.Rewards->Reason == TEXT("coop_level")));
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::ParseMatchOutcome(Scored.LeftChop(2) + TEXT(",\"rewards\":7}}"), Outcome, Problem)));
		}
	};
}

#endif
