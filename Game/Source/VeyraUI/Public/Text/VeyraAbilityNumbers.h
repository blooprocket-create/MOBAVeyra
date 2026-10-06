// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"

/**
 * An ability's numbers beside its text (ADR-065 §7). Gameplay works them out, with the formula a cast prepares its damage
 * with (VeyraAbilityRules::DamageAmount) and the damage parts its tuning holds (VeyraAbilityRules::DamageParts); this
 * writes them, each part under its role's name from the text table, which holds no numbers (ADR-010).
 */
namespace VeyraAbilityNumbers
{
	/**
	 * At Rank, with an attacker's powers now, before the target's defences: its cooldown and its cost in ResourceName,
	 * then each damage part, as "On the one held: 180 magic damage (130 + 50% Magic Power)".
	 */
	VEYRAUI_API TArray<FString> AtRank(const FVeyraContentId& Ability, int32 Rank, double PhysicalPower, double MagicPower, const FString& ResourceName);

	/** Every rank at once, for a Vanguard not in play: each damage part as "On the one held: 60 / 95 / 130 (+50% Magic Power) magic damage". */
	VEYRAUI_API TArray<FString> ByRank(const FVeyraContentId& Ability);
}
