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

	/**
	 * Tuning with two Tier 4 Mythicals built on the Masterwork, each with two Attunements: Weight of War,
	 * carried on from the Masterwork as The Last Harbor carries Safe Harbor, and an Overcharge (Item Bible
	 * §11; ADR-025 §2). Fixture values.
	 */
	inline FVeyraItemsTuning WithMythicals(FVeyraItemsTuning Tuning)
	{
		const FVeyraContentId Weight = ItemId(TEXT("test_weight"));
		const FVeyraContentId Charge = ItemId(TEXT("test_charge"));
		Tuning.Overcharge.Add(Charge).MagicPowerFraction = 0.1;
		const auto Add = [&Tuning, &Weight, &Charge](const TCHAR* Id, const TCHAR* Second) {
			FVeyraItemDefinition& Item = Tuning.Items.Add(ItemId(Id));
			Item.Tier = 4;
			Item.Cost = 500.0;
			Item.StackLimit = 1;
			Item.Components = { ItemId(TEXT("test_temper")), ItemId(Second) };
			Item.Attunement = { Weight, Charge };
		};
		Add(TEXT("test_harbor"), TEXT("test_wheel"));
		Add(TEXT("test_rival"), TEXT("test_harness"));
		return Tuning;
	}

	/**
	 * Tuning with a quest line (Item Bible §10; ADR-025 §3): test_reclaimer, a Quest Item, evolves after
	 * two last hits into test_reservoir, which the shop never sells and which carries Residual Current
	 * (test_current); test_haven is a Masterwork built on the reservoir. Fixture values.
	 */
	inline FVeyraItemsTuning WithQuest(FVeyraItemsTuning Tuning)
	{
		const auto Add = [&Tuning](const TCHAR* Id, int32 Tier, double Cost) -> FVeyraItemDefinition& {
			FVeyraItemDefinition& Item = Tuning.Items.Add(ItemId(Id));
			Item.Tier = Tier;
			Item.Cost = Cost;
			Item.StackLimit = 1;
			Item.Category = EVeyraItemCategory::Quest;
			return Item;
		};
		Add(TEXT("test_reclaimer"), 1, 450.0);
		Add(TEXT("test_reservoir"), 2, 0.0).Attunement = { ItemId(TEXT("test_current")) };
		FVeyraQuestTuning& Quest = Tuning.Quests.Add(ItemId(TEXT("test_reclaimer")));
		Quest.Threshold = 2;
		Quest.EvolvesInto = ItemId(TEXT("test_reservoir"));
		FVeyraResidualCurrentTuning& Current = Tuning.ResidualCurrent.Add(ItemId(TEXT("test_current")));
		Current.CurrentPerLastHit = 2.0;
		Current.CurrentCap = 5.0;
		Current.QuietSeconds = 1.0;
		Current.HeldSeconds = 1.0;
		Current.CurrentPerSecond = 1.0;
		Current.RegenerationAmplification = 3.0;
		FVeyraItemDefinition& Haven = Tuning.Items.Add(ItemId(TEXT("test_haven")));
		Haven.Tier = 3;
		Haven.Cost = 500.0;
		Haven.StackLimit = 1;
		Haven.Components = { ItemId(TEXT("test_reservoir")), ItemId(TEXT("test_plate")) };
		Haven.Attunement = { ItemId(TEXT("test_weight")) };
		return Tuning;
	}
}
