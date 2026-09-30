// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraItemsTuning.h"

namespace VeyraItems
{
namespace
{
	// The tier rules (Item Bible §2): components, assemblies, Masterworks. Their meaning, not tuning.
	constexpr int32 ComponentTier = 1;
	constexpr int32 MasterworkTier = 3;
	// Boots stop at Tier 2 in the initial item system (Item Bible §5).
	constexpr int32 HighestBootsTier = 2;

	/** How many of the Attunement maps define Id. */
	int32 AttunementDefinitions(const FVeyraItemsTuning& Tuning, const FVeyraContentId& Id)
	{
		return static_cast<int32>(Tuning.WeightOfWar.Contains(Id)) + static_cast<int32>(Tuning.Overcharge.Contains(Id))
			+ static_cast<int32>(Tuning.SpoolUp.Contains(Id)) + static_cast<int32>(Tuning.Overcycle.Contains(Id))
			+ static_cast<int32>(Tuning.PerfectCut.Contains(Id));
	}
}

TArray<FString> Validate(const FVeyraItemsTuning& Tuning)
{
	TArray<FString> Problems;
	for (const TPair<FVeyraContentId, FVeyraConsumableTuning>& Entry : Tuning.Consumables)
	{
		const FVeyraItemDefinition* Item = Tuning.Items.Find(Entry.Key);
		if (Entry.Value.Charges > 0 && Item && Item->StackLimit != 1)
		{
			Problems.Add(FString::Printf(TEXT("/items/%s/stackLimit: a refillable consumable does not stack (Item Bible §10)"), *Entry.Key.ToString()));
		}
	}
	for (const TPair<FVeyraContentId, FVeyraItemDefinition>& Entry : Tuning.Items)
	{
		const FString Pointer = FString::Printf(TEXT("/items/%s"), *Entry.Key.ToString());
		const FVeyraItemDefinition& Item = Entry.Value;
		const bool bConsumable = Item.Category == EVeyraItemCategory::Consumable;

		if ((Item.Tier == ComponentTier) != Item.Components.IsEmpty())
		{
			Problems.Add(FString::Printf(TEXT("%s/components: a Tier 1 component has no recipe, and every higher tier has one"), *Pointer));
		}
		if ((Item.Tier == MasterworkTier ? 1 : 0) != Item.Attunement.Num())
		{
			Problems.Add(FString::Printf(TEXT("%s/attunement: a Masterwork has exactly one Attunement, and no lower tier has one"), *Pointer));
		}
		if (Item.Category == EVeyraItemCategory::Boots && Item.Tier > HighestBootsTier)
		{
			Problems.Add(FString::Printf(TEXT("%s/tier: Boots stop at Tier %d"), *Pointer, HighestBootsTier));
		}
		for (int32 Index = 0; Index < Item.Components.Num(); ++Index)
		{
			const FVeyraItemDefinition* Component = Tuning.Items.Find(Item.Components[Index]);
			if (!Component)
			{
				Problems.Add(FString::Printf(TEXT("%s/components/%d: names %s, which this file does not define"), *Pointer, Index, *Item.Components[Index].ToString()));
			}
			else if (Component->Tier >= Item.Tier)
			{
				Problems.Add(FString::Printf(TEXT("%s/components/%d: %s is not a lower tier, so the recipe could loop"), *Pointer, Index, *Item.Components[Index].ToString()));
			}
		}
		for (int32 Index = 0; Index < Item.Attunement.Num(); ++Index)
		{
			if (AttunementDefinitions(Tuning, Item.Attunement[Index]) != 1)
			{
				Problems.Add(FString::Printf(TEXT("%s/attunement/%d: %s must be defined in exactly one Attunement map"), *Pointer, Index, *Item.Attunement[Index].ToString()));
			}
		}
		if (bConsumable != Tuning.Consumables.Contains(Entry.Key))
		{
			Problems.Add(FString::Printf(TEXT("%s/category: a consumable has an entry in /consumables, and nothing else does"), *Pointer));
		}
		if (bConsumable && (Item.Tier != ComponentTier || !Item.Attunement.IsEmpty()))
		{
			Problems.Add(FString::Printf(TEXT("%s/tier: a consumable is a Tier 1 item with no Attunement"), *Pointer));
		}
		if (!bConsumable && Item.StackLimit != 1)
		{
			Problems.Add(FString::Printf(TEXT("%s/stackLimit: only a consumable stacks"), *Pointer));
		}
	}
	for (const TPair<FVeyraContentId, FVeyraConsumableTuning>& Consumable : Tuning.Consumables)
	{
		if (!Tuning.Items.Contains(Consumable.Key))
		{
			Problems.Add(FString::Printf(TEXT("/consumables/%s: names no item this file defines"), *Consumable.Key.ToString()));
		}
	}
	return Problems;
}

double TotalCost(const FVeyraItemsTuning& Tuning, const FVeyraContentId& Item)
{
	// Validate guarantees components are a lower tier, so this ends.
	const FVeyraItemDefinition* Definition = Tuning.Items.Find(Item);
	if (!Definition)
	{
		return 0.0;
	}
	double Total = Definition->Cost;
	for (const FVeyraContentId& Component : Definition->Components)
	{
		Total += TotalCost(Tuning, Component);
	}
	return Total;
}
}
