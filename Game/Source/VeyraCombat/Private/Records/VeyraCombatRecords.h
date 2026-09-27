// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

class UAbilitySystemComponent;

/** What Combat records about who fought whom, for Combat State (Combat Bible §28) and assists (§18). */
namespace VeyraCombatRecords
{
	/**
	 * Source damaged Target, or put a status on it. If they are enemy Vanguards, both enter Combat
	 * State and Target credits Source toward an assist. Anything else records nothing. Server only.
	 */
	void NoteHostileAction(UAbilitySystemComponent* Source, UAbilitySystemComponent& Target);
}
