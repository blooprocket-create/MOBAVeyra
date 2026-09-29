// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Math/Vector.h"
#include "Misc/Optional.h"
#include "Slots/VeyraAbilitySlot.h"
#include "Tuning/VeyraBotsTuning.h"
#include "UObject/WeakObjectPtr.h"
#include "Units/VeyraUnit.h"
#include "VeyraAbilityTypes.h"

class AActor;

/** How an ability is aimed, from the archetype that defines it (ADR-013 §4). */
enum class EVeyraBotTargeting : uint8
{
	/** At an enemy unit. */
	Unit,
	/** At a ground point: an area, a skillshot or a dash. */
	Point,
	/** At nothing: a self-buff or an empowered attack. */
	Self,
};

/** What a bot needs to know to use one ability, read from its tuning. */
struct FVeyraBotAbilityProfile
{
	EVeyraBotTargeting Targeting = EVeyraBotTargeting::Self;

	/** How far from the caster an enemy may be for the cast to reach it; 0 for Self. */
	double Reach = 0.0;

	/** Seconds from the cast until it lands, before any travel: windups and delays. */
	double LeadSeconds = 0.0;

	/** A projectile's speed, for leading a moving target; 0 when nothing travels. */
	double ProjectileSpeed = 0.0;

	/** A dash that carries the caster away from its point, not toward it. */
	bool bAwayFromPoint = false;

	/** A targeted ability's damage at its caster's first Level, all of one type; 0 for anything else. */
	double Damage = 0.0;

	/** Whether that damage is True, so no resistance lessens it. */
	bool bTrueDamage = false;

	/** The kinds of unit a targeted ability may target; empty for any hostile unit. */
	TArray<EVeyraUnitKind> TargetKinds;

	/** Its resource cost, one value for every rank or one per rank. */
	TArray<double> CostByRank;
};

/** A unit a bot sees. */
struct FVeyraBotUnit
{
	TWeakObjectPtr<AActor> Actor;
	FVector Location = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	double Health = 0.0;
	double MaxHealth = 0.0;

	/** Its collision radius, for edge-to-edge distances. */
	double Radius = 0.0;

	/** The fraction of the bot's basic attack damage it takes, after its resistance. */
	double DamageTaken = 1.0;

	/**
	 * For an enemy Vanguard: how many Fluxborn of its side stand within their aggression response
	 * range of it, and would turn on a bot that hit it (Battleground Bible §19).
	 */
	int32 Defenders = 0;

	double HealthFraction() const { return MaxHealth > 0.0 ? Health / MaxHealth : 0.0; }
};

/** One ability slot as a bot sees it. */
struct FVeyraBotSlot
{
	EVeyraAbilitySlot Slot = EVeyraAbilitySlot::Q;
	FVeyraContentId Ability;
	EVeyraBotAbilityUse Use = EVeyraBotAbilityUse::Damage;
	FVeyraBotAbilityProfile Profile;

	/** Learned, off cooldown, affordable, and nothing else holds the caster. */
	bool bReady = false;
};

/** An enemy structure in the bot's lane, and how it threatens the bot. */
struct FVeyraBotStructure
{
	FVeyraBotUnit Unit;

	/** How far it shoots, edge to edge; 0 for a structure that does not shoot. */
	double AttackRange = 0.0;

	/** Whether it can be damaged now. */
	bool bVulnerable = false;

	/** Whether it is shooting the bot. */
	bool bTargetsBot = false;

	/** Whether allied Fluxborn stand in its range, where it shoots them first. */
	bool bAlliesInRange = false;
};

/** A jungle camp on the bot's side, as it knows it (ADR-014 §7). */
struct FVeyraBotCamp
{
	FVector Center = FVector::ZeroVector;

	/** Its living creatures; none while it waits to respawn. */
	TArray<FVeyraBotUnit> Creatures;

	/** When it spawns next, in match time; 0 while its creatures stand. */
	double SpawnsAt = 0.0;
};

/**
 * What a bot knows at one decision (ADR-013 §4): read from the world by VeyraBotSenses, and plain
 * data, so the rules that decide from it are pure and tested without a world.
 */
struct FVeyraBotView
{
	/** Match time, in seconds. */
	double Now = 0.0;

	bool bAlive = false;
	bool bRecalling = false;

	/** At its own fountain, where the shop delivers. */
	bool bAtFountain = false;

	/** Its Gold, and whether its build has a purchase that Gold affords now. */
	double Gold = 0.0;
	bool bPurchaseWaiting = false;

	FVeyraBotUnit Self;

	/** Its basic attack's reach, edge to edge, and what one does to a Fluxborn, before mitigation. */
	double AttackRange = 0.0;
	double AttackDamage = 0.0;

	/** Its fountain, where it retreats to. */
	FVector Home = FVector::ZeroVector;

	/** Where it holds in its lane: behind its wave, or at its outermost structure. */
	FVector LaneHold = FVector::ZeroVector;

	TArray<FVeyraBotUnit> EnemyVanguards;
	TArray<FVeyraBotUnit> AllyVanguards;
	TArray<FVeyraBotUnit> EnemyFluxborn;


	/** The next standing enemy structure along its lane, if it sees one. */
	TOptional<FVeyraBotStructure> EnemyStructure;

	TArray<FVeyraBotSlot> Slots;

	/** Whether it plays the jungle (ADR-014 §7). */
	bool bJungle = false;

	/** Its side's camps. */
	TArray<FVeyraBotCamp> Camps;

	/** The open Flux Wells. */
	TArray<FVeyraBotUnit> Wells;

	/** For a jungler: the enemy Vanguards within its gank range. */
	TArray<FVeyraBotUnit> GankTargets;
};

/** What a bot does next. */
enum class EVeyraBotAction : uint8
{
	/** Nothing new: it is dead, recalling, healing at the fountain, or already where it should be. */
	Wait,
	/** Walk to Destination. */
	Move,
	/** Walk home and away from danger. */
	Retreat,
	/** Channel home. */
	Recall,
	/** Attack Target: an enemy Vanguard, a Fluxborn or a structure. */
	Attack,
	/** Cast Slot at CastTarget. */
	Cast,
};

struct FVeyraBotIntent
{
	EVeyraBotAction Action = EVeyraBotAction::Wait;
	FVector Destination = FVector::ZeroVector;
	TWeakObjectPtr<AActor> Target;
	EVeyraAbilitySlot Slot = EVeyraAbilitySlot::Q;
	FVeyraCastTarget CastTarget;

	/** Why, for the log. */
	const TCHAR* Reason = TEXT("");
};

/** What a bot remembers between decisions. */
struct FVeyraBotMemory
{
	/** When each enemy Vanguard came into sight, for the reaction time. */
	TMap<TWeakObjectPtr<AActor>, double> FirstSeen;

	/** Set when Health falls below the retreat line; cleared once healed at the fountain. */
	bool bRetreating = false;

	/** The enemy Fluxborn it last chose to attack, which it keeps attacking so a windup is not thrown away. */
	TWeakObjectPtr<AActor> FarmTarget;
};
