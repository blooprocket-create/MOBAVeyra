// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Attacks/VeyraBasicAttackTypes.h"
#include "Battleground/VeyraBattlegroundTypes.h"
#include "Content/VeyraContentId.h"
#include "Damage/VeyraDamageTypes.h"
#include "Stats/VeyraStatBlock.h"
#include "Tuning/VeyraTuningProvenance.h"
#include "Tuning/VeyraTerrainTuning.h"
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
 * Team A's base to Team B's; Team B's structures stand the same distances along it from Team B's end.
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

	/**
	 * Which side of the road each Spire stands, in SpireDistances' order (author ruling 2026-10-12): +1 the lane's left
	 * seen walking from Team A's end in world X/Y, -1 its right. Each stands the layout's SpireOffset from the centre line.
	 */
	UPROPERTY()
	TArray<int32> SpireSides;

	/** Where Team A's Fluxborn of this lane spawn: their distance along the lane from Team A's end, clear of the inhibitor. */
	UPROPERTY()
	double FluxbornSpawnDistance = 0.0;
};

/** Team A's base (Battleground Bible §3, §12, §18); Team B's is its rotation (VeyraLayout::Rotate). */
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

	/** How far from the fountain point a Vanguard counts as at its fountain (§12), in units. */
	UPROPERTY()
	double FountainRadius = 0.0;
};

/**
 * One Dense Fog circle of Team A's half (Battleground Bible §11, Vision Bible §2): the battleground's
 * bush. Team B's is its rotation; circles that touch are one fog volume.
 */
USTRUCT()
struct FVeyraFogLayout
{
	GENERATED_BODY()

	UPROPERTY()
	FVeyraMapPoint Center;

	/** In units; above 0. */
	UPROPERTY()
	double Radius = 0.0;
};

/** Validated vertical search bounds and walkable slope for terrain placement (ADR-040). */
USTRUCT()
struct FVeyraSurfaceTuning
{
	GENERATED_BODY()

	UPROPERTY()
	double MinZ = 0.0;

	UPROPERTY()
	double MaxZ = 0.0;

	UPROPERTY()
	double MaxSlopeDegrees = 0.0;
};

/** One run of Team A's keep wall (ADR-073 §1): architecture along the keep's edge, broken where a gate stands. */
USTRUCT()
struct FVeyraKeepWallLayout
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FVeyraMapPoint> Points;

	/** Its full thickness, in units. */
	UPROPERTY()
	double Thickness = 0.0;

	/** How tall it is drawn, in units. */
	UPROPERTY()
	double Height = 0.0;
};

/** Where a lane leaves Team A's keep through its wall (ADR-073 §1). */
USTRUCT()
struct FVeyraKeepGateLayout
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraLane Lane = EVeyraLane::Mid;

	UPROPERTY()
	FVeyraMapPoint Center;

	/** The opening's width, in units. */
	UPROPERTY()
	double Width = 0.0;
};

/** Team A's walled keep (ADR-073 §1): its floor, its wall runs and its gates. Team B's is its rotation. */
USTRUCT()
struct FVeyraKeepLayout
{
	GENERATED_BODY()

	/** The keep's floor, a simple polygon, its corners in order. */
	UPROPERTY()
	TArray<FVeyraMapPoint> Floor;

	UPROPERTY()
	TArray<FVeyraKeepWallLayout> Walls;

	UPROPERTY()
	TArray<FVeyraKeepGateLayout> Gates;
};

/** What a clearing is for (ADR-073 §1). */
UENUM()
enum class EVeyraClearingKind : uint8
{
	/** A wildlife camp's ground: the camp of the same id stands on it. */
	Camp,
	/** A junction, river mouth, pocket or ford. */
	Glade,
};

/** One of Team A's clearings (ADR-073 §1): open ground kept at its full size. Team B's is its rotation. */
USTRUCT()
struct FVeyraClearingLayout
{
	GENERATED_BODY()

