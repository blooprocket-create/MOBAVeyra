// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Damage/VeyraDamageTypes.h"
#include "UObject/ObjectMacros.h"

#include "VeyraAbilitiesTuning.generated.h"

// The Abilities domain's tuning, bound from Game/Tuning/Abilities.json (ADR-006 §6). The schema
// holds every range; a 0 here only means "not loaded".

/** One targeted, instant ability that deals one damage component (Combat Bible §29). */
USTRUCT()
struct FVeyraTargetedDamageAbilityTuning
{
	GENERATED_BODY()

	UPROPERTY()
	double CastRange = 0.0;

	UPROPERTY()
	double CooldownSeconds = 0.0;

	UPROPERTY()
	double ResourceCost = 0.0;

	UPROPERTY()
	EVeyraDamageType DamageType = EVeyraDamageType::Physical;

	UPROPERTY()
	double DamageAmount = 0.0;
};

USTRUCT()
struct FVeyraAbilitiesTuning
{
	GENERATED_BODY()

	/** The Abilities.json format this build reads (a schema version marker, not tuning). */
	static constexpr int32 SchemaVersion = 1;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraTargetedDamageAbilityTuning> TargetedDamage;
};
