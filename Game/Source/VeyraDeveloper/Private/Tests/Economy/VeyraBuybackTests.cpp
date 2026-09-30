// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Buyback/VeyraBuybackComponent.h"
#include "Buyback/VeyraBuybackRules.h"
#include "Components/ActorTestSpawner.h"
#include "CQTest.h"
#include "Gold/VeyraGoldComponent.h"
#include "Rewards/VeyraEconomyTuning.h"
#include "Rewards/VeyraEconomyTuningSubsystem.h"

namespace VeyraEconomyTests
{
	// Veyra.Economy.BuybackRules.*: a dead Vanguard buys back from 10:00 for Gold that rises with match
	// time and with each purchase, behind a cooldown that starts when it buys (Economy & Progression
	// Bible §15; ADR-020 §3).
	TEST_CLASS(BuybackRules, "Veyra.Economy")
	{
		FActorTestSpawner Spawner;

		// Fixture values: ADR-020's stand-ins, so the arithmetic below reads plainly.
		static constexpr double Opens = 600.0;
		static constexpr double Base = 300.0;
		static constexpr double PerMinute = 25.0;
		static constexpr double PerPurchase = 150.0;
		static constexpr double Cooldown = 240.0;

		static FVeyraBuybackTuning Tuning()
		{
			FVeyraBuybackTuning Out;
			Out.AvailableFromSeconds = Opens;
			Out.BaseCost = Base;
			Out.CostPerMinute = PerMinute;
			Out.CostPerPurchase = PerPurchase;
			Out.CooldownSeconds = Cooldown;
			return Out;
		}

		TEST_METHOD(ItsCostRisesWithEachWholeMinuteAndEachPurchase)
		{
			const FVeyraBuybackTuning Buyback = Tuning();
			ASSERT_THAT(IsTrue(VeyraBuyback::Cost(Opens, 0, Buyback) == Base));
			ASSERT_THAT(IsTrue(VeyraBuyback::Cost(Opens + 59.0, 0, Buyback) == Base, TEXT("a minute counts once it is whole")));
			ASSERT_THAT(IsTrue(VeyraBuyback::Cost(Opens + 120.0, 0, Buyback) == Base + 2.0 * PerMinute));
			ASSERT_THAT(IsTrue(VeyraBuyback::Cost(Opens + 120.0, 2, Buyback) == Base + 2.0 * PerMinute + 2.0 * PerPurchase,
				TEXT("and each earlier buyback adds its surcharge, independently")));
		}

		TEST_METHOD(OnlyTheDeadBuyBackOnceItOpensOffCooldownWithTheGold)
		{
			const FVeyraBuybackTuning Buyback = Tuning();
			const double Plenty = 10000.0;
			const double Now = 50.0;
			ASSERT_THAT(IsTrue(VeyraBuyback::Quote(Opens, Now, /*bDead*/ false, Plenty, 0, 0.0, Buyback).Refusal == EVeyraBuybackRefusal::Alive));
			ASSERT_THAT(IsTrue(VeyraBuyback::Quote(Opens - 1.0, Now, true, Plenty, 0, 0.0, Buyback).Refusal == EVeyraBuybackRefusal::TooEarly));
			ASSERT_THAT(IsTrue(VeyraBuyback::Quote(Opens, Now, true, Plenty, 1, Now + 1.0, Buyback).Refusal == EVeyraBuybackRefusal::CoolingDown));
			ASSERT_THAT(IsTrue(VeyraBuyback::Quote(Opens, Now, true, Base - 1.0, 0, 0.0, Buyback).Refusal == EVeyraBuybackRefusal::NotEnoughGold));
			const FVeyraBuybackQuote Ready = VeyraBuyback::Quote(Opens + 60.0, Now, true, Plenty, 1, Now, Buyback);
			ASSERT_THAT(IsTrue(Ready.Refusal == EVeyraBuybackRefusal::None && Ready.Cost == Base + PerMinute + PerPurchase, TEXT("ready as its cooldown ends")));
		}

		TEST_METHOD(ABuybackTakesItsGoldAndStartsItsCooldownAndARefusalChangesNothing)
		{
			const FVeyraBuybackTuning& Committed = UVeyraEconomyTuningSubsystem::Get().Buyback;
			AActor& Owner = Spawner.SpawnActor<AActor>();
			UVeyraGoldComponent* Gold = NewObject<UVeyraGoldComponent>(&Owner);
			Gold->RegisterComponent();
			UVeyraBuybackComponent* Buyback = NewObject<UVeyraBuybackComponent>(&Owner);
			Buyback->RegisterComponent();
			int32 Broadcasts = 0;
			Buyback->OnBoughtBack.AddLambda([&Broadcasts](double) { ++Broadcasts; });

			const double MatchSeconds = Committed.AvailableFromSeconds;
			const double Cost = VeyraBuyback::Cost(MatchSeconds, 0, Committed);
			const double Now = 5.0;
			ASSERT_THAT(IsTrue(Gold->Grant(Cost, EVeyraGoldReason::Developer)));
			ASSERT_THAT(IsTrue(Buyback->Buy(MatchSeconds, Now, /*bDead*/ false, *Gold) == EVeyraBuybackRefusal::Alive));
			ASSERT_THAT(IsTrue(Buyback->Buy(MatchSeconds, Now, true, *Gold) == EVeyraBuybackRefusal::None));
			ASSERT_THAT(IsTrue(Gold->GetGold() == 0.0 && Buyback->GetPurchases() == 1 && Broadcasts == 1));
			ASSERT_THAT(IsTrue(Buyback->GetReadyAt() == Now + Committed.CooldownSeconds));

			ASSERT_THAT(IsTrue(Gold->Grant(VeyraBuyback::Cost(MatchSeconds, 1, Committed), EVeyraGoldReason::Developer)));
			ASSERT_THAT(IsTrue(Buyback->Buy(MatchSeconds, Now + 1.0, true, *Gold) == EVeyraBuybackRefusal::CoolingDown));
			ASSERT_THAT(IsTrue(Buyback->GetPurchases() == 1 && Broadcasts == 1 && Gold->GetGold() == VeyraBuyback::Cost(MatchSeconds, 1, Committed),
				TEXT("a refusal takes nothing")));
		}
	};
}
