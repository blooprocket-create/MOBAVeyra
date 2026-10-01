// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

class UAbilitySystemComponent;

/** What Combat records about who fought whom, for Combat State (Combat Bible §28) and credit (§18). */
namespace VeyraCombatRecords
{
	/**
	 * Dealer damaged Target, or put a status on it; the unit it answers to acts (ADR-034 §1), so an owned
	 * unit's action is its owner's. If that is a Vanguard and Target an enemy, Target credits it toward its
	 * death, whatever Target is; if both are Vanguards, both also enter Combat State. Anything else records
	 * nothing. Server only.
	 */
	void NoteHostileAction(UAbilitySystemComponent* Dealer, UAbilitySystemComponent& Target);
}
