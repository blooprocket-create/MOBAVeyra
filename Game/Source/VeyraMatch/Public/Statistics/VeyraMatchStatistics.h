// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Gold/VeyraGoldComponent.h"

#include "VeyraMatchStatistics.generated.h"

/** Damage by type, as health actually removed (Match Statistics Bible §3). */
USTRUCT()
struct VEYRAMATCH_API FVeyraDamageByType
{
	GENERATED_BODY()

	UPROPERTY()
	double Physical = 0.0;

	UPROPERTY()
	double Magic = 0.0;

	UPROPERTY()
	double TrueDamage = 0.0;

	double Total() const { return Physical + Magic + TrueDamage; }
};

/** Gold earned, by where it came from; the sources sum to the total (Match Statistics Bible §7). */
USTRUCT()
struct VEYRAMATCH_API FVeyraGoldBySource
{
	GENERATED_BODY()

	/** The Gold every player starts with, as League counts it (ADR-017 §9). */
	UPROPERTY()
	double Starting = 0.0;

	/** Enemy Vanguard kills, First Blood's bonus included. */
	UPROPERTY()
	double Kills = 0.0;

	UPROPERTY()
	double Assists = 0.0;

	/** Fluxborn: last hits and shares of those fought near. */
	UPROPERTY()
	double Minions = 0.0;

	/** Jungle creatures. */
	UPROPERTY()
	double Jungle = 0.0;

	/** Structures and Flux Wells, where their rules pay Gold. */
	UPROPERTY()
	double Objectives = 0.0;

	/** Enemy wards destroyed. */
	UPROPERTY()
	double Wards = 0.0;

	/** The steady income of a live match. */
	UPROPERTY()
	double Passive = 0.0;

	double Total() const { return Starting + Kills + Assists + Minions + Jungle + Objectives + Wards + Passive; }
};

/** Crowd control on enemy Vanguards, by kind, as effective seconds (Match Statistics Bible §4). */
USTRUCT()
struct VEYRAMATCH_API FVeyraCrowdControlByKind
{
	GENERATED_BODY()

	UPROPERTY()
	double Stun = 0.0;

	UPROPERTY()
	double Slow = 0.0;

	/**
	 * Effective seconds of any crowd control: a stun and a slow on one target at once count once, so it
	 * may be less than Stun and Slow together (§4).
	 */
	UPROPERTY()
	double Total = 0.0;
};

/**
 * One player's record of a match (Match Statistics Bible §2–§8; ADR-017 §3), human or bot. Only
 * Match's statistics service writes it, from what Combat, Economy, World and Vision report.
 */
USTRUCT()
struct VEYRAMATCH_API FVeyraPlayerStatistics
{
	GENERATED_BODY()

	// Combat.
	UPROPERTY()
	int32 Kills = 0;

	UPROPERTY()
	int32 Deaths = 0;

	UPROPERTY()
	int32 Assists = 0;

	/** Health removed from enemy Vanguards, apart from everything else (§2). */
	UPROPERTY()
	double VanguardDamage = 0.0;

	/** Health removed from anything, by type. */
	UPROPERTY()
	FVeyraDamageByType DamageDealt;

	/** Health this player lost, by type; what shields absorbed is not in it. */
	UPROPERTY()
	FVeyraDamageByType DamageTaken;

	/** Damage the shields this player gave actually absorbed, on anyone (§3). */
	UPROPERTY()
	double DamageShielded = 0.0;

	/** Health this player actually restored, to itself and to teammates (§3). */
	UPROPERTY()
	double SelfHealing = 0.0;

	UPROPERTY()
	double TeammateHealing = 0.0;

	UPROPERTY()
	FVeyraCrowdControlByKind CrowdControl;

	// Progression and economy.
	UPROPERTY()
	int32 Level = 0;

	UPROPERTY()
	double GoldEarned = 0.0;

	UPROPERTY()
	FVeyraGoldBySource GoldBySource;

	/** Last hits on Fluxborn and on jungle creatures, apart (§2). */
	UPROPERTY()
	int32 MinionKills = 0;

	UPROPERTY()
	int32 JungleKills = 0;

	// Objectives.
	/** Health removed from enemy towers: lane Spires and base towers (§2). */
	UPROPERTY()
	double TowerDamage = 0.0;

	/** Flux Wells its side secured while it worked on them, damage it dealt to Wells, and the securing last hits it landed (§5). */
	UPROPERTY()
	int32 WellsSecured = 0;

	UPROPERTY()
	double WellDamage = 0.0;

	UPROPERTY()
	int32 WellFinalHits = 0;

	// Vision.
	UPROPERTY()
	int32 WardsPlaced = 0;

	UPROPERTY()
	int32 WardsDestroyed = 0;

	// The final equipment (§8).
	/** Each inventory slot's item, in order; invalid for an empty slot. */
	UPROPERTY()
	TArray<FVeyraContentId> Items;

	/** Each Flux Spell slot's spell, in order; invalid for an empty slot. */
	UPROPERTY()
	TArray<FVeyraContentId> FluxSpells;
};

/** A time span, in server world time. */
struct FVeyraSpan
{
	double Start = 0.0;
	double End = 0.0;
};

/** The statistics' arithmetic, as plain rules (ADR-017 §3). */
namespace VeyraStatisticsRules
{
	/** How much time Spans cover together: overlaps count once (Match Statistics Bible §4). */
	VEYRAMATCH_API double UnionSeconds(TArray<FVeyraSpan> Spans);

	/**
	 * Adds Amount to the source Reason counts as, when it is earned Gold (§7): sales, undone purchases
	 * and developer grants are not earned. Returns whether it counted.
	 */
	VEYRAMATCH_API bool AddEarned(FVeyraGoldBySource& Sources, EVeyraGoldReason Reason, double Amount);
}
