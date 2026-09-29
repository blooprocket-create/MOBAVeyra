// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Inventory/VeyraEquipmentRules.h"

#include "Inventory/VeyraInventoryRules.h"
#include "Tuning/VeyraItemsTuning.h"

namespace VeyraEquipment
{
FVeyraEquipmentStats StatsFor(const FVeyraItemsTuning& Tuning, TConstArrayView<FVeyraInventorySlot> Slots, double BaseAttackSpeed,
	const TMap<FVeyraContentId, int32>& Stacks)
{
	FVeyraEquipmentStats Stats;
	double AttackSpeedFraction = 0.0;
	TArray<FVeyraContentId, TInlineAllocator<6>> Attunements;
	for (const FVeyraInventorySlot& Slot : Slots)
	{
		const FVeyraItemDefinition* Item = Slot.IsEmpty() ? nullptr : Tuning.Items.Find(Slot.Item);
		if (!Item)
		{
			continue;
		}
		Stats.MaxHealth += Item->Stats.Health * Slot.Count;
		Stats.HealthRegen += Item->Stats.HealthRegeneration * Slot.Count;
		Stats.PhysicalPower += Item->Stats.PhysicalPower * Slot.Count;
		Stats.MagicPower += Item->Stats.MagicPower * Slot.Count;
		Stats.AbilityHaste += Item->Stats.AbilityHaste * Slot.Count;
		Stats.MoveSpeed += Item->Stats.MoveSpeed * Slot.Count;
		Stats.MagicPenetrationFlat += Item->Stats.MagicPenetrationFlat * Slot.Count;
		AttackSpeedFraction += Item->Stats.AttackSpeed * Slot.Count;
		// A Masterwork is held once, so each Attunement counts once (ADR-012 §9).
		for (const FVeyraContentId& Attunement : Item->Attunement)
		{
			Attunements.AddUnique(Attunement);
		}
	}

	// The Attunements (Item Bible §8–§9). Bonus Health is what the items add, for Weight of War.
	const double BonusHealth = Stats.MaxHealth;
	for (const FVeyraContentId& Attunement : Attunements)
	{
		const int32 Held = Stacks.FindRef(Attunement);
		if (const FVeyraWeightOfWarTuning* Weight = Tuning.WeightOfWar.Find(Attunement))
		{
			Stats.PhysicalPower += BonusHealth * Weight->BonusHealthFraction;
		}
		if (const FVeyraOverchargeTuning* Overcharge = Tuning.Overcharge.Find(Attunement))
		{
			Stats.MagicPowerFraction += Overcharge->MagicPowerFraction;
		}
		if (const FVeyraStackingAttunementTuning* SpoolUp = Tuning.SpoolUp.Find(Attunement))
		{
			AttackSpeedFraction += SpoolUp->PerStack * FMath::Min(Held, SpoolUp->MaxStacks);
		}
		if (const FVeyraStackingAttunementTuning* Overcycle = Tuning.Overcycle.Find(Attunement))
		{
			Stats.AbilityHaste += Overcycle->PerStack * FMath::Min(Held, Overcycle->MaxStacks);
		}
	}
	Stats.AttackSpeed = BaseAttackSpeed * AttackSpeedFraction;
	return Stats;
}
}
