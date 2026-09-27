// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
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

/** What a shield grant does to an active shield with the same identity from the same source (Combat Bible §7). */
UENUM()
enum class EVeyraShieldReapply : uint8
{
	/** The grant replaces the shield: its amount and duration start again. */
	Replace,
	/** The grant adds to what the shield has left, up to its maximum, and its duration starts again. */
	Merge,
};

/**
 * One shield grant (ADR-009 §3), with its amounts already worked out by the caller from its data,
 * after the §51 modifiers.
 */
struct FVeyraShieldGrant
{
	/** The shield's identity. The same identity from the same source is one shield; no identity never merges. */
	FVeyraContentId Id;

	EVeyraShieldCategory Category = EVeyraShieldCategory::Universal;

	/** What this grant adds; above 0. */
	double Amount = 0.0;

	double DurationSeconds = 0.0;

	EVeyraShieldReapply Reapply = EVeyraShieldReapply::Replace;

	/** The most the shield can hold after merging; at least Amount. */
	double MaxAmount = 0.0;

	/**
	 * An optional group whose shields from one source together hold at most CapGroupTotal on a unit,
	 * such as Cairn's passive and ultimate shields. No group, and a total of 0, when the shield has none.
	 */
	FVeyraContentId CapGroup;

	double CapGroupTotal = 0.0;
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
