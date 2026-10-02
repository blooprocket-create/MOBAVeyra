// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraItemsTuning.h"

namespace VeyraItems
{
namespace
{
	// The tier rules (Item Bible §2, §11): components, assemblies, Masterworks, Mythicals. Their meaning, not tuning.
	constexpr int32 ComponentTier = 1;
	constexpr int32 MasterworkTier = 3;
	// How many Attunements a Masterwork and a Mythical carry (§2, §11; ADR-025 §2), and passives a Quest Item (ADR-025 §3).
	constexpr int32 MasterworkAttunements = 1;
	constexpr int32 MythicalAttunements = 2;
	constexpr int32 QuestPassives = 1;
	// Boots stop at Tier 2 in the initial item system (Item Bible §5).
	constexpr int32 HighestBootsTier = 2;

	/** How many of the Attunement maps define Id. */
	int32 AttunementDefinitions(const FVeyraItemsTuning& Tuning, const FVeyraContentId& Id)
	{
		return static_cast<int32>(Tuning.WeightOfWar.Contains(Id)) + static_cast<int32>(Tuning.Overcharge.Contains(Id))
			+ static_cast<int32>(Tuning.SpoolUp.Contains(Id)) + static_cast<int32>(Tuning.Overcycle.Contains(Id))
			+ static_cast<int32>(Tuning.PerfectCut.Contains(Id)) + static_cast<int32>(Tuning.ReprisalGuard.Contains(Id))
			+ static_cast<int32>(Tuning.Drag.Contains(Id)) + static_cast<int32>(Tuning.Convergence.Contains(Id))
			+ static_cast<int32>(Tuning.Fracture.Contains(Id)) + static_cast<int32>(Tuning.EndlessCleave.Contains(Id))
			+ static_cast<int32>(Tuning.TemperedByConflict.Contains(Id)) + static_cast<int32>(Tuning.ResidualCurrent.Contains(Id))
			+ static_cast<int32>(Tuning.DragTheTempo.Contains(Id)) + static_cast<int32>(Tuning.QuietingChime.Contains(Id))
			+ static_cast<int32>(Tuning.MarkedForDoom.Contains(Id)) + static_cast<int32>(Tuning.SafeHarbor.Contains(Id))
			+ static_cast<int32>(Tuning.HighTide.Contains(Id)) + static_cast<int32>(Tuning.Reverberation.Contains(Id));
	}
}

TArray<FString> Validate(const FVeyraItemsTuning& Tuning)
{
	TArray<FString> Problems;
	// A Magic Resist Reduction always leaves some of the resistance (ADR-023 §5).
	for (const TPair<FVeyraContentId, FVeyraStackingAttunementTuning>& Entry : Tuning.Fracture)
	{
		if (Entry.Value.PerStack * Entry.Value.MaxStacks >= 1.0)
		{
			Problems.Add(FString::Printf(TEXT("/fracture/%s/perStack: every stack together must remove less than all Magic Resistance"), *Entry.Key.ToString()));
		}
	}
	for (const TPair<FVeyraContentId, FVeyraConsumableTuning>& Entry : Tuning.Consumables)
	{
		const FVeyraItemDefinition* Item = Tuning.Items.Find(Entry.Key);
		if (Entry.Value.Charges > 0 && Item && Item->StackLimit != 1)
		{
			Problems.Add(FString::Printf(TEXT("/items/%s/stackLimit: a refillable consumable does not stack (Item Bible §12)"), *Entry.Key.ToString()));
		}
	}
	for (const TPair<FVeyraContentId, FVeyraItemDefinition>& Entry : Tuning.Items)
	{
		const FString Pointer = FString::Printf(TEXT("/items/%s"), *Entry.Key.ToString());
		const FVeyraItemDefinition& Item = Entry.Value;
		const bool bConsumable = Item.Category == EVeyraItemCategory::Consumable;
		const bool bQuestItem = Item.Category == EVeyraItemCategory::Quest;
		const bool bEvolutionOnly = IsEvolutionOnly(Tuning, Entry.Key);

		if (bEvolutionOnly ? !Item.Components.IsEmpty() : (Item.Tier == ComponentTier) != Item.Components.IsEmpty())
		{
			Problems.Add(FString::Printf(TEXT("%s/components: a Tier 1 component has no recipe, nor has an item a quest evolves into; every other item has one"), *Pointer));
		}
		if (bEvolutionOnly != (Item.Cost == 0.0))
		{
			Problems.Add(FString::Printf(TEXT("%s/cost: an item a quest evolves into costs 0, its Gold its base form's; every other item costs more"), *Pointer));
		}
		// A Quest Item's passive sits where an Attunement would (ADR-025 §3).
		const int32 Attunements = Item.Tier == MasterworkTier ? MasterworkAttunements : IsMythical(Item) ? MythicalAttunements : 0;
		if (bQuestItem ? Item.Attunement.Num() > QuestPassives : Attunements != Item.Attunement.Num())
		{
			Problems.Add(FString::Printf(
				TEXT("%s/attunement: a Masterwork has exactly one Attunement, a Mythical exactly two, a Quest Item at most one passive, and no other item any"), *Pointer));
		}
		if (bQuestItem && !bEvolutionOnly && !Tuning.Quests.Contains(Entry.Key))
		{
			Problems.Add(FString::Printf(TEXT("%s/category: a Quest Item has a quest, or a quest evolves into it"), *Pointer));
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
	// A quest belongs to a Quest Item and evolves it into another, which only that quest makes (ADR-025 §3).
	TSet<FVeyraContentId> Evolutions;
	for (const TPair<FVeyraContentId, FVeyraQuestTuning>& Quest : Tuning.Quests)
	{
		const FString Pointer = FString::Printf(TEXT("/quests/%s"), *Quest.Key.ToString());
		const FVeyraItemDefinition* Base = Tuning.Items.Find(Quest.Key);
		const FVeyraItemDefinition* Evolved = Tuning.Items.Find(Quest.Value.EvolvesInto);
		if (!Base || Base->Category != EVeyraItemCategory::Quest)
		{
			Problems.Add(FString::Printf(TEXT("%s: names no Quest Item this file defines"), *Pointer));
		}
		if (!Evolved || Evolved->Category != EVeyraItemCategory::Quest || Quest.Value.EvolvesInto == Quest.Key || Tuning.Quests.Contains(Quest.Value.EvolvesInto))
		{
			Problems.Add(FString::Printf(TEXT("%s/evolvesInto: must name another Quest Item, one without a quest of its own"), *Pointer));
		}
		bool bAlreadyEvolved = false;
		Evolutions.Add(Quest.Value.EvolvesInto, &bAlreadyEvolved);
		if (bAlreadyEvolved)
		{
			Problems.Add(FString::Printf(TEXT("%s/evolvesInto: another quest evolves into %s; one quest makes each"), *Pointer, *Quest.Value.EvolvesInto.ToString()));
		}
	}
	return Problems;
}

double TotalCost(const FVeyraItemsTuning& Tuning, const FVeyraContentId& Item)
{
	// An evolved Quest Item cost what its base form did (ADR-025 §3).
	if (const FVeyraContentId* Base = EvolvesFrom(Tuning, Item))
	{
		return TotalCost(Tuning, *Base);
	}
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

const FVeyraContentId* EvolvesFrom(const FVeyraItemsTuning& Tuning, const FVeyraContentId& Item)
{
	for (const TPair<FVeyraContentId, FVeyraQuestTuning>& Quest : Tuning.Quests)
	{
		if (Quest.Value.EvolvesInto == Item)
		{
			return &Quest.Key;
		}
	}
	return nullptr;
}

bool IsEvolutionOnly(const FVeyraItemsTuning& Tuning, const FVeyraContentId& Item)
{
	return EvolvesFrom(Tuning, Item) != nullptr;
}

FVeyraContentId QuestLine(const FVeyraItemsTuning& Tuning, const FVeyraContentId& Item)
{
	const FVeyraItemDefinition* Definition = Tuning.Items.Find(Item);
	if (!Definition || Definition->Category != EVeyraItemCategory::Quest)
	{
		return FVeyraContentId();
	}
	// Validate keeps evolutions one step deep, so the base form names the line.
	const FVeyraContentId* Base = EvolvesFrom(Tuning, Item);
	return Base ? *Base : Item;
}
}
