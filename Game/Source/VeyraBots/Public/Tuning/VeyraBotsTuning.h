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
	/**
	 * Cast at a jungle creature or a Flux Well in reach that the cast's damage would finish, to take
	 * it before anyone else can: League's Smite (ADR-015 §8).
	 */
	Secure,
	/**
	 * Never cast: a bot leaves it alone, as a stance it cannot yet leave before it moves (Vera's Dig
	 * In; ADR-018 §8). The kit is still complete in the data.
	 */
	Never,
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

/** What a seat plays (ADR-013 §8.1; ADR-014 §7): a lane, or the jungle, as League's five roles. */
UENUM()
enum class EVeyraBotRole : uint8
{
	Top,
	Mid,
	Bottom,
	/** Clears its side's camps, takes Flux Wells and ganks (League's jungler). */
	Jungle,
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

	/** How near, edge to edge, an enemy Fluxborn must be for a bot to push with it, in units. */
	UPROPERTY()
	double PushRange = 0.0;

	/** A bot that retreated heals at its fountain to this fraction of Max Health before it goes back. */
	UPROPERTY()
	double LeaveFountainHealthFraction = 0.0;

	/** How near an open Flux Well must be for a laner in a quiet lane to take it, in units (ADR-014 §7). */
	UPROPERTY()
	double WellRange = 0.0;
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

	/**
	 * How many basic attacks' damage a Fluxborn may have left when the bot goes for the last hit: at
	 * least 1, and more to cover walking up and winding up.
	 */
	UPROPERTY()
	double LastHitLead = 0.0;

	/** The chance, at each decision with nothing to last-hit, that the bot shoves its wave. */
	UPROPERTY()
	double PushChance = 0.0;

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

	/** With this much Gold and its next purchase affordable, a bot in a quiet lane recalls to shop. */
	UPROPERTY()
	double ShopRecallGold = 0.0;

	/**
	 * The most Fluxborn of an enemy Vanguard's side that may stand within their aggression response
	 * range of it when the bot starts a fight with it: more, and the hit would turn them on the bot.
	 */
	UPROPERTY()
	int32 FluxbornTolerance = 0;
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

/** How a jungler plays (ADR-014 §7). */
USTRUCT()
struct FVeyraBotJungleTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** How near an enemy Vanguard must be for a jungler to gank it, in units. */
	UPROPERTY()
	double GankRange = 0.0;

	/** How hurt that enemy must be: below this fraction of its Health. */
	UPROPERTY()
	double GankHealthFraction = 0.0;

	/** How near an open Flux Well must be for a jungler to take it, in units. */
	UPROPERTY()
	double WellRange = 0.0;

	/**
	 * The share of its resource a jungler keeps for ganks and fights: it casts its basic abilities at
	 * its camp only while it holds more than this fraction of its most, as League's junglers clear.
	 */
	UPROPERTY()
	double AbilityResourceFloor = 0.0;
};

/** How bots ward (ADR-016 §7): League's jungler and support ward the bushes they pass, here Dense Fog. */
USTRUCT()
struct FVeyraBotWardingTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** The seats that ward, by their place in Seats, from 0. */
	UPROPERTY()
	TArray<int32> Seats;

	/** How near a Dense Fog patch's centre a bot passes to ward it, in units. */
	UPROPERTY()
	double SpotReach = 0.0;

	/** A patch an allied ward stands this near is warded already, in units. */
	UPROPERTY()
	double SpotSpacing = 0.0;
};

/** One bot seat: what it plays, and the starting Flux Spells it chooses, as League's bots do (ADR-015 §8). */
USTRUCT()
struct FVeyraBotSeatTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraBotRole Role = EVeyraBotRole::Mid;

	/** Roster spells, one per spell slot in slot order; at most two. */
	UPROPERTY()
	TArray<FVeyraContentId> FluxSpells;
};

/** The Bots domain's tuning, bound from Game/Tuning/Bots.json (ADR-006 §6, ADR-013 §5). */
USTRUCT()
struct FVeyraBotsTuning
{
	GENERATED_BODY()

	/** The Bots.json format this build reads (a schema version marker, not tuning). */
	static constexpr int32 SchemaVersion = 5;

	UPROPERTY()
	FVeyraBotSensesTuning Senses;

	UPROPERTY()
	FVeyraBotPositioningTuning Positioning;

	UPROPERTY()
	FVeyraBotDifficultiesTuning Difficulties;

	/** What each seat plays and the spells it takes, by the bot's place among its side's bots; later seats wrap around. */
	UPROPERTY()
	TArray<FVeyraBotSeatTuning> Seats;

	/** What each Flux Spell is for, which tells a bot when to cast it. */
	UPROPERTY()
	TMap<FVeyraContentId, EVeyraBotAbilityUse> FluxSpells;

	UPROPERTY()
	FVeyraBotJungleTuning Jungle;

	UPROPERTY()
	FVeyraBotWardingTuning Warding;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraBotVanguardTuning> Vanguards;
};

namespace VeyraBots
{
	/** The lane a role plays; a jungler keeps Mid's for the moments it holds a place. */
	inline EVeyraLane LaneOf(EVeyraBotRole Role)
	{
		switch (Role)
		{
		case EVeyraBotRole::Top:
			return EVeyraLane::Top;
		case EVeyraBotRole::Bottom:
			return EVeyraLane::Bottom;
		case EVeyraBotRole::Mid:
		case EVeyraBotRole::Jungle:
			break;
		}
		return EVeyraLane::Mid;
	}

	/**
	 * The checks the schema cannot express: each build is items the catalog sells, none part of
	 * another's recipe; each skill priority names Q, W and E once; and every released Vanguard has
	 * an entry whose abilities cover its kit.
	 */
	VEYRABOTS_API TArray<FString> Validate(const FVeyraBotsTuning& Tuning);
}
