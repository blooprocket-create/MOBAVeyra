// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Misc/EnumClassFlags.h"
#include "UObject/ObjectMacros.h"

#include "VeyraStatusTypes.generated.h"

/**
 * What a status does (ADR-009 §1). Each kind reads its Magnitude in one way, given beside it. The
 * stat kinds change their attribute through one percentage modifier (Combat Bible §41).
 */
UENUM()
enum class EVeyraStatusKind : uint8
{
	/** Cannot move, basic attack or cast (Combat Bible §8). Crowd control. Magnitude: 0. */
	Stun,
	/** Reduces Movement Speed; the strongest Slow controls it (§8, §23). Crowd control. Magnitude: the fraction removed, above 0 and below 1. */
	Slow,
	/** Changes Move Speed. Not crowd control, even when negative. Magnitude: the change per stack, above -1 and not 0; 0.2 is 20% faster. */
	MoveSpeed,
	/** Changes Attack Speed. Magnitude: the change per stack, above -1 and not 0. */
	AttackSpeed,
	/** Shortens crowd control that lands later (§8). Magnitude: the fraction removed per stack, above 0 and below 1. */
	Tenacity,
	/** Reduces damage taken (§15). Magnitude: the fraction removed per stack, above 0 and below 1. */
	DamageReduction,
	/** Shortens forced displacement (§9). Magnitude: the fraction removed per stack, above 0 and below 1. */
	DisplacementResistance,
	/**
	 * Basic attacks also hit the other enemies in the attacker's cleave area for part of their damage
	 * (ADR-009 §5); the strongest applies. Not crowd control. Magnitude: that fraction, above 0 and at most 1.
	 */
	AttackCleave,
};

/** How a new application meets an active status with the same ID (Combat Bible §46). */
UENUM()
enum class EVeyraStackingPolicy : uint8
{
	/** One instance on the unit; a new application replaces it and restarts its duration. */
	UniqueRefresh,
	/** One instance on the unit; a new application replaces it only if stronger, or equally strong and lasting longer. */
	UniqueReplaceStrongest,
	/** One instance on the unit; each application adds a stack, up to MaxStacks, and restarts the duration. */
	Stacking,
	/** One instance per source; each source's refreshes as UniqueRefresh. */
	IndependentSources,
};

/** The actions a unit's statuses stop it taking (Combat Bible §8). */
enum class EVeyraActionBlocks : uint8
{
	None = 0,
	Move = 1 << 0,
	Attack = 1 << 1,
	Cast = 1 << 2,
};
ENUM_CLASS_FLAGS(EVeyraActionBlocks);

/**
 * One status an effect applies, as its data declares it. Abilities' tuning binds these records;
 * Combat checks them (VeyraStatuses::Validate) before applying one.
 */
USTRUCT()
struct VEYRACOMBAT_API FVeyraStatusSpec
{
	GENERATED_BODY()

	/** The status's stable identity (Combat Bible §46). The same ID is the same status for stacking. */
	UPROPERTY()
	FVeyraContentId Id;

	UPROPERTY()
	EVeyraStatusKind Kind = EVeyraStatusKind::Stun;

	UPROPERTY()
	EVeyraStackingPolicy Stacking = EVeyraStackingPolicy::UniqueRefresh;

	/** Read as its kind declares. */
	UPROPERTY()
	double Magnitude = 0.0;

	/** Before Tenacity, which shortens only crowd control. */
	UPROPERTY()
	double DurationSeconds = 0.0;

	/** The most stacks the status can hold: at least 1, and exactly 1 unless it stacks. */
	UPROPERTY()
	int32 MaxStacks = 1;

	/**
	 * Seconds each takedown by the unit adds to the status's remaining time (ADR-009 §1), up to
	 * TakedownExtensionMaxSeconds in all. Both 0 for a status takedowns do not extend.
	 */
	UPROPERTY()
	double TakedownExtensionSeconds = 0.0;

	UPROPERTY()
	double TakedownExtensionMaxSeconds = 0.0;
};

/** One active status as every machine sees it. Replicated for presentation. */
USTRUCT()
struct FVeyraStatusEntry
{
	GENERATED_BODY()

	/** Identifies the entry for as long as it lasts, through refreshes and new stacks. */
	UPROPERTY()
	int32 Sequence = 0;

	UPROPERTY()
	FVeyraContentId Id;

	UPROPERTY()
	EVeyraStatusKind Kind = EVeyraStatusKind::Stun;

	/** Per stack. */
	UPROPERTY()
	double Magnitude = 0.0;

	UPROPERTY()
	int32 Stacks = 1;

	/** When the current application began and when it ends, in the server's world time. */
	UPROPERTY()
	double StartedAt = 0.0;

	UPROPERTY()
	double EndsAt = 0.0;
};

/** A unit's active statuses. */
USTRUCT()
struct FVeyraStatusLedger
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FVeyraStatusEntry> Entries;
};

/** Combat's status rules (Combat Bible §8, §46), as plain functions. */
namespace VeyraStatuses
{
	/** Problems with Spec, each a field name and a message; empty when it can be applied. */
	VEYRACOMBAT_API TArray<FString> Validate(const FVeyraStatusSpec& Spec);

	/** Whether Tenacity shortens the kind: crowd control does, buffs and speed changes do not (§8). */
	VEYRACOMBAT_API bool IsTenacityReducible(EVeyraStatusKind Kind);

	/**
	 * A reducible duration after Tenacity: DurationSeconds times TenacityRetained, but never below
	 * FloorSeconds, and never longer than it started (§8).
	 */
	VEYRACOMBAT_API double ApplyTenacity(double DurationSeconds, double TenacityRetained, double FloorSeconds);

	/**
	 * The multiplier a stat kind's modifier applies for Stacks stacks: 1 + Magnitude × Stacks for a
	 * change, 1 - Magnitude × Stacks for a reduction. Unused for Stun and Slow.
	 */
	VEYRACOMBAT_API double StatMultiplier(EVeyraStatusKind Kind, double Magnitude, int32 Stacks);

	/** Whether a new application replaces an active one under UniqueReplaceStrongest. */
	VEYRACOMBAT_API bool IsStronger(const FVeyraStatusEntry& Active, double Magnitude, double EndsAt);

	/** The largest magnitude among the entries of Kind; 0 when there is none. */
	VEYRACOMBAT_API double Strongest(TConstArrayView<FVeyraStatusEntry> Entries, EVeyraStatusKind Kind);

	/** The fraction of speed the strongest Slow removes; 0 when there is none (§8). */
	VEYRACOMBAT_API double StrongestSlow(TConstArrayView<FVeyraStatusEntry> Entries);

	/** The actions the entries block. */
	VEYRACOMBAT_API EVeyraActionBlocks ActionBlocks(TConstArrayView<FVeyraStatusEntry> Entries);
}
