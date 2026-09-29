// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Attacks/VeyraBasicAttackTypes.h"
#include "Content/VeyraContentId.h"
#include "Progression/VeyraProgressionTypes.h"
#include "Stats/VeyraStatBlock.h"
#include "Tuning/VeyraAbilitiesTuning.h"
#include "Tuning/VeyraTuningProvenance.h"
#include "UObject/ObjectMacros.h"

#include "VeyraVanguardsTuning.generated.h"

// The Vanguards domain's tuning, bound from Game/Tuning/Vanguards.json (ADR-006 §6, ADR-008 §2). The
// schema holds every range; a 0 here only means "not loaded".

/** What a Vanguard spends to cast (Combat Bible §27; ADR-008 §2). Only Mana arrives with M5. */
UENUM()
enum class EVeyraResourceFamily : uint8
{
	Mana,
};

/** Who may play a Vanguard (ADR-010 §6). */
UENUM()
enum class EVeyraVanguardAvailability : uint8
{
	/** Released: players may own and pick it. */
	Playable,
	/** For tests and development servers only; Shipping match servers refuse it. */
	Developer,
};

/** A Vanguard's body in the world. */
USTRUCT()
struct FVeyraVanguardBodyTuning
{
	GENERATED_BODY()

	UPROPERTY()
	double CapsuleRadius = 0.0;

	/** Includes the capsule's rounded ends, so never less than the radius. */
	UPROPERTY()
	double CapsuleHalfHeight = 0.0;

	UPROPERTY()
	double TurnRateDegreesPerSecond = 0.0;
};

/** The abilities in a Vanguard's slots, by content ID; each at most one. */
USTRUCT()
struct FVeyraVanguardKitTuning
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FVeyraContentId> Q;

	UPROPERTY()
	TArray<FVeyraContentId> W;

	UPROPERTY()
	TArray<FVeyraContentId> E;

	UPROPERTY()
	TArray<FVeyraContentId> R;
};

/** One Vanguard (ADR-008 §2). */
USTRUCT()
struct FVeyraVanguardDefinition
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	EVeyraVanguardAvailability Availability = EVeyraVanguardAvailability::Developer;

	UPROPERTY()
	EVeyraResourceFamily Resource = EVeyraResourceFamily::Mana;

	UPROPERTY()
	FVeyraVanguardBodyTuning Body;

	/** The stats at level 1. */
	UPROPERTY()
	FVeyraStatBlock BaseStats;

	/** What each later level adds (Economy & Progression Bible §9). */
	UPROPERTY()
	FVeyraStatGrowth Growth;

	UPROPERTY()
	FVeyraBasicAttackProfile BasicAttack;

	UPROPERTY()
	FVeyraVanguardKitTuning Abilities;

	/** At most one passive, from one of the passive maps. */
	UPROPERTY()
	TArray<FVeyraContentId> Passive;
};

/**
 * A passive that shields its Vanguard whenever one of its abilities immobilizes an enemy Vanguard,
 * by a Stun or a displacement (Cairn's Deep Foundation, Character Bible §18).
 */
USTRUCT()
struct FVeyraDeepFoundationTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** The shield each immobilized enemy Vanguard grants; merging bounds several into one. */
	UPROPERTY()
	FVeyraShieldTuning Shield;

	/** How long the same enemy Vanguard cannot grant the shield again, in seconds. */
	UPROPERTY()
	double LockoutSeconds = 0.0;
};

/**
 * A generic passive that builds up over the hit chain, consecutive basic attacks on one enemy
 * Vanguard (ADR-008 §5, ADR-009 §5): each attack in the chain applies Status again, so a stacking
 * status gains a stack per hit, and the chain ending takes it away (Qazharr's Sea Dog, Character
 * Bible §13).
 */
USTRUCT()
struct FVeyraHitChainTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** A status ID from Abilities.json's statuses map, put on the passive's Vanguard. */
	UPROPERTY()
	FVeyraContentId Status;
};

/**
 * Oriel's Gathering Light (Character Bible §20). Each damaging ability cast that hits an enemy
 * Vanguard adds one stack. At StacksToPrime it is primed, and the next such cast consumes the stacks
 * and sends a homing fragment at one struck Vanguard she can acquire. The stacks end at death
 * (ADR-008 §9).
 */