	UPROPERTY()
	FVeyraContentId Id;

	UPROPERTY()
	FVeyraMapPoint Center;

	/** In units; above 0. */
	UPROPERTY()
	double Radius = 0.0;

	UPROPERTY()
	EVeyraClearingKind Kind = EVeyraClearingKind::Glade;
};

/** One of Team A's jungle routes (ADR-073 §1): open ground along a path as wide as its width. Team B's is its rotation. */
USTRUCT()
struct FVeyraRouteLayout
{
	GENERATED_BODY()

	/** What designers call it; play never reads it. */
	UPROPERTY()
	FString Id;

	/** Its full width, in units. */
	UPROPERTY()
	double Width = 0.0;

	UPROPERTY()
	TArray<FVeyraMapPoint> Points;
};

/** How the rock and forest masses are derived and turned into terrain (ADR-073 §2, §3, §5). */
USTRUCT()
struct FVeyraMassRules
{
	GENERATED_BODY()

	/** No mass is thinner than this, in units: the generator opens thinner leftovers. */
	UPROPERTY()
	double MinThickness = 0.0;

	/** Leftovers smaller than this, in square units, become open ground. */
	UPROPERTY()
	double MinArea = 0.0;

	/** How far a mass's outline may move when simplified, in units. */
	UPROPERTY()
	double SimplifyTolerance = 0.0;

	/** Walkable bank beside the river's water, each side, in units. */
	UPROPERTY()
	double RiverMargin = 0.0;

	/** Inner masses at least this large, in square units, are forest; smaller ones rock. */
	UPROPERTY()
	double ForestMinArea = 0.0;

	/** How deep each terrain box along a mass's edge reaches inside it, in units. */
	UPROPERTY()
	double EdgeThickness = 0.0;

	/** The longest one edge box may be, in units: longer edges are split. */
	UPROPERTY()
	double EdgeMaxLength = 0.0;

	/**
	 * The radius the generator rounds each convex corner of a mass to, cutting back spikes narrower than twice it, in
	 * units: an edge box lies flush along its span, so at a corner sharper than a right angle it would stand out of the mass.
	 */
	UPROPERTY()
	double CornerRadius = 0.0;

	/** How far a mass mesh's footprint may lie from its gameplay edge, in units. Presentation's bound. */
	UPROPERTY()
	double VisualTolerance = 0.0;
};

/** What a mass is made of (ADR-073 §2, §5): its look, never its gameplay. */
UENUM()
enum class EVeyraMassKind : uint8
{
	Rock,
	Forest,
	/** A mass against the floor's rim. */
	Cliff,
};

/** One rock or forest mass, as generated (ADR-073 §2): blocked ground everything else leaves. */
USTRUCT()
struct FVeyraMassLayout
{
	GENERATED_BODY()

	/** a00, a01, ... for Team A's; b00, b01, ... their half turns. */
	UPROPERTY()
	FString Id;

	UPROPERTY()
	EVeyraMassKind Kind = EVeyraMassKind::Rock;

	/** Its outline, counter-clockwise in world X/Y. */
	UPROPERTY()
	TArray<FVeyraMapPoint> Points;
};

/** GENERATED by Game/Scripts/Crucible/BuildCrucibleMasses.py, never by hand (ADR-073 §2): both teams' masses. */
USTRUCT()
struct FVeyraMassesLayout
{
	GENERATED_BODY()

	/** The hash of the inputs the masses were derived from; CI refuses a stale set. */
	UPROPERTY()
	FString InputSha256;

	UPROPERTY()
	TArray<FVeyraMassLayout> Items;
};

