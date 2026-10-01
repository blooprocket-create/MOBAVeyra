// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraStanceAbility.h"

#include "AbilitySystemComponent.h"
#include "Algo/AllOf.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraAbilitiesLog.h"

bool UVeyraStanceAbility::Defines(const FVeyraContentId& Ability) const
{
	return UVeyraAbilitiesTuningSubsystem::FindStance(Ability) != nullptr;
}

double UVeyraStanceAbility::GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraStanceAbilityTuning* Stance = UVeyraAbilitiesTuningSubsystem::FindStance(Ability);
	return Stance ? VeyraAbilityRules::ValueAtRank(Stance->Cast.ResourceCostByRank, Rank) : 0.0;
}

double UVeyraStanceAbility::GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraStanceAbilityTuning* Stance = UVeyraAbilitiesTuningSubsystem::FindStance(Ability);
	return Stance ? VeyraAbilityRules::ValueAtRank(Stance->Cast.CooldownSecondsByRank, Rank) : 0.0;
}

const FVeyraCastTuning* UVeyraStanceAbility::GetCastTuning(const FVeyraContentId& Ability) const
{
	const FVeyraStanceAbilityTuning* Stance = UVeyraAbilitiesTuningSubsystem::FindStance(Ability);
	return Stance ? &Stance->Cast : nullptr;
}

FVeyraChannelPlan UVeyraStanceAbility::Deliver(const FVeyraCast& Cast)
{
	const FVeyraStanceAbilityTuning* Stance = UVeyraAbilitiesTuningSubsystem::FindStance(Cast.Ability);
	UAbilitySystemComponent* Caster = Cast.Caster.Get();
	UVeyraAbilityLoadoutComponent* Loadout = Caster && Caster->GetOwner() ? Caster->GetOwner()->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
	if (!Stance || !Loadout)
	{
		return FVeyraChannelPlan();
	}
	// In the stance when every slot it names holds its ability already: casting then puts back what each
	// slot stowed.
	const bool bInStance = Algo::AllOf(Stance->Slots, [Loadout](const FVeyraStanceSlotTuning& Held) {
		const FVeyraLoadoutEntry* Own = Loadout->FindOwnSlot(Held.Slot);
		return Own && Own->Ability == Held.Ability;
	});
	for (const FVeyraStanceSlotTuning& Held : Stance->Slots)
	{
		const FVeyraLoadoutEntry* Stowed = bInStance ? Loadout->FindStowed(Held.Slot) : nullptr;
		if (bInStance && !Stowed)
		{
			continue;
		}
		// Copied: the swap moves the entry it names.
		const FVeyraContentId Next = bInStance ? Stowed->Ability : Held.Ability;
		Loadout->SwapOwn(*Caster, Held.Slot, Next);
	}
	UE_LOG(LogVeyraAbilities, Verbose, TEXT("%s %s %s (cast %d)."), *GetNameSafe(Caster->GetOwner()), bInStance ? TEXT("leaves") : TEXT("takes"),
		*Cast.Ability.ToString(), Cast.CastId);
	return FVeyraChannelPlan();
}

bool UVeyraStanceAbility::IsOffensive(const FVeyraContentId& /*Ability*/) const
{
	// Changing stance threatens no one, so it keeps its caster hidden (ADR-030 §1).
	return false;
}
