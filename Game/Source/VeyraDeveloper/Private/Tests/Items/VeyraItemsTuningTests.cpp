// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Engine/Engine.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraItemsTests
{
	FVeyraContentId ItemId(const TCHAR* Text)
	{
		return FVeyraContentId::FromText(Text).GetValue();
	}

	/** A test catalog in the bible's shape: two components, an assembly, a Masterwork and a consumable. Fixture values. */
	FVeyraItemsTuning TestCatalog()
	{
		FVeyraItemsTuning Tuning;
		Tuning.Shop.InventorySlots = 6;
		Tuning.Shop.ResaleFraction = 0.7;
		Tuning.Shop.UniqueFromTier = 3;
		Tuning.Shop.MaxBoots = 1;
		const auto Add = [&Tuning](const TCHAR* Id, int32 Tier, double Cost, TArray<FVeyraContentId> Components) -> FVeyraItemDefinition& {
			FVeyraItemDefinition& Item = Tuning.Items.Add(ItemId(Id));
			Item.Tier = Tier;
			Item.Cost = Cost;
			Item.StackLimit = 1;
			Item.Components = MoveTemp(Components);
			return Item;
		};
		Add(TEXT("test_grip"), 1, 350.0, {});
		Add(TEXT("test_plate"), 1, 400.0, {});
		Add(TEXT("test_harness"), 2, 350.0, { ItemId(TEXT("test_grip")), ItemId(TEXT("test_plate")) });
		Add(TEXT("test_temper"), 3, 600.0, { ItemId(TEXT("test_harness")), ItemId(TEXT("test_plate")) }).Attunement = { ItemId(TEXT("test_weight")) };
		FVeyraItemDefinition& Tonic = Add(TEXT("test_tonic"), 1, 50.0, {});
		Tonic.Category = EVeyraItemCategory::Consumable;
		Tonic.StackLimit = 5;
		Tuning.Consumables.Add(ItemId(TEXT("test_tonic")));
		Tuning.WeightOfWar.Add(ItemId(TEXT("test_weight")));
		return Tuning;
	}

	bool HasProblem(const TArray<FString>& Problems, const TCHAR* Prefix)
	{
		return Problems.ContainsByPredicate([Prefix](const FString& Problem) { return Problem.StartsWith(Prefix); });
	}

	// Veyra.Items.Catalog.*: the tier rules and the costs (Item Bible §2, §11; ADR-012 §3).
	TEST_CLASS(Catalog, "Veyra.Items")
	{
		TEST_METHOD(AConsistentCatalogPasses)
		{
			const TArray<FString> Problems = VeyraItems::Validate(TestCatalog());
			ASSERT_THAT(IsTrue(Problems.IsEmpty(), FString::Join(Problems, TEXT(" | "))));
		}

		TEST_METHOD(TheTierRulesAreEnforced)
		{
			FVeyraItemsTuning Broken = TestCatalog();
			Broken.Items[ItemId(TEXT("test_grip"))].Components = { ItemId(TEXT("test_plate")) };
			Broken.Items[ItemId(TEXT("test_harness"))].Attunement = { ItemId(TEXT("test_weight")) };
			Broken.Items[ItemId(TEXT("test_temper"))].Attunement.Reset();
			const TArray<FString> Problems = VeyraItems::Validate(Broken);
			ASSERT_THAT(IsTrue(HasProblem(Problems, TEXT("/items/test_grip/components")), FString::Join(Problems, TEXT(" | "))));
			ASSERT_THAT(IsTrue(HasProblem(Problems, TEXT("/items/test_harness/attunement")), TEXT("no Attunement below a Masterwork")));
			ASSERT_THAT(IsTrue(HasProblem(Problems, TEXT("/items/test_temper/attunement")), TEXT("a Masterwork has one")));
		}

		TEST_METHOD(RecipesNeverLoopAndBootsStopAtTierTwo)
		{
			FVeyraItemsTuning Broken = TestCatalog();
			Broken.Items[ItemId(TEXT("test_harness"))].Components.Add(ItemId(TEXT("test_temper")));
			Broken.Items[ItemId(TEXT("test_temper"))].Category = EVeyraItemCategory::Boots;
			const TArray<FString> Problems = VeyraItems::Validate(Broken);
			ASSERT_THAT(IsTrue(HasProblem(Problems, TEXT("/items/test_harness/components/2")), FString::Join(Problems, TEXT(" | "))));
			ASSERT_THAT(IsTrue(HasProblem(Problems, TEXT("/items/test_temper/tier"))));
		}

		TEST_METHOD(OnlyConsumablesStackAndEachHasItsEntry)
		{
			FVeyraItemsTuning Broken = TestCatalog();
			Broken.Items[ItemId(TEXT("test_grip"))].StackLimit = 2;
			Broken.Consumables.Remove(ItemId(TEXT("test_tonic")));
			const TArray<FString> Problems = VeyraItems::Validate(Broken);
			ASSERT_THAT(IsTrue(HasProblem(Problems, TEXT("/items/test_grip/stackLimit")), FString::Join(Problems, TEXT(" | "))));
			ASSERT_THAT(IsTrue(HasProblem(Problems, TEXT("/items/test_tonic/category"))));
		}

		TEST_METHOD(TotalCostCountsEveryComponentAllTheWayDown)
		{
			const FVeyraItemsTuning Tuning = TestCatalog();
			ASSERT_THAT(IsTrue(VeyraItems::TotalCost(Tuning, ItemId(TEXT("test_grip"))) == 350.0));
			ASSERT_THAT(IsTrue(VeyraItems::TotalCost(Tuning, ItemId(TEXT("test_harness"))) == 350.0 + 350.0 + 400.0));
			ASSERT_THAT(IsTrue(VeyraItems::TotalCost(Tuning, ItemId(TEXT("test_temper"))) == 600.0 + 1100.0 + 400.0));
		}

		TEST_METHOD(TheCommittedCatalogLoads)
		{
			const UVeyraItemsTuningSubsystem* Subsystem = GEngine->GetEngineSubsystem<UVeyraItemsTuningSubsystem>();
			ASSERT_THAT(IsTrue(Subsystem && Subsystem->IsLoaded()));
			ASSERT_THAT(IsTrue(VeyraItems::Validate(UVeyraItemsTuningSubsystem::Get()).IsEmpty()));
			// Starting Gold buys a component, or a component and consumables (Economy & Progression Bible §10).
			ASSERT_THAT(IsTrue(UVeyraItemsTuningSubsystem::FindItem(ItemId(TEXT("iron_grip"))) != nullptr));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