/**
 * The battleground's authoritative spatial layout (ADR-011 §12): the one source for the generated map and the
 * server's spawning. Team B's half is Team A's rotated half a turn about the centre (author ruling 2026-10-05), which
 * swaps the bases and maps the lanes onto each other, top onto bottom and mid onto itself, so both teams have the same
 * battleground from their own side. The river through the centre is its own rotation.
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

	UPROPERTY()
	FVeyraSurfaceTuning Surface;

	UPROPERTY()
	FVeyraTerrainTuning Terrain;

	UPROPERTY()
	FVeyraRiverLayout River;

	UPROPERTY()
	TArray<FVeyraLaneLayout> Lanes;

	UPROPERTY()
	FVeyraBaseLayout Base;

	/** Team A's Dense Fog (ADR-016 §4, §11); Team B's is its rotation. */
	UPROPERTY()
	TArray<FVeyraFogLayout> DenseFog;

	/** How far each Spire stands from its lane's centre line, on its side (author ruling 2026-10-12), in units. */
	UPROPERTY()
	double SpireOffset = 0.0;

	UPROPERTY()
	FVeyraKeepLayout Keep;

	/** Team A's clearings (ADR-073 §1); Team B's are their rotation. */
	UPROPERTY()
	TArray<FVeyraClearingLayout> Clearings;

	/** Team A's jungle routes (ADR-073 §1); Team B's are their rotation. */
	UPROPERTY()
	TArray<FVeyraRouteLayout> Routes;

	/** Half the height of the terrain raised along masses and keep walls, in units: taller than any body it blocks. */
	UPROPERTY()
	double WallHalfHeight = 0.0;

	UPROPERTY()
	FVeyraMassRules MassRules;

	UPROPERTY()
	FVeyraMassesLayout Masses;
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

/**
 * Backdoor protection (Battleground Bible §19; Combat Bible §33): a lane Spire, base-defense tower or
 * Prime Well with no attacking Fluxborn near takes less damage, the reduction climbing toward its
 * maximum; an attacking Fluxborn arriving removes it at once. It never lifts invulnerability.
 */
USTRUCT()
struct FVeyraBackdoorTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** How near the structure's centre an attacking Fluxborn must be to lift the protection, in units. */
	UPROPERTY()
	double Radius = 0.0;

	/** The most damage it removes, 0 to below 1: 0.66 takes two thirds off. */
	UPROPERTY()
	double MaxReduction = 0.0;

	/** Seconds from none to the maximum, once no attacking Fluxborn is near. */
	UPROPERTY()
	double RampSeconds = 0.0;

	/** How often it is brought up to date, in seconds of world time. */
	UPROPERTY()
	double UpdateSeconds = 0.0;
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

/** What part a kind of Fluxborn plays in its wave (Battleground Bible §4, §19). */
UENUM()
enum class EVeyraFluxbornRole : uint8
{
	/** Front-line melee bodies (Striders). */
	Frontline,
	/** Ranged units (Sparks). */
	Ranged,
	/** Periodic siege units, which put structures in range first (Breakers, §19). */
	Siege,
};

/** One kind of Fluxborn: its stats, body and basic attack (Battleground Bible §4; ADR-011 §7). */
USTRUCT()
struct FVeyraFluxbornDefinition
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraFluxbornRole Role = EVeyraFluxbornRole::Frontline;

	/** Its base stats, before Team Flux strengthens it. No resource. */
	UPROPERTY()
	FVeyraStatBlock Stats;

	/** The same attack component and profile as Vanguards use; its acquisition radius is how far it looks for enemies. */
	UPROPERTY()
	FVeyraBasicAttackProfile BasicAttack;

	/** The body's capsule, in units. */
	UPROPERTY()
	double CapsuleRadius = 0.0;

	UPROPERTY()
	double CapsuleHalfHeight = 0.0;
};

/** How every Fluxborn's server controller behaves (ADR-011 §7). */
USTRUCT()
struct FVeyraFluxbornAiTuning
{
	GENERATED_BODY()

	/** How often it reconsiders its target and its path, in seconds of world time. */
	UPROPERTY()
	double ThinkSeconds = 0.0;

