// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraVolleyAbility.h"

#include "AbilitySystemComponent.h"
#include "Delivery/VeyraVolleySubsystem.h"
#include "Engine/World.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Tuning/VeyraAbilitiesTuning.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

bool UVeyraVolleyAbility::Defines(const FVeyraContentId& Ability) const
{
	return UVeyraAbilitiesTuningSubsystem::FindVolley(Ability) != nullptr;
}

double UVeyraVolleyAbility::GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraVolleyAbilityTuning* Volley = UVeyraAbilitiesTuningSubsystem::FindVolley(Ability);
	return Volley ? VeyraAbilityRules::ValueAtRank(Volley->Cast.ResourceCostByRank, Rank) : 0.0;
}

double UVeyraVolleyAbility::GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraVolleyAbilityTuning* Volley = UVeyraAbilitiesTuningSubsystem::FindVolley(Ability);
	return Volley ? VeyraAbilityRules::ValueAtRank(Volley->Cast.CooldownSecondsByRank, Rank) : 0.0;
}

const FVeyraCastTuning* UVeyraVolleyAbility::GetCastTuning(const FVeyraContentId& Ability) const
{
	const FVeyraVolleyAbilityTuning* Volley = UVeyraAbilitiesTuningSubsystem::FindVolley(Ability);
	return Volley ? &Volley->Cast : nullptr;
}

bool UVeyraVolleyAbility::IsOffensive(const FVeyraContentId& /*Ability*/) const
{
	// Opening the lane harms no one; its shots are the offensive casts.
	return false;
}

FVeyraChannelPlan UVeyraVolleyAbility::Deliver(const FVeyraCast& Cast)
{
	const FVeyraVolleyAbilityTuning* Volley = UVeyraAbilitiesTuningSubsystem::FindVolley(Cast.Ability);
	UAbilitySystemComponent* Caster = Cast.Caster.Get();
	const UVeyraAbilityLoadoutComponent* Loadout = Caster && Caster->GetOwner() ? Caster->GetOwner()->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
	const FVeyraLoadoutEntry* Entry = Loadout ? Loadout->FindAbility(Cast.Ability) : nullptr;
	UVeyraVolleySubsystem* Volleys = GetWorld() ? GetWorld()->GetSubsystem<UVeyraVolleySubsystem>() : nullptr;
	if (Volley && Caster && Entry && Volleys)
	{
		Volleys->Open(*Caster, Entry->Slot, Cast.Ability, *Volley, Cast.Direction, GetCasterLevel(*Caster));
	}
	return FVeyraChannelPlan();
}
