// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "UObject/ObjectMacros.h"

#include "VeyraCombatTuning.generated.h"

/** Armor and Magic Resistance mitigation (Combat Bible §3). */
USTRUCT()
struct FVeyraResistanceTuning
{
	GENERATED_BODY()

	/**
	 * K in "damage taken = raw × K / (K + resistance)". Bound from Game/Tuning/Combat.json, whose
	 * schema requires it to be greater than 0; 0 here only means "not loaded".
	 */
	UPROPERTY()
	double MitigationConstant = 0.0;
};

/** Server-side checks of whether a target is valid and in range (Combat Bible §30, §40). */
USTRUCT()
struct FVeyraTargetingTuning
{
	GENERATED_BODY()

	/**
	 * Extra range, in units, the server allows on top of a cast range, so a target that just left
	 * range on the caster's screen is still accepted (Combat Bible §30).
	 */
	UPROPERTY()
	double ServerRangeTolerance = 0.0;
};

/** The Combat domain's tuning, bound from Game/Tuning/Combat.json (ADR-006 §6). */
USTRUCT()
struct FVeyraCombatTuning
{
	GENERATED_BODY()

	/** The Combat.json format this build reads (a schema version marker, not tuning). */
	static constexpr int32 SchemaVersion = 1;

	UPROPERTY()
	FVeyraResistanceTuning Resistance;

	UPROPERTY()
	FVeyraTargetingTuning Targeting;
};