	/** How far past its edge an enemy Vanguard that hurts a nearby ally draws it (Battleground Bible §19), in units. */
	UPROPERTY()
	double AggressionResponseRange = 0.0;

	/**
	 * How far from its lane's path it engages a target, and strays itself, in units: a target farther out is let go, and
	 * so is any target once the Fluxborn itself is farther out; then it returns (ADR-065 §2).
	 */
	UPROPERTY()
	double LeashRange = 0.0;

	/** How close to a waypoint counts as reaching it, in units. */
	UPROPERTY()
	double WaypointAcceptance = 0.0;
};

/** The lane Fluxborn (Battleground Bible §4, §17, §19; ADR-011 §7). */
USTRUCT()
struct FVeyraFluxbornTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Every kind, by its stable ID, such as strider. */
	UPROPERTY()
	TMap<FVeyraContentId, FVeyraFluxbornDefinition> Units;

	UPROPERTY()
	FVeyraFluxbornAiTuning Ai;

	/** How long a fallen Fluxborn's body stays before it is removed, in seconds. */
	UPROPERTY()
	double CorpseSeconds = 0.0;

	/** Local avoidance among Fluxborn (Combat Bible §24): how far each looks, in units, and how much it gives way, 0 to 1. */
	UPROPERTY()
	double AvoidanceConsiderationRadius = 0.0;

	UPROPERTY()
	double AvoidanceWeight = 0.0;
};

/** One span of the match with its own wave cadence (Battleground Bible §17). */
USTRUCT()
struct FVeyraWavePhaseTuning
{
	GENERATED_BODY()

	/** When the phase begins, in match-clock seconds; the first begins at 0. */
	UPROPERTY()
	double FromSeconds = 0.0;

	/** Seconds from each wave spawned in this phase to the next. */
	UPROPERTY()
	double IntervalSeconds = 0.0;

	/** Every this many waves brings siege units; 0 for none. */
	UPROPERTY()
	int32 SiegeEveryWaves = 0;
};

/** Some Fluxborn of one kind in a wave. */
USTRUCT()
struct FVeyraWaveUnitTuning
{
	GENERATED_BODY()

	UPROPERTY()
	FVeyraContentId Unit;

	UPROPERTY()
	int32 Count = 0;
};

/**
 * The Fluxborn waves (Battleground Bible §17, §18): all three lanes spawn together, on a schedule
 * whose phases quicken it; ordinary waves hold front-line and ranged units, some bring siege units,
 * and a lane whose enemy inhibitor is down adds more. No wave grows stronger over time.
 */
USTRUCT()
struct FVeyraWavesTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** When the first wave spawns, in match-clock seconds. */
	UPROPERTY()
	double FirstWaveSeconds = 0.0;

	/** In order of FromSeconds, the first from 0. */
	UPROPERTY()
	TArray<FVeyraWavePhaseTuning> Phases;

	/** Every wave's units, in the order they leave the base: front line first. */
	UPROPERTY()
	TArray<FVeyraWaveUnitTuning> Units;

	/** Added to the waves that bring siege units, after the front line. */
	UPROPERTY()
	TArray<FVeyraWaveUnitTuning> SiegeUnits;

	/** Added to each wave of a lane whose enemy inhibitor is down (§18). */
	UPROPERTY()
	TArray<FVeyraWaveUnitTuning> InhibitorDownUnits;

	/** Seconds between one unit of a wave leaving the base and the next, so they walk out in a file. */
	UPROPERTY()
	double UnitIntervalSeconds = 0.0;
};

/** One species of jungle wildlife (Battleground Bible §8; ADR-014 §2). */
USTRUCT()
struct FVeyraWildlifeSpecies
{
	GENERATED_BODY()

	/** Its stats. No resource; nothing scales them over the match (§17: camps never evolve). */
	UPROPERTY()
	FVeyraStatBlock Stats;

