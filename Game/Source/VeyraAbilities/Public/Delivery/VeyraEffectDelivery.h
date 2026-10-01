// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Attacks/VeyraBasicAttackTypes.h"
#include "Events/VeyraAbilityEvents.h"
#include "Misc/Optional.h"
#include "Statuses/VeyraStatusTypes.h"
#include "Tuning/VeyraAbilitiesTuning.h"
#include "VeyraCombatVerbs.h"

class AActor;
class UAbilitySystemComponent;

/** A reaction's burst prepared at Commit, its damage from the caster's offence then (ADR-034 §6). */
struct FVeyraPreparedBurst
{
	FVeyraShape Shape;

	/** Invalid when the burst deals no damage. */
	FVeyraPreparedDamage Damage;

	TArray<FVeyraStatusSpec> Statuses;
};

/** A reaction prepared at Commit: its damage worked out from the caster's power then (ADR-026 §1). */
struct FVeyraPreparedReaction
{
	FVeyraContentId Status;
	bool bConsume = false;
	bool bPerStack = false;
	FVeyraDamageComponents Damage;
	TArray<FVeyraStatusSpec> Statuses;
	TArray<FVeyraContentId> Replaces;

	/** Around the target as it reacts, sparing it; at most one. */
	TArray<FVeyraPreparedBurst> Burst;
};

/** What an ability does to each unit it hits, prepared at Commit (Combat Bible §50). */
struct FVeyraPreparedEffects
{
	/** Invalid when the effects deal no damage. */
	FVeyraPreparedDamage Damage;

	TArray<FVeyraStatusSpec> Statuses;
	TOptional<FVeyraDisplacementTuning> Displacement;

	/** Damage for the target's missing Health, read when the hit lands; it joins Damage. */
	TOptional<FVeyraMissingHealthDamageTuning> MissingHealthDamage;

	/** Damage's components as prepared, and what multiplies them against a kind of unit as the hit lands. */
	FVeyraDamageComponents RawDamage;
	TArray<FVeyraUnitKindMultiplierTuning> UnitKindMultipliers;

	/** Statuses that spare a unit the displacement. */
	TArray<FVeyraContentId> DisplacementUnlessStatuses;

	/** What the hit adds against statuses its target holds, in order. */
	TArray<FVeyraPreparedReaction> Reactions;

	/**
	 * For a bundle whose only damage is its reactions': their damage prepared at Commit, each type at 0,
	 * so what they add at impact keeps the caster's offence at Commit (Combat Bible §50).
	 */
	FVeyraPreparedDamage ReactionDamage;
};

/** Where effects are applied from: the point displacements are measured from, and the way they face. */
struct FVeyraEffectFrame
{
	FVector Origin = FVector::ZeroVector;
	FVector Direction = FVector::ForwardVector;

	/** Whether the origin is the caster, so a Pull toward it stops at the caster's edge. */
	bool bOriginIsCaster = false;
};

/** How abilities' effects are prepared and applied (ADR-008 §3). Server only. */
namespace VeyraEffectDelivery
{
	/** One damage component's amount for Caster at Rank: the rank's amount plus the caster's power times the ratios. */
	VEYRAABILITIES_API double DamageAmount(const UAbilitySystemComponent& Caster, const FVeyraDamageTuning& Damage, int32 Rank);

	/**
	 * The statuses Ids name, as Combat applies them, at their source's Level, which Level-scaled values
	 * such as a DoT's damage read; an ID the statuses map lacks is skipped.
	 */
	VEYRAABILITIES_API TArray<FVeyraStatusSpec> StatusSpecs(TConstArrayView<FVeyraContentId> Ids, int32 SourceLevel = 1);

	/**
	 * Shield's grant from Caster at Rank, its amounts worked out now from the caster's stats (Combat
	 * Bible §51): the rank's amount plus Max Health and Magic Power times the ratios, and its maximum
	 * and cap group total as shares of Max Health.
	 */
	VEYRAABILITIES_API FVeyraShieldGrant ShieldGrant(const UAbilitySystemComponent& Caster, const FVeyraShieldTuning& Shield, int32 Rank);

	/**
	 * Server only: grants Holder Shield from Caster at Rank, as ShieldGrant works it out, and has an
	 * absorbed reward it names watched (ADR-027 §5). Returns the shield's effect, invalid if none was granted.
	 */
	VEYRAABILITIES_API FActiveGameplayEffectHandle GrantShield(UAbilitySystemComponent& Caster, UAbilitySystemComponent& Holder, const FVeyraShieldTuning& Shield, int32 Rank);

	/**
	 * Impact as a basic attack's secondary impact from Caster at Rank, its damage from the caster's
	 * power now (ADR-009 §5): an empowered attack's, or a passive's.
	 */
	VEYRAABILITIES_API FVeyraSecondaryImpact SecondaryImpact(const UAbilitySystemComponent& Caster, const FVeyraSecondaryImpactTuning& Impact, int32 Rank);

	/** Effects for Caster at Rank: damage from the caster's power now, statuses and displacement from data. */
	VEYRAABILITIES_API FVeyraPreparedEffects Prepare(UAbilitySystemComponent& Caster, const FVeyraEffectBundleTuning& Effects, int32 Rank);

	/** Whether Effects do anything. */
	VEYRAABILITIES_API bool IsEmpty(const FVeyraPreparedEffects& Effects);

	/**
	 * Whether Effects would do anything to Target as they land: damage, a status or a displacement of
	 * their own, or a reaction to a status it holds. A Spell Shield is spent only by such a hit (ADR-025 §4).
	 */
	VEYRAABILITIES_API bool WouldLandOn(const FVeyraPreparedEffects& Effects, const UAbilitySystemComponent& Target);

	/**
	 * Applies Effects from Caster to Unit, measuring any displacement from Frame, and announces the hit
	 * with the control that landed (On Ability Hit) when Source names an ability.
	 */
	VEYRAABILITIES_API void Apply(UAbilitySystemComponent& Caster, AActor& Unit, const FVeyraPreparedEffects& Effects, const FVeyraEffectFrame& Frame,
		const FVeyraAbilityHitSource& Source);
}
