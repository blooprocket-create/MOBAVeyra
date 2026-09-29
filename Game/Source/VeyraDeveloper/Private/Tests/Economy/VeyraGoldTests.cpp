// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Components/ActorTestSpawner.h"
#include "CQTest.h"
#include "Gold/VeyraGoldComponent.h"
#include "VeyraPlayerState.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraEconomyTests
{
	// Veyra.Economy.GoldLedger.*: spending, holding and refunding Gold for purchases (Economy &
	// Progression Bible §11–§12, §16; ADR-012 §4).
	TEST_CLASS(GoldLedger, "Veyra.Economy")
	{
		// Fixture values.
		static constexpr double Starting = 500.0;
		static constexpr double Price = 300.0;
		static constexpr double Held = 150.0;

		FActorTestSpawner Spawner;
		UVeyraGoldComponent* Purse = nullptr;

		BEFORE_EACH()
		{
			Purse = Spawner.SpawnActor<AVeyraPlayerState>().FindComponentByClass<UVeyraGoldComponent>();
			ASSERT_THAT(IsNotNull(Purse));
			ASSERT_THAT(IsTrue(Purse->Grant(Starting, EVeyraGoldReason::Starting)));
		}

		TEST_METHOD(SpendingNeverGoesIntoDebt)
		{
			ASSERT_THAT(IsFalse(Purse->Spend(Starting + 1.0), TEXT("no negative balance (§11.1)")));
			ASSERT_THAT(IsFalse(Purse->Spend(-1.0)));
			ASSERT_THAT(IsTrue(Purse->GetGold() == Starting, TEXT("a refused purchase changes nothing")));
			ASSERT_THAT(IsTrue(Purse->Spend(Price)));
			ASSERT_THAT(IsTrue(Purse->GetGold() == Starting - Price));
		}

		TEST_METHOD(AHeldPurchaseIsSpentAndACancelledOneComesBackWhole)
		{
			const TOptional<int32> Hold = Purse->Hold(Held);
			ASSERT_THAT(IsTrue(Hold.IsSet() && Purse->GetGold() == Starting - Held && Purse->GetHolds().Num() == 1));
			ASSERT_THAT(IsTrue(Purse->ReleaseHold(Hold.GetValue()) == Held, TEXT("cancelled: a full refund (§11.3)")));
			ASSERT_THAT(IsTrue(Purse->GetGold() == Starting && Purse->GetHolds().IsEmpty()));
			ASSERT_THAT(IsTrue(Purse->ReleaseHold(Hold.GetValue()) == 0.0, TEXT("never refunded twice")));
			ASSERT_THAT(IsTrue(Purse->GetGold() == Starting));
		}

		TEST_METHOD(ADeliveredPurchaseStaysSpent)
		{
			const TOptional<int32> Hold = Purse->Hold(Held);
			ASSERT_THAT(IsTrue(Hold.IsSet()));
			Purse->SettleHold(Hold.GetValue());
			ASSERT_THAT(IsTrue(Purse->GetHolds().IsEmpty() && Purse->GetGold() == Starting - Held));
			ASSERT_THAT(IsTrue(Purse->ReleaseHold(Hold.GetValue()) == 0.0, TEXT("a settled purchase is no longer held")));
		}

		TEST_METHOD(AHoldBeyondTheBalanceIsRefused)
		{
			ASSERT_THAT(IsFalse(Purse->Hold(Starting + 1.0).IsSet()));
			ASSERT_THAT(IsTrue(Purse->GetGold() == Starting && Purse->GetHolds().IsEmpty()));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
