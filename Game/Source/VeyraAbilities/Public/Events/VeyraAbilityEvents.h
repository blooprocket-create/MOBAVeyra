// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakObjectPtr.h"

#include "VeyraAbilityEvents.generated.h"

class AActor;
class UAbilitySystemComponent;

/** Which cast an ability's effects come from, for its On Ability Hit announcements. */
struct FVeyraAbilityHitSource
{
	FVeyraContentId Ability;

	/** The server's ID of the cast (Combat Bible §45). */
	int32 CastId = 0;

	/** Whether the ability itself gave its caster a shield for the unit it hits, as an area's per-Vanguard shield does. */
	bool bCasterShielded = false;

	/**
	 * Whether the impact skips the Spell Shield check (ADR-025 §4): a grab asked already, before it took
	 * hold, so one decision covers the hold, the hit and the statuses.
	 */
	bool bSkipSpellShield = false;
};

/** One ability connecting with a unit (Combat Bible §16, On Ability Hit), and the control it applied there. */
struct FVeyraAbilityHit
{
	TWeakObjectPtr<UAbilitySystemComponent> Caster;
	TWeakObjectPtr<AActor> Target;
	FVeyraContentId Ability;
	int32 CastId = 0;

	/** The hit carried damage: a damaging ability's hit, even when shields or invulnerability take it (§16). */
	bool bDamaging = false;

	/** A Stun from the hit landed. */
	bool bStunned = false;

	/** The hit displaced the unit. */
	bool bDisplaced = false;

	/** The ability gave its caster a shield for this unit (FVeyraAbilityHitSource). */
	bool bCasterShielded = false;
};

/**
 * Abilities' outcomes, announced as events to the systems above them (ARCHITECTURE.md §1.7): passives
 * listen for ability hits. Server only.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraAbilityEventSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Raised as each ability connects with a unit, even when the unit prevents its damage (§16). */
	TMulticastDelegate<void(const FVeyraAbilityHit&)> OnAbilityHit;

	/** Announces Hit in World's subsystem, if it has one. */
	static void Announce(const UWorld* World, const FVeyraAbilityHit& Hit);
};
