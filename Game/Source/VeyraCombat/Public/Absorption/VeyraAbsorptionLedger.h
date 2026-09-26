// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Damage/VeyraDamageTypes.h"
#include "UObject/ObjectMacros.h"

#include "VeyraAbsorptionLedger.generated.h"

/** The three shield categories (Combat Bible §7). */
UENUM()
enum class EVeyraShieldCategory : uint8
{
	/** Absorbs Physical Damage only. */
	Physical,
	/** Absorbs Magic Damage only. */
	Magic,
	/** Absorbs Physical, Magic and True Damage. */
	Universal,
};

/** One active shield. */
USTRUCT()
struct FVeyraShieldEntry
{
	GENERATED_BODY()

	/** Identifies the entry and orders it by age: a smaller Sequence is older. */
	UPROPERTY()
	int32 Sequence = 0;

	UPROPERTY()
	EVeyraShieldCategory Category = EVeyraShieldCategory::Universal;

	UPROPERTY()
	double Remaining = 0.0;
};

/** One active grant of Temporary Health (Combat Bible §7, "Temporary Health"). */
USTRUCT()
struct FVeyraTemporaryHealthGrant
{
	GENERATED_BODY()

	/** Identifies the grant and orders it by age: a smaller Sequence is older. */
	UPROPERTY()
	int32 Sequence = 0;

	UPROPERTY()
	double Remaining = 0.0;
};

/** Everything that absorbs damage before a unit's ordinary Health. */
USTRUCT()
struct FVeyraAbsorptionLedger
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FVeyraShieldEntry> Shields;

	UPROPERTY()
	TArray<FVeyraTemporaryHealthGrant> TemporaryHealth;
};

/** What one damage component did in steps 7–9. */
struct FVeyraAbsorptionResult
{
	/** True when Invulnerability stopped the component at step 7; nothing was consumed. */
	bool bBlockedByInvulnerability = false;

	double ShieldAbsorbed = 0.0;
	double TemporaryHealthSpent = 0.0;
	double HealthLost = 0.0;

	/** Damage beyond the remaining ordinary Health. */
	double Overkill = 0.0;

	/** Shields and grants this component emptied. They have been removed from the ledger. */
	TArray<int32> DepletedShields;
	TArray<int32> DepletedTemporaryHealth;
};

/** Combat Bible §25 steps 7–9 and the Health that Temporary Health adds (§7). */
namespace VeyraAbsorption
{
	/**
	 * Resolves one mitigated damage component against a unit:
	 * - step 7: an Invulnerable unit consumes nothing;
	 * - step 8: shields, most specialized category first (Physical or Magic, then Universal; True
	 *   Damage uses Universal only), and the oldest first within a category;
	 * - step 9: Temporary Health grants, oldest first, for every damage type, then ordinary Health,
	 *   which never goes below 0.
	 * Updates the ledger in place and removes emptied entries. Health is the unit's ordinary Health
	 * before this component.
	 */
	VEYRACOMBAT_API FVeyraAbsorptionResult Absorb(EVeyraDamageType Type, double Amount, bool bInvulnerable,
		FVeyraAbsorptionLedger& Ledger, double Health);

	/** The remaining Temporary Health across all grants. */
	VEYRACOMBAT_API double TotalTemporaryHealth(const FVeyraAbsorptionLedger& Ledger);

	/** Current Health including Temporary Health, which counts as Health (§7). */
	VEYRACOMBAT_API double GetEffectiveHealth(double Health, const FVeyraAbsorptionLedger& Ledger);

	/** Max Health including Temporary Health, which raises it by the amount that remains (§7). */
	VEYRACOMBAT_API double GetEffectiveMaxHealth(double MaxHealth, const FVeyraAbsorptionLedger& Ledger);
}
