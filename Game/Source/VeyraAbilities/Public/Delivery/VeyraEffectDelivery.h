// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Misc/Optional.h"
#include "Statuses/VeyraStatusTypes.h"
#include "Tuning/VeyraAbilitiesTuning.h"
#include "VeyraCombatVerbs.h"

class AActor;
class UAbilitySystemComponent;

/** What an ability does to each unit it hits, prepared at Commit (Combat Bible §50). */
struct FVeyraPreparedEffects
{
	/** Invalid when the effects deal no damage. */
	FVeyraPreparedDamage Damage;

	TArray<FVeyraStatusSpec> Statuses;
	TOptional<FVeyraDisplacementTuning> Displacement;
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

	/** The statuses Ids name, as Combat applies them; an ID the statuses map lacks is skipped. */
	VEYRAABILITIES_API TArray<FVeyraStatusSpec> StatusSpecs(TConstArrayView<FVeyraContentId> Ids);

	/** Effects for Caster at Rank: damage from the caster's power now, statuses and displacement from data. */
	VEYRAABILITIES_API FVeyraPreparedEffects Prepare(UAbilitySystemComponent& Caster, const FVeyraEffectBundleTuning& Effects, int32 Rank);

	/** Whether Effects do anything. */
	VEYRAABILITIES_API bool IsEmpty(const FVeyraPreparedEffects& Effects);

	/** Applies Effects from Caster to Unit, measuring any displacement from Frame. */
	VEYRAABILITIES_API void Apply(UAbilitySystemComponent& Caster, AActor& Unit, const FVeyraPreparedEffects& Effects, const FVeyraEffectFrame& Frame);
}
