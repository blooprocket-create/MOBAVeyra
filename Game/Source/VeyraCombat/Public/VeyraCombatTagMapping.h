// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Absorption/VeyraAbsorptionLedger.h"
#include "Damage/VeyraDamageTypes.h"
#include "GameplayTagContainer.h"
#include "Misc/Optional.h"

/** The one mapping between Combat's enums and the native tag vocabulary (VeyraCore Tags/). */
namespace VeyraCombatTagMapping
{
	VEYRACOMBAT_API FGameplayTag DamageTypeTag(EVeyraDamageType Type);
	VEYRACOMBAT_API TOptional<EVeyraDamageType> DamageTypeFromTag(const FGameplayTag& Tag);

	VEYRACOMBAT_API FGameplayTag ShieldCategoryTag(EVeyraShieldCategory Category);
	VEYRACOMBAT_API TOptional<EVeyraShieldCategory> ShieldCategoryFromTag(const FGameplayTag& Tag);
}
