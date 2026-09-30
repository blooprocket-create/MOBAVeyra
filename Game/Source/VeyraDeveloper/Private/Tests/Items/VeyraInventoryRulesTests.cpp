// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Inventory/VeyraEquipmentRules.h"
#include "Inventory/VeyraInventoryRules.h"
#include "Tests/Items/VeyraItemsTestCatalog.h"
#include "Tuning/VeyraItemsTuning.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraItemsTests
{
	// Veyra.Items.InventoryRules.*: pricing, holding and the purchase queue, one test per rule of the
	// Economy & Progression Bible §10–§12 and ADR-012 §9.
	TEST_CLASS(InventoryRules, "Veyra.Items")
	{
		const FVeyraItemsTuning Tuning = TestCatalog();
		TArray<FVeyraInventorySlot> Slots;
		TArray<FVeyraPendingPurchase> Queue;

		BEFORE_EACH()
		{
			Slots.SetNum(Tuning.Shop.InventorySlots);
		}

		/** Buys Item into the slots at once, as at the fountain. */
		FVeyraPurchaseQuote BuyHere(const TCHAR* Item)
		{
			const FVeyraPurchaseQuote Quote = VeyraInventory::Quote(Tuning, Slots, Queue, ItemId(Item));
			if (Quote.Refusal == EVeyraShopRefusal::None)
			{
				FVeyraPendingPurchase Entry;
				Entry.Item = ItemId(Item);
				Entry.Paid = Quote.Price;
				Entry.Needs = Quote.Needs;
				VeyraInventory::Apply(Tuning, Slots, Entry);
			}
			return Quote;
		}

		/** Buys Item into the queue, as away from the fountain. */
		FVeyraPurchaseQuote BuyAway(const TCHAR* Item)
		{
			const FVeyraPurchaseQuote Quote = VeyraInventory::Quote(Tuning, Slots, Queue, ItemId(Item));
			if (Quote.Refusal == EVeyraShopRefusal::None)
			{
				FVeyraPendingPurchase& Entry = Queue.AddDefaulted_GetRef();
				Entry.Item = ItemId(Item);
				Entry.Paid = Quote.Price;
				Entry.Needs = Quote.Needs;
			}
			return Quote;
		}

		int32 CountOf(TConstArrayView<FVeyraInventorySlot> In, const TCHAR* Item) const
		{
			int32 Count = 0;
			for (const FVeyraInventorySlot& Slot : In)
			{
				Count += !Slot.IsEmpty() && Slot.Item == ItemId(Item) ? Slot.Count : 0;
			}
			return Count;
		}

		TEST_METHOD(ARecipeBuysWhatIsMissingAndUsesWhatIsOwned)
		{
			BuyHere(TEXT("test_plate"));
			const FVeyraPurchaseQuote Temper = BuyHere(TEXT("test_temper"));
			ASSERT_THAT(IsTrue(Temper.Refusal == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(Temper.Price == VeyraItems::TotalCost(Tuning, ItemId(TEXT("test_temper"))) - 400.0, TEXT("the owned plate counts")));
			ASSERT_THAT(IsTrue(CountOf(Slots, TEXT("test_temper")) == 1 && CountOf(Slots, TEXT("test_plate")) == 0));
			const FVeyraInventorySlot* Held = Slots.FindByPredicate([](const FVeyraInventorySlot& Slot) { return Slot.Item == ItemId(TEXT("test_temper")); });
			ASSERT_THAT(IsTrue(Held->PaidEach == VeyraItems::TotalCost(Tuning, ItemId(TEXT("test_temper"))), TEXT("its present form's whole cost")));
		}

		TEST_METHOD(ValidationCountsTheSlotsTheQueueWillFill)
		{
			for (int32 Index = 0; Index < Tuning.Shop.InventorySlots; ++Index)
			{
				ASSERT_THAT(IsTrue(BuyAway(TEXT("test_plate")).Refusal == EVeyraShopRefusal::None));
			}
			ASSERT_THAT(IsTrue(BuyAway(TEXT("test_grip")).Refusal == EVeyraShopRefusal::InventoryFull, TEXT("pending purchases are no extra storage")));
		}

		TEST_METHOD(ARecipeFreesTheSlotsItsComponentsLeave)
		{
			for (int32 Index = 0; Index < Tuning.Shop.InventorySlots - 1; ++Index)
			{
				BuyHere(TEXT("test_plate"));
			}
			BuyHere(TEXT("test_grip"));
			ASSERT_THAT(IsTrue(BuyAway(TEXT("test_harness")).Refusal == EVeyraShopRefusal::None, TEXT("full, but it consumes a plate and the grip")));
		}

		TEST_METHOD(APendingRecipeReservesWithoutConsuming)
		{
			BuyHere(TEXT("test_grip"));
			const FVeyraPurchaseQuote Wheel = BuyAway(TEXT("test_wheel"));
			ASSERT_THAT(IsTrue(Wheel.Price == 400.0 + 350.0, TEXT("one grip owned, one bought as part")));
			ASSERT_THAT(IsTrue(CountOf(Slots, TEXT("test_grip")) == 1, TEXT("the owned grip still works in the field")));
		}

		TEST_METHOD(AnEarlierPurchaseFeedsALaterRecipe)
		{
			BuyAway(TEXT("test_grip"));
			BuyAway(TEXT("test_grip"));
			ASSERT_THAT(IsTrue(BuyAway(TEXT("test_wheel")).Price == 400.0, TEXT("both queued grips feed it")));
			TArray<FVeyraInventorySlot> After;
			ASSERT_THAT(IsTrue(VeyraInventory::Simulate(Tuning, Slots, Queue, After).IsEmpty()));
			ASSERT_THAT(IsTrue(CountOf(After, TEXT("test_wheel")) == 1 && CountOf(After, TEXT("test_grip")) == 0));
		}

		TEST_METHOD(TwoRecipesNeverConsumeOneComponent)
		{
			BuyHere(TEXT("test_grip"));
			ASSERT_THAT(IsTrue(BuyAway(TEXT("test_harness")).Price == 350.0 + 400.0));
			ASSERT_THAT(IsTrue(BuyAway(TEXT("test_harness")).Price == 350.0 + 400.0 + 350.0, TEXT("the second pays for its own grip")));
		}

		TEST_METHOD(AMissingComponentInvalidatesWhatNeededItAndWhatFollowed)
		{
			BuyAway(TEXT("test_grip"));
			BuyAway(TEXT("test_grip"));
			BuyAway(TEXT("test_plate"));
			BuyAway(TEXT("test_wheel"));
			// The first grip is cancelled: the wheel needed it; the plate did not.
			Queue.RemoveAt(0);
			TArray<FVeyraInventorySlot> After;
			const TArray<int32> Invalid = VeyraInventory::Simulate(Tuning, Slots, Queue, After);
			ASSERT_THAT(IsTrue(Invalid.Num() == 1 && Queue[Invalid[0]].Item == ItemId(TEXT("test_wheel"))));
		}

		TEST_METHOD(MasterworksAreUniqueAndAssembliesRepeat)
		{
			BuyHere(TEXT("test_temper"));
			ASSERT_THAT(IsTrue(BuyHere(TEXT("test_temper")).Refusal == EVeyraShopRefusal::Unique));
			BuyHere(TEXT("test_wheel"));
			ASSERT_THAT(IsTrue(BuyHere(TEXT("test_wheel")).Refusal == EVeyraShopRefusal::None));
		}

		TEST_METHOD(OnePairOfBootsWhichUpgrade)
		{
			BuyHere(TEXT("test_boots"));
			ASSERT_THAT(IsTrue(BuyHere(TEXT("test_boots")).Refusal == EVeyraShopRefusal::BootsLimit));
			ASSERT_THAT(IsTrue(BuyHere(TEXT("test_swift")).Refusal == EVeyraShopRefusal::None, TEXT("the upgrade consumes the pair it replaces")));
			ASSERT_THAT(IsTrue(CountOf(Slots, TEXT("test_swift")) == 1 && CountOf(Slots, TEXT("test_boots")) == 0));
		}

		TEST_METHOD(ConsumablesStackToTheirLimitThenTakeASlot)
		{
			for (int32 Index = 0; Index < 6; ++Index)
			{
				BuyHere(TEXT("test_tonic"));
			}
			ASSERT_THAT(IsTrue(CountOf(Slots, TEXT("test_tonic")) == 6));
			ASSERT_THAT(IsTrue(Slots.FilterByPredicate([](const FVeyraInventorySlot& Slot) { return !Slot.IsEmpty(); }).Num() == 2));
		}

		TEST_METHOD(EquipmentAddsEachItemsStatsAndBonusAttackSpeedFromTheBase)
		{
			// Fixture values: a grip with power and Attack Speed, two of them held.
			constexpr double BaseAttackSpeed = 0.625;
			FVeyraItemsTuning WithStats = Tuning;
			WithStats.Items[ItemId(TEXT("test_grip"))].Stats.PhysicalPower = 10.0;
			WithStats.Items[ItemId(TEXT("test_grip"))].Stats.AttackSpeed = 0.12;
			WithStats.Items[ItemId(TEXT("test_plate"))].Stats.Health = 150.0;
			BuyHere(TEXT("test_grip"));
			BuyHere(TEXT("test_grip"));
			BuyHere(TEXT("test_plate"));
			const FVeyraEquipmentStats Stats = VeyraEquipment::StatsFor(WithStats, Slots, BaseAttackSpeed, {});
			ASSERT_THAT(IsTrue(Stats.PhysicalPower == 20.0 && Stats.MaxHealth == 150.0));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Stats.AttackSpeed, BaseAttackSpeed * 0.24), TEXT("a fraction of the base, added (ADR-012 §6)")));
		}

		TEST_METHOD(ArmorAndMagicResistAddFromEachHeldItem)
		{
			// Fixture values: a plate with both defences and a grip with Armor, two grips held (ADR-025 §1).
			constexpr double BaseAttackSpeed = 0.625;
			FVeyraItemsTuning WithDefences = Tuning;
			WithDefences.Items[ItemId(TEXT("test_plate"))].Stats.Armor = 15.0;
			WithDefences.Items[ItemId(TEXT("test_plate"))].Stats.MagicResist = 25.0;
			WithDefences.Items[ItemId(TEXT("test_grip"))].Stats.Armor = 5.0;
			BuyHere(TEXT("test_plate"));
			BuyHere(TEXT("test_grip"));
			BuyHere(TEXT("test_grip"));
			const FVeyraEquipmentStats Stats = VeyraEquipment::StatsFor(WithDefences, Slots, BaseAttackSpeed, {});
			ASSERT_THAT(IsTrue(Stats.Armor == 25.0 && Stats.MagicResist == 25.0));
		}

		TEST_METHOD(EachAttunementAddsWhatItsDataSays)
		{
			// Fixture values: the Masterwork's Weight of War, and an Overcharge, a Spool Up and an
			// Overcycle given to it in turn.
			constexpr double BaseAttackSpeed = 0.625;
			FVeyraItemsTuning WithAttunements = Tuning;
			FVeyraItemDefinition& Temper = WithAttunements.Items[ItemId(TEXT("test_temper"))];
			Temper.Stats.Health = 400.0;
			WithAttunements.WeightOfWar[ItemId(TEXT("test_weight"))].BonusHealthFraction = 0.025;
			BuyHere(TEXT("test_temper"));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraEquipment::StatsFor(WithAttunements, Slots, BaseAttackSpeed, {}).PhysicalPower, 400.0 * 0.025),
				TEXT("Weight of War: Physical Power from bonus Health")));

			const FVeyraContentId Stacking = ItemId(TEXT("test_stacking"));
			Temper.Attunement = { Stacking };
			WithAttunements.Overcharge.Add(Stacking).MagicPowerFraction = 0.3;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraEquipment::StatsFor(WithAttunements, Slots, BaseAttackSpeed, {}).MagicPowerFraction, 0.3), TEXT("Overcharge")));

			WithAttunements.Overcharge.Reset();
			FVeyraStackingAttunementTuning& SpoolUp = WithAttunements.SpoolUp.Add(Stacking);
			SpoolUp.PerStack = 0.06;
			SpoolUp.MaxStacks = 5;
			const TMap<FVeyraContentId, int32> Beyond = { { Stacking, 9 } };
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraEquipment::StatsFor(WithAttunements, Slots, BaseAttackSpeed, Beyond).AttackSpeed, BaseAttackSpeed * 0.06 * 5),
				TEXT("Spool Up: bonus Attack Speed per stack, up to its cap")));

			WithAttunements.SpoolUp.Reset();
			FVeyraStackingAttunementTuning& Overcycle = WithAttunements.Overcycle.Add(Stacking);
			Overcycle.PerStack = 4.0;
			Overcycle.MaxStacks = 5;
			const TMap<FVeyraContentId, int32> Two = { { Stacking, 2 } };
			ASSERT_THAT(IsTrue(VeyraEquipment::StatsFor(WithAttunements, Slots, BaseAttackSpeed, Two).AbilityHaste == 8.0, TEXT("Overcycle: Ability Haste per stack")));
		}

		TEST_METHOD(CritAddsAndMagicPowerPercentagesMultiply)
		{
			// Fixture values: a grip with Crit Chance, two held, and a Masterwork with a Perfect Cut and a
			// Magic Power percentage beside an Overcharge (ADR-023 §2-§3).
			constexpr double BaseAttackSpeed = 0.625;
			FVeyraItemsTuning WithCrit = Tuning;
			WithCrit.Items[ItemId(TEXT("test_grip"))].Stats.CritChance = 0.15;
			FVeyraItemDefinition& Temper = WithCrit.Items[ItemId(TEXT("test_temper"))];
			Temper.Stats.MagicPowerFraction = 0.08;
			const FVeyraContentId Cut = ItemId(TEXT("test_cut"));
			Temper.Attunement = { Cut };
			WithCrit.WeightOfWar.Reset();
			WithCrit.PerfectCut.Add(Cut).CritDamageBonus = 0.4;
			// The Masterwork first: bought after the grips, its recipe would take one of them.
			BuyHere(TEXT("test_temper"));
			BuyHere(TEXT("test_grip"));
			BuyHere(TEXT("test_grip"));
			FVeyraEquipmentStats Stats = VeyraEquipment::StatsFor(WithCrit, Slots, BaseAttackSpeed, {});
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Stats.CritChance, 0.3), TEXT("each grip's Crit Chance adds")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Stats.CritDamageBonus, 0.4), TEXT("Perfect Cut")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Stats.MagicPowerFraction, 0.08)));

			// A second source of a Magic Power percentage multiplies with the first (Combat Bible §41).
			WithCrit.PerfectCut.Reset();
			WithCrit.Overcharge.Add(Cut).MagicPowerFraction = 0.3;
			Stats = VeyraEquipment::StatsFor(WithCrit, Slots, BaseAttackSpeed, {});
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Stats.MagicPowerFraction, 1.08 * 1.3 - 1.0), FString::SanitizeFloat(Stats.MagicPowerFraction)));
		}

		TEST_METHOD(ARefillableConsumableIsHeldOnceAndArrivesFull)
		{
			// Fixture values: the tonic made refillable, with two charges (Item Bible §12; ADR-023 §6).
			constexpr int32 Charges = 2;
			const FVeyraContentId Flask = ItemId(TEXT("test_tonic"));
			FVeyraItemsTuning Refillable = Tuning;
			Refillable.Items[Flask].StackLimit = 1;
			Refillable.Consumables[Flask].Charges = Charges;

			FVeyraPendingPurchase Entry;
			Entry.Item = Flask;
			Entry.Paid = VeyraInventory::Quote(Refillable, Slots, Queue, Flask).Price;
			ASSERT_THAT(IsTrue(VeyraInventory::Apply(Refillable, Slots, Entry) == EVeyraShopRefusal::None));
			const FVeyraInventorySlot* Held = Slots.FindByPredicate([&Flask](const FVeyraInventorySlot& Slot) { return Slot.Item == Flask; });
			ASSERT_THAT(IsTrue(Held && Held->Count == 1 && Held->Charges == Charges, TEXT("it arrives full")));
			ASSERT_THAT(IsTrue(VeyraInventory::Quote(Refillable, Slots, Queue, Flask).Refusal == EVeyraShopRefusal::Unique, TEXT("held once")));

			// One waiting in the queue is held too.
			Slots.Reset();
			Slots.SetNum(Tuning.Shop.InventorySlots);
			Queue.Add(Entry);
			ASSERT_THAT(IsTrue(VeyraInventory::Quote(Refillable, Slots, Queue, Flask).Refusal == EVeyraShopRefusal::Unique));
			ASSERT_THAT(IsTrue(VeyraInventory::Quote(Tuning, Slots, Queue, Flask).Refusal == EVeyraShopRefusal::None, TEXT("one used up stacks as before")));
		}

		TEST_METHOD(ResaleIsTheShopsFractionOrTheConsumablesOwn)
		{
			BuyHere(TEXT("test_harness"));
			BuyHere(TEXT("test_tonic"));
			const FVeyraInventorySlot& Harness = *Slots.FindByPredicate([](const FVeyraInventorySlot& Slot) { return Slot.Item == ItemId(TEXT("test_harness")); });
			const FVeyraInventorySlot& Tonic = *Slots.FindByPredicate([](const FVeyraInventorySlot& Slot) { return Slot.Item == ItemId(TEXT("test_tonic")); });
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraInventory::ResaleValue(Tuning, Harness), 0.7 * VeyraItems::TotalCost(Tuning, ItemId(TEXT("test_harness"))))));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraInventory::ResaleValue(Tuning, Tonic), 0.4 * 50.0)));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
