// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

class UAbilitySystemComponent;

/** How a death is finalized (Combat Bible §18, §44). Called by the vitals set when Health reaches 0. */
namespace VeyraDeath
{
	/**
	 * Marks the victim dead, ends its temporary effects (§44) and announces the death. Permanent
	 * (infinite) effects and cooldowns stay: they persist through death. Server only; does nothing
	 * for a victim already dead. LethalUnit dealt the lethal damage; its owner answers for an owned unit's.
	 */
	void FinalizeDeath(UAbilitySystemComponent& Victim, UAbilitySystemComponent* LethalUnit);

	/**
	 * The unit leaves the battleground dead, as a death does, but at nobody's hand: its records and
	 * temporary effects end and no death is announced (ADR-034 §3). Server only; false for a unit already
	 * dead or with no life to end.
	 */
	bool Withdraw(UAbilitySystemComponent& Unit);
}
