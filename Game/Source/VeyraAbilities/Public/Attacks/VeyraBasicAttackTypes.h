// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Damage/VeyraDamageTypes.h"
#include "Misc/Optional.h"
#include "Shapes/VeyraShapes.h"
#include "Statuses/VeyraStatusTypes.h"
#include "Templates/Function.h"
#include "UObject/ObjectMacros.h"
#include "UObject/WeakObjectPtr.h"

#include "VeyraBasicAttackTypes.generated.h"

class AActor;
class UAbilitySystemComponent;

/** A ranged basic attack's projectile, which follows its target (Combat Bible §4). */
USTRUCT()
struct FVeyraAttackProjectileTuning
{
	GENERATED_BODY()

	/** Units per second. */
	UPROPERTY()
	double Speed = 0.0;

	/** Its own radius: it lands when it touches the target's body. */
	UPROPERTY()
	double Radius = 0.0;
};

/**
 * How a unit's basic attacks work (Combat Bible §4, §48; ADR-008 §2). A Vanguard definition carries
 * one; VeyraBasicAttacks::Validate checks it.
 */
USTRUCT()
struct FVeyraBasicAttackProfile
{
	GENERATED_BODY()

	/** Edge to edge, in units (Combat Bible §40). */
	UPROPERTY()
	double Range = 0.0;

	/** Physical unless the Vanguard overrides it (Combat Bible §4). */
	UPROPERTY()
	EVeyraDamageType DamageType = EVeyraDamageType::Physical;

	/** The attack's base damage: the attacker's Physical Power and Magic Power times these. */
	UPROPERTY()
	double PhysicalPowerRatio = 0.0;

	UPROPERTY()
	double MagicPowerRatio = 0.0;

	/** The part of each attack's interval before Commit, above 0 and below 1; the rest is backswing. */
	UPROPERTY()
	double WindupFraction = 0.0;

	/** How far past the attacker's edge attack-move looks for an enemy, in units. */
	UPROPERTY()
	double AcquisitionRadius = 0.0;

	/** The shortest time between attacks whatever the Attack Speed, in seconds; 0 for none (a kit's floor). */
	UPROPERTY()
	double MinimumIntervalSeconds = 0.0;

	/** A ranged attack's projectile; empty for a melee attack, which lands at Commit. At most one. */
	UPROPERTY()
	TArray<FVeyraAttackProjectileTuning> Projectile;

	/** Where the attack cleaves when something makes it cleave: at the attacker, facing the target. At most one. */
	UPROPERTY()
	TArray<FVeyraShape> Cleave;
};

/** Why a basic attack cannot start now. */
UENUM()
enum class EVeyraAttackRejection : uint8
{
	None,
	/** The unit has no basic attack profile. */
	NoProfile,
	/** The attacker is dead or has no body. */
	AttackerDead,
	/** Crowd control stops the attacker attacking (Combat Bible §8). */
	CrowdControlled,
	/** The attacker is winding up an attack or casting, or its movement is forced. */
	Busy,
	/** The previous attack's interval has not passed. */
	OnCooldown,
	/** The target is not a living enemy unit. */
	InvalidTarget,
	/** The target is beyond attack range, edge to edge (Combat Bible §40). */
	OutOfRange,
	/** The attacker cannot see the target: fog, stealth or Dense Fog (Vision Bible §1, §2; ADR-016 §2). */
	NotVisible,
};

VEYRAABILITIES_API const TCHAR* LexToString(EVeyraAttackRejection Rejection);

/** Where an attack is (Combat Bible §48). */
UENUM()
enum class EVeyraAttackPhase : uint8
{
	None,
	/** Before Commit: moving or acting cancels it and costs nothing. */
	Windup,
	/** After Commit: the hit is secured, and moving or acting may cut the backswing short. */
	Backswing,
};

/** An attack as every machine sees it, for presentation. */
USTRUCT()
struct FVeyraAttackState
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraAttackPhase Phase = EVeyraAttackPhase::None;

	UPROPERTY()
	TObjectPtr<AActor> Target;

	/** When the phase ends, in the server's world time. */
	UPROPERTY()
	double PhaseEndsAt = 0.0;
};

/** The other enemies an attack hits for part of its damage (ADR-009 §5). */
struct FVeyraAttackCleave
{
	/** The fraction of the attack's damage each takes, above 0 and at most 1. */
	double DamageFraction = 0.0;

