// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Tuning/VeyraTuningProvenance.h"
#include "UObject/ObjectMacros.h"

#include "VeyraMatchTuning.generated.h"

// The Match domain's tuning, bound from Game/Tuning/Match.json (ADR-006 §6). The schema holds every
// range; a 0 here only means "not loaded".

USTRUCT()
struct FVeyraTeamsTuning
{
	GENERATED_BODY()

	UPROPERTY()
	int32 MaxTeamSize = 0;
};

/** The in-match stages before play (Match Flow Bible §1). */
USTRUCT()
struct FVeyraPhasesTuning
{
	GENERATED_BODY()

	UPROPERTY()
	double LoadingTimeoutSeconds = 0.0;

	UPROPERTY()
	double PreparationSeconds = 0.0;
};

/** How the respawn timer lengthens as the match goes on (Economy & Progression Bible §14). */
USTRUCT()
struct FVeyraRespawnElapsedTuning
{
	GENERATED_BODY()

	/** The match clock, in seconds, from which the timer starts to lengthen. */
	UPROPERTY()
	double StartSeconds = 0.0;

	/** What each minute past StartSeconds adds, as a fraction of the level's timer. */
	UPROPERTY()
	double FractionPerMinute = 0.0;

	/** The most the elapsed time adds, as a fraction. */
	UPROPERTY()
	double MaxFraction = 0.0;
};

/**
 * When a dead Vanguard returns at its fountain (Combat Bible §18): a data curve over the Vanguard's
 * level and the elapsed match time (Economy & Progression Bible §14; ADR-011 §11).
 */
USTRUCT()
struct FVeyraRespawnTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** The timer at each level from Level 1; a level past the last entry uses the last. 0 respawns at once. */
	UPROPERTY()
	TArray<double> SecondsByLevel;

	UPROPERTY()
	FVeyraRespawnElapsedTuning Elapsed;
};

/**
 * How quickly a living Vanguard recovers at its own fountain (Battleground Bible §12; ADR-011 §11,
 * provisional answer 8).
 */
USTRUCT()
struct FVeyraFountainTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** How near its side's start, in units, a Vanguard must stand. */
	UPROPERTY()
	double Radius = 0.0;

	/** Max Health restored each second, as a fraction. */
	UPROPERTY()
	double HealthFractionPerSecond = 0.0;

	/** Max resource restored each second, as a fraction. */
	UPROPERTY()
	double ResourceFractionPerSecond = 0.0;

	/** Seconds between restorations. */
	UPROPERTY()
	double IntervalSeconds = 0.0;
};

/**
 * Recall (Economy & Progression Bible §10; ADR-012 §8): a channel that brings a living Vanguard home
 * to its fountain.
 */
USTRUCT()
struct FVeyraRecallTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Seconds the channel lasts, on match time. */
	UPROPERTY()
	double ChannelSeconds = 0.0;
};

/** How a hosted match's server ends a match on its own (ADR-007 §8). */
USTRUCT()
struct FVeyraMatchLifecycleTuning
{
	GENERATED_BODY()

	/** Real seconds with no rostered participant connected before the match ends as abandoned. */
	UPROPERTY()
	double AbandonAfterSeconds = 0.0;
};

/** How the server accepts a player's move orders (ADR-006 §7). */
USTRUCT()
struct FVeyraOrdersTuning
{
	GENERATED_BODY()

	UPROPERTY()
	double MaxPerSecond = 0.0;

	UPROPERTY()
	double DestinationProjectionExtent = 0.0;

	UPROPERTY()
	double ArrivalTolerance = 0.0;
};

/**
 * How the match's AI participants behave (Custom Matches Bible §1; ADR-010 §7). For now a bot is a
 * practice target: it wanders near the middle of the map and does not fight back.
 */
USTRUCT()
struct FVeyraMatchBotsTuning
{
	GENERATED_BODY()

	/** Seconds between a bot's choices of where to walk. */
	UPROPERTY()
	double WanderIntervalSeconds = 0.0;

	/** How far, in units, from the point midway between the two sides' starts a bot may walk. */
	UPROPERTY()
	double WanderRadius = 0.0;
};

/** The slot a developer match ranks up for each participant at level 1. */
UENUM()
enum class EVeyraDeveloperStartingRank : uint8
{
	/** The player chooses. */
	None,
	Q,
	W,
	E,
};

/** Developer matches until champion select (ADR-008 §8). */
USTRUCT()
struct FVeyraDeveloperMatchTuning
{
	GENERATED_BODY()

	/** The Vanguard each participant plays, in join order; the last one plays for every later joiner. */
	UPROPERTY()
	TArray<FVeyraContentId> Vanguards;

	UPROPERTY()
	EVeyraDeveloperStartingRank StartingRank = EVeyraDeveloperStartingRank::None;
};

USTRUCT()
struct FVeyraMatchTuning
{
	GENERATED_BODY()

	/** The Match.json format this build reads (a schema version marker, not tuning). */
	static constexpr int32 SchemaVersion = 5;

	UPROPERTY()
	FVeyraTeamsTuning Teams;

	UPROPERTY()
	FVeyraPhasesTuning Phases;

	UPROPERTY()
	FVeyraRespawnTuning Respawn;

	UPROPERTY()
	FVeyraFountainTuning Fountain;

	UPROPERTY()
	FVeyraRecallTuning Recall;

	UPROPERTY()
	FVeyraMatchLifecycleTuning Lifecycle;

	UPROPERTY()
	FVeyraOrdersTuning Orders;

	UPROPERTY()
	FVeyraMatchBotsTuning Bots;

	UPROPERTY()
	FVeyraDeveloperMatchTuning DeveloperMatch;
};
