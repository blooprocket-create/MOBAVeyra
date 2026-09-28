// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Inventory/VeyraEquipmentRules.h"

#include "Inventory/VeyraInventoryRules.h"
#include "Tuning/VeyraItemsTuning.h"

namespace VeyraEquipment
{
FVeyraEquipmentStats StatsFor(const FVeyraItemsTuning& Tuning, TConstArrayView<FVeyraInventorySlot> Slots, double BaseAttackSpeed)
{
	FVeyraEquipmentStats Stats;
	double AttackSpeedFraction = 0.0;
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
	}
	Stats.AttackSpeed = BaseAttackSpeed * AttackSpeedFraction;
	return Stats;
}
}