	TArray<FVeyraStatusSpec> Statuses;
};

/**
 * An attack's secondary impact: proc damage in an area behind its target, which never re-enters the
 * hit pipeline (ADR-009 §5). An attack has at most one.
 */
struct FVeyraSecondaryImpact
{
	/** A higher priority replaces a lower one. */
	int32 Priority = 0;

	/** Placed at the target, facing away from the attacker. The target itself is never hit by it. */
	FVeyraShape Shape;

	FVeyraRawDamageEvent Damage;
	TArray<FVeyraStatusSpec> Statuses;
};

/**
 * One basic attack as its Commit builds it. The empowerment and the attack modifiers add to it before
 * its damage is prepared (Combat Bible §17, §50; ADR-009 §5).
 */
struct VEYRAABILITIES_API FVeyraAttackPlan
{
	TWeakObjectPtr<AActor> Target;

	/** Consecutive attacks at this enemy Vanguard, this one included; 0 when the target is not one. */
	int32 Chain = 0;

	/** Whether the attack consumed an empowerment (Combat Bible §17). */
	bool bEmpowered = false;

	/** Whether the attack crits (Combat Bible §5): its crit bonus is already in Damage, as a rider. */
	bool bCritical = false;

	FVeyraRawDamageEvent Damage;

	/**
	 * The attack's own damage, before an empowerment or a modifier adds riders to Damage. Against a
	 * structure only the riders are reduced to Structure Effectiveness (Combat Bible §33).
	 */
	FVeyraDamageComponents BaseDamage;

	TArray<FVeyraStatusSpec> TargetStatuses;
	TOptional<FVeyraAttackCleave> Cleave;
	TOptional<FVeyraSecondaryImpact> SecondaryImpact;

	/** Adds Amount of Type to the attack's damage, joining its component of that type (one per type, §25). */
	void AddDamage(EVeyraDamageType Type, double Amount);

	/** Takes Impact if the attack has none, or only one of lower priority. */
	void OfferSecondaryImpact(const FVeyraSecondaryImpact& Impact);
};

/** A basic attack's trigger: On Attack when it commits, On Hit when it connects (Combat Bible §16). */
struct FVeyraAttackEvent
{
	TWeakObjectPtr<UAbilitySystemComponent> Attacker;
	TWeakObjectPtr<AActor> Target;
	int32 Chain = 0;
	bool bEmpowered = false;
	bool bCritical = false;

	/** A blinded attacker's attack (ADR-028 §1): it counts as an attack, and lands nothing. */
	bool bMissed = false;
};

/** What presentation sees of an empowerment waiting for the next attack: which ability, and until when. */
USTRUCT()
struct FVeyraAttackEmpowermentView
{
	GENERATED_BODY()

	/** Invalid when no empowerment waits. */
	UPROPERTY()
	FVeyraContentId Ability;

	/** When it lapses unused, in the server's world time. */
	UPROPERTY()
	double ExpiresAt = 0.0;

	/** How many attacks it still empowers (ADR-027 §2). */
	UPROPERTY()
	int32 Attacks = 0;
};

/** An empowerment the next basic attack consumes at its Commit (Combat Bible §17). */
struct FVeyraAttackEmpowerment
{
	FVeyraContentId Ability;
	int32 CastId = 0;

	/** How long it waits for an attack, in seconds. */
	double DurationSeconds = 0.0;

	/** How many attacks it empowers, at least 1; each spends one (ADR-027 §2). */
	int32 Attacks = 1;

	/** What the empowered attacks' windups are multiplied by, in (0, 1]. */
	double WindupScale = 1.0;

	/** Adds the empowerment to the attack. */
	TFunction<void(FVeyraAttackPlan&)> Apply;
};

/** The basic attack rules that need no world (Combat Bible §4). */
namespace VeyraBasicAttacks
{
	/** Problems with Profile, each a field name and a message; empty when it can be used. */
	VEYRAABILITIES_API TArray<FString> Validate(const FVeyraBasicAttackProfile& Profile);

	/**
	 * The attack's damage as a structure takes it (Combat Bible §33): each type keeps its share of
	 * the attack's own damage in full, and Effectiveness of what riders added on top.
	 */
	VEYRAABILITIES_API FVeyraRawDamageEvent AgainstStructure(const FVeyraAttackPlan& Plan, double Effectiveness);
}
