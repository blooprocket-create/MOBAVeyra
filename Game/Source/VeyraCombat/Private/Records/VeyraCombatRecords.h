// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

class UAbilitySystemComponent;

/** What Combat records about who fought whom, for Combat State (Combat Bible §28) and credit (§18). */
namespace VeyraCombatRecords
{
	/**
	 * Source damaged Target, or put a status on it. If Source is a Vanguard and Target an enemy, Target
	 * credits Source toward its death, whatever Target is; if both are Vanguards, both also enter
	 * Combat State. Anything else records nothing. Server only.
	 */
	void NoteHostileAction(UAbilitySystemComponent* Source, UAbilitySystemComponent& Target);
}
