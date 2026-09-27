// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Templates/SubclassOf.h"

class UAbilitySystemComponent;
class UVeyraPassive;

/** A participant prepared as its Vanguard. */
struct FVeyraPreparedVanguard
{
	bool bPrepared = false;

	/** The started passive, which the caller keeps alive; null for a Vanguard without one. */
	UVeyraPassive* Passive = nullptr;
};

/** The Vanguards domain's verbs (ADR-008 §2, §5). Server only. */
namespace VeyraVanguards
{
	/** The passive class that runs PassiveId, from the passive map that defines it (ADR-008 §5); null if none does. */
	VEYRAVANGUARDS_API TSubclassOf<UVeyraPassive> PassiveClassFor(const FVeyraContentId& PassiveId);

	/**
	 * Makes the participant whose Ability System Component is AbilitySystem into Vanguard, from its
	 * definition (ADR-008 §2): its base stats, the abilities in its slots, its level growth, its basic
	 * attack and its passive, through the components beside the Ability System Component. Call once
	 * per participant. Refused, with bPrepared false, for an unknown Vanguard or a participant missing
	 * a component.
	 */
	VEYRAVANGUARDS_API FVeyraPreparedVanguard PrepareCombatant(UAbilitySystemComponent& AbilitySystem, const FVeyraContentId& Vanguard);
}