	/** The same attack component and profile as Vanguards use. */
	UPROPERTY()
	FVeyraBasicAttackProfile BasicAttack;

	/** The body's capsule, in units. */
	UPROPERTY()
	double CapsuleRadius = 0.0;

	UPROPERTY()
	double CapsuleHalfHeight = 0.0;

	/**
	 * The trait its camp's clear grants the Vanguard credited with the last kill (§8): at most one
	 * status from Abilities.json, or none.
	 */
	UPROPERTY()
	TArray<FVeyraContentId> Traits;
};

/** How every creature's server controller behaves (ADR-014 §2). */
USTRUCT()
struct FVeyraWildlifeAiTuning
{
	GENERATED_BODY()

	/** How often it reconsiders its target, in seconds of world time. */
	UPROPERTY()
	double ThinkSeconds = 0.0;

	/** How long a fallen creature's body stays before it is removed, in seconds. */
	UPROPERTY()
	double CorpseSeconds = 0.0;

	/** How close to its spot a creature walking home must come to be home, in units. */
	UPROPERTY()
	double HomeAcceptance = 0.0;
};

/** One of Team A's camps; Team B's is its rotation (Battleground Bible §7, §8, §17). */
USTRUCT()
struct FVeyraCampTuning
{
	GENERATED_BODY()

	/** The camp's own name (ADR-073 §1): its ground is the layout's clearing of the same id. Camps may share a species. */
	UPROPERTY()
	FVeyraContentId Id;

	UPROPERTY()
	FVeyraContentId Species;

	/** How many creatures of its species it holds. */
	UPROPERTY()
	int32 Count = 0;

	/** Where it sits: its leash's centre. */
	UPROPERTY()
	FVeyraMapPoint Center;

	/** When it first spawns, in match-clock seconds (§17: two spawn groups). */
	UPROPERTY()
	double SpawnSeconds = 0.0;

	/** How long after its last creature dies it spawns again, in seconds (§17: each camp on its own timer). */
	UPROPERTY()
	double RespawnSeconds = 0.0;

	/** How far from its centre its creatures fight before they give up, walk home and heal (§17), in units. */
	UPROPERTY()
	double LeashRadius = 0.0;

	/** With several creatures, how far from the centre each stands, in units. */
	UPROPERTY()
	double Spacing = 0.0;
};

/** The jungle's wildlife (Battleground Bible §7, §8, §17; ADR-014 §2). */
USTRUCT()
struct FVeyraWildlifeTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Every species, by its stable ID, such as ashfang. */
	UPROPERTY()
	TMap<FVeyraContentId, FVeyraWildlifeSpecies> Species;

	UPROPERTY()
	FVeyraWildlifeAiTuning Ai;

	/** Team A's camps. */
	UPROPERTY()
	TArray<FVeyraCampTuning> Camps;
};

/** A Flux Well's opening and cycle (Battleground Bible §6). */
USTRUCT()
struct FVeyraFluxWellTimingTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** When the Wells first open, in match-clock seconds. */
	UPROPERTY()
	double OpenSeconds = 0.0;

	/** How long after its secure a Well opens again, in seconds. */
	UPROPERTY()
	double RespawnSeconds = 0.0;
};

/** How allied Vanguards standing at an open Well drain it (Battleground Bible §6; ADR-014 §4). */
USTRUCT()
struct FVeyraFluxWellPresenceTuning
{
	GENERATED_BODY()

	/** How often presence drains it, in seconds of world time. */
	UPROPERTY()
	double TickSeconds = 0.0;

	/** Health drained per second by one Vanguard alone. */
	UPROPERTY()
	double DrainPerSecond = 0.0;

	/** Added per second by each further Vanguard counted. */
	UPROPERTY()
	double DrainPerAdditional = 0.0;

	/** The most Vanguards of one side that count (§6: a dogpile is not required). */
	UPROPERTY()
	int32 MaxCounted = 0;

