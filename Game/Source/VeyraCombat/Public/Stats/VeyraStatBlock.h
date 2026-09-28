// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "UObject/ObjectMacros.h"

#include "VeyraStatBlock.generated.h"

/**
 * A unit's base stats, or a change to them (ADR-008 §2). Vanguard definitions bind their base stats
 * to it from tuning, and Progression builds each level's growth as one. Values are plain numbers;
 * VeyraCombat::InitializeStats and GrowBaseStats validate and apply them.
 */
USTRUCT()
struct VEYRACOMBAT_API FVeyraStatBlock
{
	GENERATED_BODY()

	UPROPERTY()
	double MaxHealth = 0.0;

	/** Health restored per second (Combat Bible §6). */
	UPROPERTY()
	double HealthRegen = 0.0;

	/** 0 for a unit with no resource (Combat Bible §27). */
	UPROPERTY()
	double MaxResource = 0.0;

	/** Resource restored per second. */
	UPROPERTY()
	double ResourceRegen = 0.0;

	UPROPERTY()
	double Armor = 0.0;

	UPROPERTY()
	double MagicResist = 0.0;

	UPROPERTY()
	double PhysicalPower = 0.0;

	UPROPERTY()
	double MagicPower = 0.0;

	/** Attacks per second. */
	UPROPERTY()
	double AttackSpeed = 0.0;

	UPROPERTY()
	double MoveSpeed = 0.0;
};
