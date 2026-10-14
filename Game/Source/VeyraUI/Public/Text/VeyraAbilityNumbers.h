// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Internationalization/Text.h"

/** What a tooltip's value is, for its colour (ADR-080 §2): damage by its type, crowd control and harm, help, or plain. */
enum class EVeyraFactTone : uint8
{
	Physical,
	Magic,
	TrueDamage,
	Harm,
	Help,
	Plain,
};

/** One line of an ability's tooltip (ADR-080 §2): its role, its value now, and how that value is made. */
struct FVeyraAbilityFact
{
	/** The role's name from the text table: "Damage", "Area", "On contact". */
	FText Label;
	/** The value now: "72 physical", "Stunned 1 s", "Slowed 30%". */
	FString Value;
	/** How it is made, or for how long: "60 + 20% of your 58 Physical Power", "for 1.5 s"; empty for nothing to add. */
	FString Note;
	EVeyraFactTone Tone = EVeyraFactTone::Plain;
};

/** An ability at a rank, as its tooltip shows it (ADR-080 §2). */
struct FVeyraAbilitySummary
{
	/** The cooldown after the caster's Ability Haste, and its words: "10.9 s"; empty for none. */
	double CooldownSeconds = 0.0;
	FString Cooldown;
	/** The cost in the caster's resource: "55 Mana"; empty for none. */
	FString Cost;
	TArray<FVeyraAbilityFact> Facts;
	/** Each damage part's amount at the next rank, as "80"; empty at the last rank, or for an ability that deals none. */
	FString NextRank;
};

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

	/**
	 * Ability at Rank of MaxRank for a caster with these powers and Ability Haste, before the target's defences, as its tooltip
	 * shows it: the cooldown the caster would wait, the cost, each damage part and shield with how it is made, and each status
	 * its effects put on what they reach. The same formulas a cast uses (VeyraAbilityRules::DamageAmount, ShieldAmount and
	 * VeyraHaste::CooldownMultiplier).
	 */
	VEYRAUI_API FVeyraAbilitySummary Summarise(const FVeyraContentId& Ability, int32 Rank, int32 MaxRank, double PhysicalPower, double MagicPower,
		double MaxHealth, double AbilityHaste, const FString& ResourceName);
}