	/** Contested, the side with more drains at this fraction of the rate for its lead; equal sides stall it. */
	UPROPERTY()
	double ContestedFactor = 0.0;
};

/** An open Well nobody works on heals (ADR-014 §9). */
USTRUCT()
struct FVeyraFluxWellRegenerationTuning
{
	GENERATED_BODY()

	/** How long without damage or presence before it heals, in seconds. */
	UPROPERTY()
	double IdleSeconds = 0.0;

	/** Health restored per second, as a fraction of its Max Health. */
	UPROPERTY()
	double FractionPerSecond = 0.0;
};

/** The North and South Flux Wells (Battleground Bible §6; ADR-014 §4). */
USTRUCT()
struct FVeyraFluxWellsTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Where each Well stands: on a river island (author ruling 2026-10-05); each site's rotation is another site. */
	UPROPERTY()
	TArray<FVeyraMapPoint> Sites;

	UPROPERTY()
	FVeyraFluxWellTimingTuning Timing;

	UPROPERTY()
	double MaxHealth = 0.0;

	UPROPERTY()
	double Armor = 0.0;

	UPROPERTY()
	double MagicResist = 0.0;

	/** The body's capsule, in units. */
	UPROPERTY()
	double CapsuleRadius = 0.0;

	UPROPERTY()
	double CapsuleHalfHeight = 0.0;

	/** How near its centre a Vanguard must stand to count as present, in units. */
	UPROPERTY()
	double Radius = 0.0;

	UPROPERTY()
	FVeyraFluxWellPresenceTuning Presence;

	UPROPERTY()
	FVeyraFluxWellRegenerationTuning Regeneration;
};

/** How often the battleground's units replicate (ADR-011 §7; amends ADR-006 §5). */
USTRUCT()
struct FVeyraWorldReplicationTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** A Fluxborn sends its state every this many server ticks: 3 is 10 Hz at a 30 Hz tick. */
	UPROPERTY()
	int32 FluxbornUpdateEveryServerTicks = 0;
};

/** The World domain's tuning, bound from Game/Tuning/World.json (ADR-006 §6, ADR-011 §12, §17). */
USTRUCT()
struct FVeyraWorldTuning
{
	GENERATED_BODY()

	/** The World.json format this build reads (a schema version marker, not tuning). */
	static constexpr int32 SchemaVersion = 8;

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

	UPROPERTY()
	FVeyraBackdoorTuning Backdoor;

	UPROPERTY()
	FVeyraFluxbornTuning Fluxborn;

	UPROPERTY()
	FVeyraWavesTuning Waves;

	UPROPERTY()
	FVeyraWorldReplicationTuning Replication;

	UPROPERTY()
	FVeyraWildlifeTuning Wildlife;

	UPROPERTY()
	FVeyraFluxWellsTuning FluxWells;

	/** The kind of Fluxborn with Id, or null. */
	const FVeyraFluxbornDefinition* FindFluxborn(const FVeyraContentId& Id) const { return Fluxborn.Units.Find(Id); }

	/** The species of wildlife with Id, or null. */
	const FVeyraWildlifeSpecies* FindSpecies(const FVeyraContentId& Id) const { return Wildlife.Species.Find(Id); }
};

/** The World domain's checks that a schema cannot express. */
namespace VeyraWorld
{
	/**
	 * Problems with Tuning, each a JSON pointer and a message; empty when it is consistent: one lane
	 * of each kind, the lanes rotating onto each other, each lane's structures on its owner's half, every point on the
	 * floor; a river whose main channel starts at the centre and leaves the floor, each island's channel joining it at both
	 * ends; each camp of a known species, on Team A's half with its leash on the floor and clear of the water and every
	 * lane; each Flux Well dry on its island and clear of every lane, the sites rotating onto each other.
	 */
	VEYRAWORLD_API TArray<FString> Validate(const FVeyraWorldTuning& Tuning);
}