USTRUCT()
struct FVeyraGatheringLightTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Canon gives three (§20). */
	UPROPERTY()
	int32 StacksToPrime = 0;

	/** The fragment's damage, prepared when it launches (Combat Bible §50); one amount, since a passive has no ranks. */
	UPROPERTY()
	FVeyraDamageTuning FragmentDamage;

	/** How the fragment flies: a homing projectile, which terrain does not stop (ADR-008 §9). */
	UPROPERTY()
	FVeyraAttackProjectileTuning Fragment;
};

/**
 * Bryn's Breach (Character Bible §19). Every HitsToBreach-th consecutive basic attack on the same
 * enemy Vanguard, the hit chain, consumes Breach: the attack deals BonusDamage and offers Impact, one
 * explosion behind the target. An impact of higher priority, such as Breach Round's, replaces it.
 * Neither re-enters the hit pipeline (ADR-009 §5), and changing targets starts the count again.
 */
USTRUCT()
struct FVeyraBreachTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Canon gives the third hit (§19). */
	UPROPERTY()
	int32 HitsToBreach = 0;

	/** Added to the breaching attack's own damage; one amount, since a passive has no ranks. */
	UPROPERTY()
	FVeyraDamageTuning BonusDamage;

	/** The explosion behind the target; one amount for each damage component. */
	UPROPERTY()
	FVeyraSecondaryImpactTuning Impact;
};

/** Dead Reckoning: displacement banked toward an empowered attack on a Tracked target (Roster Bible §2). */
USTRUCT()
struct FVeyraDeadReckoningTuning
{
	GENERATED_BODY()

	/** Units banked before the next attack on a Tracked target spends them. */
	UPROPERTY()
	double ThresholdUnits = 0.0;

	/** The most that bank. */
	UPROPERTY()
	double CapUnits = 0.0;

	/** The empowered attack's bonus; one amount, since a passive has no ranks. */
	UPROPERTY()
	FVeyraDamageTuning Damage;

	/** And this much more Physical Power ratio for each 100 units spent. */
	UPROPERTY()
	double PhysicalPowerRatioPerHundredUnits = 0.0;
};

/**
 * Kade's Moving Target (Roster Bible §2). An enemy Vanguard that Kade or an ally displaces becomes
 * Tracked: a status from Kade that lengthens his range against it, and his attacks on it deal bonus
 * damage. The displacement also banks toward Dead Reckoning. It reads Combat's OnDisplaced; no core
 * system names it. Its data is an entry in Vanguards.json's movingTarget map.
 */
USTRUCT()
struct FVeyraMovingTargetTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** From Abilities.json's statuses: a source-relative range, so only Kade's reach grows (ADR-018 §2). */
	UPROPERTY()
	FVeyraContentId TrackedStatus;

	/** Added to his attacks on a target he has Tracked; one amount. */
	UPROPERTY()
	FVeyraDamageTuning TrackedDamage;

	UPROPERTY()
	FVeyraDeadReckoningTuning DeadReckoning;
};

USTRUCT()
struct FVeyraVanguardsTuning
{
	GENERATED_BODY()

	/** The Vanguards.json format this build reads (a schema version marker, not tuning). */
	static constexpr int32 SchemaVersion = 5;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraVanguardDefinition> Vanguards;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraDeepFoundationTuning> DeepFoundation;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraHitChainTuning> HitChain;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraGatheringLightTuning> GatheringLight;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraBreachTuning> Breach;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraMovingTargetTuning> MovingTarget;
};

/** The Vanguards domain's rules for its tuning (ADR-008 §2, §5). */
namespace VeyraVanguardRules
{
	/**
	 * Problems with Tuning a schema cannot express, each a JSON pointer and a message: bodies, basic
	 * attacks, abilities the Abilities tuning does not define or whose rank lists do not suit their
	 * slot, passives no passive map defines, a passive ID in more than one map, and passive statuses
	 * the Abilities tuning does not define.
	 */
	VEYRAVANGUARDS_API TArray<FString> Validate(const FVeyraVanguardsTuning& Tuning, const FVeyraAbilitiesTuning& Abilities, int32 BasicAbilityMaxRank,
		int32 UltimateMaxRank);
}
