// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraOffenceSet.h"
#include "Components/ActorTestSpawner.h"
#include "CQTest.h"
#include "Gold/VeyraGoldComponent.h"
#include "Inventory/VeyraInventoryComponent.h"
#include "Life/VeyraLifeComponent.h"
#include "Shop/VeyraShopSubsystem.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tests/Items/VeyraItemsTestCatalog.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "VeyraPlayerState.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraItemsTests
{
	// Veyra.Items.Shop.*: the shop's transactions on a participant, its Gold, items and stats (Economy &
	// Progression Bible §10–§12; ADR-012 §5–§6, §9).
	TEST_CLASS(Shop, "Veyra.Items")
	{
		// Fixture values.
		static constexpr double Purse = 10000.0;
		static constexpr double GripPower = 10.0;

		FActorTestSpawner Spawner;
		FVeyraItemsTuning Tuning = TestCatalog();
		UVeyraShopSubsystem* Subsystem = nullptr;
		AVeyraPlayerState* Participant = nullptr;
		UVeyraInventoryComponent* Inventory = nullptr;
		UVeyraGoldComponent* Gold = nullptr;

		BEFORE_EACH()
		{
			Tuning.Items[ItemId(TEXT("test_grip"))].Stats.PhysicalPower = GripPower;
			UVeyraItemsTuningSubsystem::SetTestOverride(&Tuning);
			Subsystem = Spawner.GetWorld().GetSubsystem<UVeyraShopSubsystem>();
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			Participant = World.Spawn(EVeyraTeam::A, FVector::ZeroVector).GetPlayerState<AVeyraPlayerState>();
			Inventory = Participant ? Participant->FindComponentByClass<UVeyraInventoryComponent>() : nullptr;
			Gold = Participant ? Participant->FindComponentByClass<UVeyraGoldComponent>() : nullptr;
			ASSERT_THAT(IsTrue(Subsystem && Inventory && Gold));
			UVeyraShopSubsystem::InitializeInventory(*Participant);
			ASSERT_THAT(IsTrue(Gold->Grant(Purse, EVeyraGoldReason::Developer)));
		}

		AFTER_EACH()
		{
			UVeyraItemsTuningSubsystem::SetTestOverride(nullptr);
		}

		int32 CountOf(const TCHAR* Item) const
		{
			int32 Count = 0;
			for (const FVeyraInventorySlot& Slot : Inventory->GetSlots())
			{
				Count += !Slot.IsEmpty() && Slot.Item == ItemId(Item) ? Slot.Count : 0;
			}
			return Count;
		}

		double PhysicalPower() const
		{
			return Participant->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraOffenceSet::GetPhysicalPowerAttribute());
		}

		void Die() const
		{
			Participant->FindComponentByClass<UVeyraLifeComponent>()->SetState(EVeyraLifeState::Dead);
		}

		TEST_METHOD(AtTheFountainAPurchaseArrivesAtOnceWithItsStats)
		{
			const double Before = PhysicalPower();
			Subsystem->SetAtFountain(*Participant, true);
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, ItemId(TEXT("test_grip"))) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(CountOf(TEXT("test_grip")) == 1 && Gold->GetGold() == Purse - 350.0));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(PhysicalPower(), Before + GripPower), TEXT("its stats apply")));
		}

		TEST_METHOD(AwayAPurchaseWaitsAndGivesNothingUntilTheFountain)
		{
			const double Before = PhysicalPower();
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, ItemId(TEXT("test_grip"))) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(CountOf(TEXT("test_grip")) == 0 && Inventory->GetQueue().Num() == 1));
			ASSERT_THAT(IsTrue(Gold->GetGold() == Purse - 350.0 && Gold->GetHolds().Num() == 1, TEXT("spent now, and held")));
			ASSERT_THAT(IsTrue(PhysicalPower() == Before, TEXT("no benefit before delivery (§11.1)")));

			Subsystem->SetAtFountain(*Participant, true);
			ASSERT_THAT(IsTrue(CountOf(TEXT("test_grip")) == 1 && Inventory->GetQueue().IsEmpty() && Gold->GetHolds().IsEmpty()));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(PhysicalPower(), Before + GripPower)));
		}

		TEST_METHOD(CancellingRefundsInFullWithWhatDependedOnIt)
		{
			Subsystem->Buy(*Participant, ItemId(TEXT("test_grip")));
			Subsystem->Buy(*Participant, ItemId(TEXT("test_grip")));
			Subsystem->Buy(*Participant, ItemId(TEXT("test_plate")));
			Subsystem->Buy(*Participant, ItemId(TEXT("test_wheel")));
			ASSERT_THAT(IsTrue(Subsystem->Cancel(*Participant, 0) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(Inventory->GetQueue().Num() == 2, TEXT("the wheel needed the cancelled grip")));
			ASSERT_THAT(IsTrue(Gold->GetGold() == Purse - 350.0 - 400.0, TEXT("the other grip and the plate stay paid; the rest came back")));
		}

		TEST_METHOD(DeathDeliversTheQueue)
		{
			Subsystem->Buy(*Participant, ItemId(TEXT("test_plate")));
			Die();
			Subsystem->DeliverOnDeath(*Participant);
			ASSERT_THAT(IsTrue(CountOf(TEXT("test_plate")) == 1 && Inventory->GetQueue().IsEmpty()));
		}

		TEST_METHOD(SellingHappensAtTheFountainForTheResaleValue)
		{
			Subsystem->SetAtFountain(*Participant, true);
			Subsystem->Buy(*Participant, ItemId(TEXT("test_harness")));
			Subsystem->SetAtFountain(*Participant, false);
			ASSERT_THAT(IsTrue(Subsystem->Sell(*Participant, 0) == EVeyraShopRefusal::NotAtFountain));
			Subsystem->SetAtFountain(*Participant, true);
			const double Before = Gold->GetGold();
			ASSERT_THAT(IsTrue(Subsystem->Sell(*Participant, 0) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Gold->GetGold() - Before, 0.7 * VeyraItems::TotalCost(Tuning, ItemId(TEXT("test_harness"))))));
		}

		TEST_METHOD(UndoTakesBackSeveralStepsForEveryCoin)
		{
			Subsystem->SetAtFountain(*Participant, true);
			Subsystem->Buy(*Participant, ItemId(TEXT("test_grip")));
			Subsystem->Buy(*Participant, ItemId(TEXT("test_grip")));
			Subsystem->Buy(*Participant, ItemId(TEXT("test_wheel")));
			ASSERT_THAT(IsTrue(Subsystem->Undo(*Participant) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(CountOf(TEXT("test_grip")) == 2, TEXT("the recipe's components come back")));
			ASSERT_THAT(IsTrue(Subsystem->Undo(*Participant) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(Subsystem->Undo(*Participant) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(Gold->GetGold() == Purse && CountOf(TEXT("test_grip")) == 0, TEXT("no Gold gained or lost")));
			ASSERT_THAT(IsTrue(Subsystem->Undo(*Participant) == EVeyraShopRefusal::NothingToUndo));
		}

		TEST_METHOD(UndoEndsOnLeavingTheFountain)
		{
			Subsystem->SetAtFountain(*Participant, true);
			Subsystem->Buy(*Participant, ItemId(TEXT("test_grip")));
			Subsystem->SetAtFountain(*Participant, false);
			Subsystem->SetAtFountain(*Participant, true);
			ASSERT_THAT(IsTrue(Subsystem->Undo(*Participant) == EVeyraShopRefusal::NothingToUndo));
		}

		TEST_METHOD(TheDeadShopAsAtTheFountain)
		{
			Die();
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, ItemId(TEXT("test_grip"))) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(CountOf(TEXT("test_grip")) == 1, TEXT("assigned at the fountain (§10)")));
			ASSERT_THAT(IsTrue(Subsystem->Sell(*Participant, 0) == EVeyraShopRefusal::None));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
