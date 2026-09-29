// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Battleground/VeyraBattlegroundTypes.h"
#include "Damage/VeyraDamageTypes.h"
#include "Tuning/VeyraTuningProvenance.h"
#include "UObject/ObjectMacros.h"

#include "VeyraWorldTuning.generated.h"

/** A point on the battleground's floor, in world units (X up the screen, Y to its right). */
USTRUCT()
struct FVeyraMapPoint
{
	GENERATED_BODY()

	UPROPERTY()
	double X = 0.0;

	UPROPERTY()
	double Y = 0.0;
};

/**
 * One Fluxway (Battleground Bible §2, §4) and Team A's structures on it (§5, §10). The lane runs from
 * Team A's base to Team B's; Team B's structures are Team A's mirrored (VeyraLayout::Mirror).
 */
USTRUCT()
struct FVeyraLaneLayout
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraLane Lane = EVeyraLane::Mid;

	/** The lane's path from Team A's base to Team B's, at least two points. */
	UPROPERTY()
	TArray<FVeyraMapPoint> Points;

	/** How wide the lane's road is drawn, in units. */
	UPROPERTY()
	double Width = 0.0;

	/** Where Team A's inhibitor stands: its distance along the lane from Team A's end. */
	UPROPERTY()
	double InhibitorDistance = 0.0;

	/**
	 * Where Team A's Spires stand: their distances along the lane beyond the inhibitor, strictly
	 * rising, so the last is the outer Spire, the first to fall (Battleground Bible §10).
	 */
	UPROPERTY()
	TArray<double> SpireDistances;
};

/** Team A's base (Battleground Bible §3, §12, §18); Team B's is its mirror. */
USTRUCT()
struct FVeyraBaseLayout
{
	GENERATED_BODY()

	UPROPERTY()
	FVeyraMapPoint PrimeWell;

	/** The base-defense towers before the Prime Well (§18). */
	UPROPERTY()
	TArray<FVeyraMapPoint> BaseTowers;

	/** Where the team's Vanguards start and respawn. */
	UPROPERTY()
	FVeyraMapPoint Fountain;

	/** The base's pad, drawn around the Prime Well, in units. */
	UPROPERTY()
	double PadRadius = 0.0;

	/** How far from the fountain point a Vanguard counts as at its fountain (§12), in units. */
	UPROPERTY()
	double FountainRadius = 0.0;
};

/**
 * The battleground's grey-box layout (ADR-011 §12): the one source for the generated map and the
 * server's spawning. Team B's half is Team A's reflected across the river's diagonal, the line
 * Y = -X, which maps every lane onto itself and swaps the bases, so both teams' distances match.
 */
USTRUCT()
struct FVeyraBattlegroundLayout
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** The floor is a square reaching this far from the centre in each direction, in units. */
	UPROPERTY()
	double HalfExtent = 0.0;

	/** How wide the river is drawn along its diagonal, in units. */
	UPROPERTY()
	double RiverWidth = 0.0;

	UPROPERTY()
	TArray<FVeyraLaneLayout> Lanes;

	UPROPERTY()
	FVeyraBaseLayout Base;
};

/** One kind of structure's stats and body (Combat Bible §33: structures have their own Armor and MR). */
USTRUCT()
struct FVeyraStructureTuning
{
	GENERATED_BODY()

	UPROPERTY()
	double MaxHealth = 0.0;

	UPROPERTY()
	double Armor = 0.0;

	UPROPERTY()
	double MagicResist = 0.0;

	/** The body's capsule, in units: what attacks reach and what blocks movement. */
	UPROPERTY()
	double CapsuleRadius = 0.0;

	UPROPERTY()
	double CapsuleHalfHeight = 0.0;
};

/** Every kind of structure's stats (Battleground Bible §5, §10, §18). */
USTRUCT()
struct FVeyraStructuresTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	FVeyraStructureTuning LaneSpire;

	UPROPERTY()
	FVeyraStructureTuning BaseTower;

	UPROPERTY()
	FVeyraStructureTuning Inhibitor;

	UPROPERTY()
	FVeyraStructureTuning PrimeWell;
};

/** An inhibitor's reconstruction (Battleground Bible §10, §18). */
USTRUCT()
struct FVeyraInhibitorTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** How long after its destruction an inhibitor reconstructs at full Health, in seconds. */
	UPROPERTY()
	double RebuildSeconds = 0.0;
};

/** The passive Prime Well's regeneration while all its team's inhibitors stand (Battleground Bible §18). */
USTRUCT()
struct FVeyraPrimeWellTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Missing Health restored per second, as a fraction of its Max Health. */
	UPROPERTY()
	double RegenerationFractionPerSecond = 0.0;
};

/** A lane Spire's or base-defense tower's shot (Combat Bible §33, §55). */
USTRUCT()
struct FVeyraTowerAttackTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Physical at current prototype defaults (§55). */
	UPROPERTY()
	EVeyraDamageType DamageType = EVeyraDamageType::Physical;

	/** Each shot's base damage, before the consecutive-hit ramp. */
	UPROPERTY()
	double Damage = 0.0;

	/** Seconds between shots. */
	UPROPERTY()
	double IntervalSeconds = 0.0;

	/** Attack range, edge to edge (Combat Bible §40), in units. */
	UPROPERTY()
	double Range = 0.0;

	/** The shot's homing projectile. */
	UPROPERTY()
	double ProjectileSpeed = 0.0;

	UPROPERTY()
	double ProjectileRadius = 0.0;

	/** How often a tower reconsiders its target, in seconds: how soon it answers aggression or a target leaving. */
	UPROPERTY()
	double ThinkSeconds = 0.0;
};

/** Consecutive tower shots on the same Vanguard escalate (Combat Bible §33). */
USTRUCT()
struct FVeyraTowerRampTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Damage added per consecutive shot, as a fraction of the base; 0.2 is +20%. */
	UPROPERTY()
	double PerShot = 0.0;

	/** The most ramp stacks a shot can carry. */
	UPROPERTY()
	int32 MaxStacks = 0;
};

/** The World domain's tuning, bound from Game/Tuning/World.json (ADR-006 §6, ADR-011 §12, §17). */
USTRUCT()
struct FVeyraWorldTuning
{
	GENERATED_BODY()

	/** The World.json format this build reads (a schema version marker, not tuning). */
	static constexpr int32 SchemaVersion = 1;

	UPROPERTY()
	FVeyraBattlegroundLayout Layout;

	UPROPERTY()
	FVeyraStructuresTuning Structures;

	UPROPERTY()
	FVeyraInhibitorTuning Inhibitor;

	UPROPERTY()
	FVeyraPrimeWellTuning PrimeWell;

	UPROPERTY()
	FVeyraTowerAttackTuning TowerAttack;

	UPROPERTY()
	FVeyraTowerRampTuning TowerRamp;
};

/** The World domain's checks that a schema cannot express. */
namespace VeyraWorld
{
	/**
	 * Problems with Tuning, each a JSON pointer and a message; empty when it is consistent: one lane
	 * of each kind, each mirroring onto itself, its structures on Team A's half, every point on the
	 * floor.
	 */
	VEYRAWORLD_API TArray<FString> Validate(const FVeyraWorldTuning& Tuning);
}
