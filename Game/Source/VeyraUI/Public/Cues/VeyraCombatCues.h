// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Attacks/VeyraBasicAttackTypes.h"
#include "Content/VeyraContentId.h"
#include "Misc/Optional.h"
#include "Teams/VeyraTeam.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "VeyraAbilityTypes.h"

class AActor;

/** A moment of a fight a client shows (ADR-063 §1). */
enum class EVeyraCombatCueKind : uint8
{
	/** An attack began its windup. */
	AttackWindup,
	/** An attack committed: a melee attack lands now, a ranged one looses its projectile. */
	AttackCommit,
	/** A unit lost Health or shields. */
	Hit,
	/** A cast began its windup. */
	CastWindup,
	/** A cast committed, whatever phases follow it. */
	CastCommit,
	/** A unit died. */
	Death,
	/** A Vanguard's Level rose (ADR-065 §5). */
	LevelUp,
	/** A projectile one of the unit's casts launched has ended, where the server ended it (ADR-072 §4). */
	ProjectileEnd,
};

/** One cue, as the presentation's views take it. */
struct FVeyraCombatCue
{
	EVeyraCombatCueKind Kind = EVeyraCombatCueKind::Hit;

	/** Whose moment it is: the attacker, the caster, the unit hit or the unit that died. */
	TWeakObjectPtr<const AActor> Unit;

	/** An attack's target. */
	TWeakObjectPtr<const AActor> Target;

	/** A cast's ability, and where it was aimed; a projectile end's ability, and where it ended. */
	FVeyraContentId Ability;
	FVector Location = FVector::ZeroVector;

	/** A projectile end's cast. */
	int32 CastId = 0;

	/** A hit's Health and shields lost; a level-up's new Level. */
	double Amount = 0.0;

	/** When an attack's or a cast's windup ends, in the server's world time. */
	double EndsAt = 0.0;
};

/** What a client saw of one unit at one moment: everything a cue is read from. */
struct FVeyraUnitSighting
{
	bool bAlive = false;

	/** Health and every shield together, and Max Health. */
	double Vitality = 0.0;
	double MaxHealth = 0.0;

	EVeyraAttackPhase AttackPhase = EVeyraAttackPhase::None;
	double AttackPhaseEndsAt = 0.0;
	TWeakObjectPtr<const AActor> AttackTarget;

	EVeyraCastPhase CastPhase = EVeyraCastPhase::None;
	double CastPhaseEndsAt = 0.0;
	int32 CastId = 0;
	FVeyraContentId CastAbility;
	FVector CastLocation = FVector::ZeroVector;

	/** Where the latest of its casts' projectiles ended (UVeyraCastStateComponent::GetLastProjectileEnd). */
	int32 ProjectileEndSerial = 0;
	FVeyraContentId ProjectileEndAbility;
	int32 ProjectileEndCastId = 0;
	FVector ProjectileEndLocation = FVector::ZeroVector;

	/** The latest committed cast (UVeyraCastStateComponent::GetLastCommit). */
	int32 CommitSerial = 0;
	FVeyraContentId CommitAbility;
	FVector CommitLocation = FVector::ZeroVector;

	/** A Vanguard's Level, which every client receives; 0 for any other unit. */
	int32 Level = 0;
};

/** Reading a fight's moments from the state a client already receives (ADR-063 §1). */
namespace VeyraCombatCues
{
	/**
	 * The cues between two sightings of Unit, in the order they happen: a hit, then a death, or else the attack's, the
	 * cast's and a level-up. A new windup or backswing is told apart from the old by its phase end; a lost Max Health is
	 * not a hit; a Level first seen is no level-up.
	 */
	VEYRAUI_API TArray<FVeyraCombatCue> Between(const AActor& Unit, const FVeyraUnitSighting& Before, const FVeyraUnitSighting& Now);

	/** What this client sees of Unit now, as Viewer's side; unset for anything but a unit with vitals. */
	VEYRAUI_API TOptional<FVeyraUnitSighting> Sight(const AActor& Unit, EVeyraTeam Viewer);
}
