// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

class UAbilitySystemComponent;

/** How a death is finalized (Combat Bible §18, §44). Called by the vitals set when Health reaches 0. */
namespace VeyraDeath
{
	/**
	 * Marks the victim dead, ends its temporary effects (§44) and announces the death. Permanent
	 * (infinite) effects and cooldowns stay: they persist through death. Server only; does nothing
	 * for a victim already dead.
	 */
	void FinalizeDeath(UAbilitySystemComponent& Victim, UAbilitySystemComponent* Killer);
}
