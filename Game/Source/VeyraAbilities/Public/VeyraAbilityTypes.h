// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "Slots/VeyraAbilitySlot.h"
#include "UObject/ObjectMacros.h"

#include "VeyraAbilityTypes.generated.h"

/** What a cast is aimed at: a unit for targeted abilities, a ground point for areas and aimed abilities. */
USTRUCT()
struct FVeyraCastTarget
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> Actor = nullptr;

	/** Whether the cast has a ground point. */
	UPROPERTY()
	bool bHasLocation = false;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;
};

/** Why the server refused a cast. None means it was accepted. */
UENUM()
enum class EVeyraCastRejection : uint8
{
	None,
	/** Nothing is in that slot, or its content is unknown. */
	UnknownAbility,
	/** The caster's death is final. */
	CasterDead,
	/** Crowd control stops the caster casting, such as a Stun (Combat Bible §8). */
	CrowdControlled,
	/** The slot's rank is 0: no skill point has gone into it (ADR-008 §4). */
	NotLearned,
	/** Another cast's windup, channel or recovery holds the caster (Combat Bible §48). */
	Busy,
	OnCooldown,
	/** Not enough resource for the cost (Combat Bible §27). */
	InsufficientResource,
	/** No target, the caster itself, or something that is not a combatant. */
	InvalidTarget,
	TargetDead,
	/** The target is on the caster's side. */
	NotHostile,
	OutOfRange,
	/** The ability needs a ground point, and the cast has no usable one. */
	InvalidLocation,
	/** The match refuses casts in this phase. */
	WrongPhase,
	/** The match is paused (Match Flow Bible §10.2). */
	Paused,
	/** The Gameplay Ability System refused to activate the ability. */
	ActivationFailed,
	/** A Flux Spell slot its team's permanent Flux has not unlocked yet (Battleground Bible §14; ADR-015 §4). */
	Locked,
};

VEYRAABILITIES_API const TCHAR* LexToString(EVeyraCastRejection Rejection);

/** The phase of a cast that holds its caster (Combat Bible §26, §48; ADR-008 §4). */
UENUM()
enum class EVeyraCastPhase : uint8
{
	/** No cast holds the caster. */
	None,
	/** Before Commit. */
	Windup,
	/** After Commit, delivering over time with the caster in place. */
	Channel,
	/** After delivery, before the caster may cast again. */
	Recovery,
};
