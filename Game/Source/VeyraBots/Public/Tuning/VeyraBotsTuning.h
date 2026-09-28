// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Battleground/VeyraBattlegroundTypes.h"
#include "Content/VeyraContentId.h"
#include "Tuning/VeyraTuningProvenance.h"
#include "UObject/ObjectMacros.h"

#include "VeyraBotsTuning.generated.h"

/** A kit slot a bot ranks by its priority; it ranks R whenever it may (ADR-013 §5). */
UENUM()
enum class EVeyraBotSkill : uint8
{
	Q,
	W,
	E,
};

/** What an ability is for, which tells a bot when to cast it (ADR-013 §4). */
UENUM()
enum class EVeyraBotAbilityUse : uint8
{
	/** Cast at an enemy Vanguard in reach while fighting. */
	Damage,
	/** Opens a fight the bot favours. */
	Engage,
	/** Cast on itself while fighting. */
	Defend,
	/** Cast at the threat while retreating. */
	Escape,
	/** Cast before a basic attack on an enemy Vanguard. */
	Empower,
};

/** Where a bot aims a skillshot or area at a moving target. */
UENUM()
enum class EVeyraBotAim : uint8
{
	/** Where it is. */
	AtTarget,
	/** Where it will be when the shot arrives. */
	Lead,
};

/** What a bot notices. */
USTRUCT()
struct FVeyraBotSensesTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** How far, in units, a bot sees units around it. */
	UPROPERTY()
	double SightRadius = 0.0;

	/** A bot recalls only when no enemy Vanguard is this near, in units. */
	UPROPERTY()
	double SafeRadius = 0.0;

	/** How far outside an enemy tower's range, in units, a bot stays when it may not dive. */
	UPROPERTY()
	double TowerMargin = 0.0;
};

/** Where a bot stands in its lane. */
USTRUCT()
struct FVeyraBotPositioningTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** How far behind its wave's front, toward its own base, a bot holds, in units. */
	UPROPERTY()
	double FollowDistance = 0.0;

	/** How far from its spot a bot may stand before it walks back, in units. */
	UPROPERTY()
	double HoldTolerance = 0.0;
};

/** One behaviour (Custom Matches Bible §3): how a bot plays, never the rules. */
USTRUCT()
struct FVeyraBotDifficultyTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Seconds between decisions, on match time. */
	UPROPERTY()
	double ThinkSeconds = 0.0;

	/** How long an enemy Vanguard must be in sight before the bot fights it. */
	UPROPERTY()
	double ReactionSeconds = 0.0;

	/** The chance, at each decision, that the bot goes for a Fluxborn its basic attack would kill. */
	UPROPERTY()
	double LastHitChance = 0.0;

	/** The chance, at each decision, that the bot casts an ability that is ready and useful. */
	UPROPERTY()
	double CastChance = 0.0;

	UPROPERTY()
	EVeyraBotAim Aim = EVeyraBotAim::AtTarget;

	/** Below this fraction of Max Health the bot retreats, and recalls once it is safe. */
	UPROPERTY()
	double RetreatHealthFraction = 0.0;

	/** The bot fights an enemy Vanguard only while its Health fraction is at least the enemy's plus this. */
	UPROPERTY()
	double FightHealthMargin = 0.0;
};

USTRUCT()
struct FVeyraBotDifficultiesTuning
{
	GENERATED_BODY()

	UPROPERTY()
	FVeyraBotDifficultyTuning Beginner;

	UPROPERTY()
	FVeyraBotDifficultyTuning Intermediate;
};

/** How bots play one Vanguard. */
USTRUCT()
struct FVeyraBotVanguardTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** The items the bot works toward, in order; none is part of another's recipe. */
	UPROPERTY()
	TArray<FVeyraContentId> Build;

	/** The order in which it ranks Q, W and E. */
	UPROPERTY()
	TArray<EVeyraBotSkill> SkillPriority;

	/** What each ability of the kit is for. */
	UPROPERTY()
	TMap<FVeyraContentId, EVeyraBotAbilityUse> Abilities;
};

/** The Bots domain's tuning, bound from Game/Tuning/Bots.json (ADR-006 §6, ADR-013 §5). */
USTRUCT()
struct FVeyraBotsTuning
{
	GENERATED_BODY()

	/** The Bots.json format this build reads (a schema version marker, not tuning). */
	static constexpr int32 SchemaVersion = 1;

	UPROPERTY()
	FVeyraBotSensesTuning Senses;

	UPROPERTY()
	FVeyraBotPositioningTuning Positioning;

	UPROPERTY()
	FVeyraBotDifficultiesTuning Difficulties;

	/** The lane each seat plays, by the bot's place among its side's bots; later seats wrap around. */
	UPROPERTY()
	TArray<EVeyraLane> Lanes;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraBotVanguardTuning> Vanguards;
};

namespace VeyraBots
{
	/**
	 * The checks the schema cannot express: each build is items the catalog sells, none part of
	 * another's recipe; each skill priority names Q, W and E once; and every released Vanguard has
	 * an entry whose abilities cover its kit.
	 */
	VEYRABOTS_API TArray<FString> Validate(const FVeyraBotsTuning& Tuning);
}
