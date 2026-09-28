// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Brain/VeyraBotView.h"

/**
 * An ability as a bot uses it (ADR-013 §4): its archetype says how it is aimed, and its tuning how
 * far it reaches and how long it takes to land. No ability is named here.
 */
namespace VeyraBotAbilities
{
	/**
	 * The profile of Ability, from the archetype map that defines it; nothing if none does.
	 * AttackRange is the caster's basic attack reach, which an empowered attack uses.
	 */
	VEYRABOTS_API TOptional<FVeyraBotAbilityProfile> ProfileOf(const FVeyraContentId& Ability, double AttackRange);
}
