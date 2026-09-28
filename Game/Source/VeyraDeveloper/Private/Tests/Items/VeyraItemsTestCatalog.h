// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Tuning/VeyraItemsTuning.h"

namespace VeyraItemsTests
{
	inline FVeyraContentId ItemId(const TCHAR* Text)
	{
		return FVeyraContentId::FromText(Text).GetValue();
	}

	/**
	 * A test catalog in the bible's shape, independent of the committed Items.json: two components,
	 * Boots and their upgrade, two assemblies, a Masterwork and a consumable. Fixture values.
	 */
	inline FVeyraItemsTuning TestCatalog()
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
		Add(TEXT("test_boots"), 1, 300.0, {}).Category = EVeyraItemCategory::Boots;
		Add(TEXT("test_swift"), 2, 700.0, { ItemId(TEXT("test_boots")) }).Category = EVeyraItemCategory::Boots;
		Add(TEXT("test_wheel"), 2, 400.0, { ItemId(TEXT("test_grip")), ItemId(TEXT("test_grip")) });
		Add(TEXT("test_harness"), 2, 350.0, { ItemId(TEXT("test_plate")), ItemId(TEXT("test_grip")) });
		Add(TEXT("test_temper"), 3, 600.0, { ItemId(TEXT("test_harness")), ItemId(TEXT("test_plate")) }).Attunement = { ItemId(TEXT("test_weight")) };
		FVeyraItemDefinition& Tonic = Add(TEXT("test_tonic"), 1, 50.0, {});
		Tonic.Category = EVeyraItemCategory::Consumable;
		Tonic.StackLimit = 5;
		Tuning.Consumables.Add(ItemId(TEXT("test_tonic"))).ResaleFraction = 0.4;
		Tuning.WeightOfWar.Add(ItemId(TEXT("test_weight")));
		return Tuning;
	}
}
