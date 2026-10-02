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

/**
 * Absence from a live match (Match Flow Bible §4–§6; ADR-019 §3). Every clock runs on the match clock,
 * so a pause stops it.
 */
USTRUCT()
struct FVeyraAbsenceTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Seconds without meaningful activity before a connected player counts as AFK. */
	UPROPERTY()
	double AfkAfterSeconds = 0.0;

	/** Seconds after the AFK warning, still inactive, before a personal loss. */
	UPROPERTY()
	double AfkPenaltyAfterSeconds = 0.0;

	/** Seconds of continuous disconnection before a personal loss. */
	UPROPERTY()
	double DisconnectPenaltyAfterSeconds = 0.0;

	/** The largest share of the active duration a player may have been absent for a win to forgive it. */
	UPROPERTY()
	double MaxForgivenAbsentFraction = 0.0;
};

/** What counts as meaningful activity (Match Flow Bible §5.1; ADR-019 §3). */
USTRUCT()
struct FVeyraActivityTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** How far a move's destination must be from the last one counted for the move to count. */
	UPROPERTY()
	double MinimumMoveDistance = 0.0;
};

/** Where an absent Vanguard is walked (Match Flow Bible §4; ADR-019 §2). */
USTRUCT()
struct FVeyraAutopilotTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Match seconds after it starts before it turns for the fountain. */
	UPROPERTY()
	double FountainAfterSeconds = 0.0;

	/** How far behind the tower, along its lane toward home, it stops. */
	UPROPERTY()
	double BehindTowerDistance = 0.0;
};

/** A team vote: remake before a time, surrender after one (Match Flow Bible §7–§8). */
USTRUCT()
struct FVeyraRemakeVoteTuning
{
	GENERATED_BODY()

	/** Match seconds after which none may start. */
	UPROPERTY()
	double StartBeforeSeconds = 0.0;

	UPROPERTY()
	int32 YesVotes = 0;

	UPROPERTY()
	double WindowSeconds = 0.0;

	UPROPERTY()
	double CooldownSeconds = 0.0;
};

USTRUCT()
struct FVeyraSurrenderVoteTuning
{
	GENERATED_BODY()

	/** Match seconds before which none may start. */
	UPROPERTY()
	double StartAfterSeconds = 0.0;

	UPROPERTY()
	int32 YesVotes = 0;

	UPROPERTY()
	double WindowSeconds = 0.0;

	UPROPERTY()
	double CooldownSeconds = 0.0;
};

/** A unanimous pause vote and the intermission it opens (Match Flow Bible §10). */
USTRUCT()
struct FVeyraPauseVoteTuning
{
	GENERATED_BODY()

	UPROPERTY()
	double WindowSeconds = 0.0;

	UPROPERTY()
	double CooldownSeconds = 0.0;

	/** Real seconds a passed pause lasts before play resumes by itself. */
	UPROPERTY()
	double IntermissionSeconds = 0.0;
};

/** Votes (ADR-019 §4): windows, cooldowns and the intermission are real seconds. */
USTRUCT()
struct FVeyraVotesTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	FVeyraRemakeVoteTuning Remake;

	UPROPERTY()
	FVeyraSurrenderVoteTuning Surrender;

	UPROPERTY()
	FVeyraPauseVoteTuning Pause;
};

/**
 * Team pings (ADR-020 §2): how many one player may send, and how long a client keeps one. Real seconds,
 * so a pause holds neither.
 */
USTRUCT()
struct FVeyraPingsTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	int32 MaxPerWindow = 0;

	UPROPERTY()
	double WindowSeconds = 0.0;

	/** The most a client keeps a ping: the top of the player's ping-persistence setting (Settings Bible §3.2). */
	UPROPERTY()
	double KeepSeconds = 0.0;
};

/**
 * The mastery emote (ADR-045 §9; Account, Collection & Mastery Bible §5.2): how long it shows above its Vanguard,
 * and how often a player may show it. It has no gameplay effect. World seconds.
 */
USTRUCT()
struct FVeyraMasteryEmoteTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	double Seconds = 0.0;

	UPROPERTY()
	double CooldownSeconds = 0.0;
};

/** In-match chat's limits (Chat & Communication Bible §2; ADR-029 §2). */
USTRUCT()
struct FVeyraChatTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** The longest message, in characters, once cleaned; at least 1. */
	UPROPERTY()
	int32 MaxCharacters = 0;

	/** At most this many messages in any window, per player; at least 1. */
	UPROPERTY()
	int32 MaxPerWindow = 0;

	/** Real seconds; above 0. */
	UPROPERTY()
	double WindowSeconds = 0.0;

	/** How many messages a client keeps to show; at least 1. */
	UPROPERTY()
	int32 KeepMessages = 0;
};

/**
 * How a match ends on screen (ADR-020 §1): how long an ended match stays up, its players watching the end
 * (the camera on the fallen Prime Well), before they leave for the results. Real seconds.
 */
USTRUCT()
struct FVeyraEndingTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	double ShowSeconds = 0.0;
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
	static constexpr int32 SchemaVersion = 10;

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
	FVeyraAbsenceTuning Absence;

	UPROPERTY()
	FVeyraActivityTuning Activity;

	UPROPERTY()
	FVeyraAutopilotTuning Autopilot;

	UPROPERTY()
	FVeyraVotesTuning Votes;

	UPROPERTY()
	FVeyraPingsTuning Pings;

	UPROPERTY()
	FVeyraMasteryEmoteTuning MasteryEmote;

	UPROPERTY()
	FVeyraChatTuning Chat;

	UPROPERTY()
	FVeyraEndingTuning Ending;

	UPROPERTY()
	FVeyraOrdersTuning Orders;

	UPROPERTY()
	FVeyraDeveloperMatchTuning DeveloperMatch;
};
