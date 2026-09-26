// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "UObject/ObjectMacros.h"

#include "VeyraAbilityTypes.generated.h"

/** A Vanguard's ability slots (Settings Bible §1.2: Q/W/E/R). Basic attacks, Flux Spells and item actives get theirs later. */
UENUM()
enum class EVeyraAbilitySlot : uint8
{
	Q,
	W,
	E,
	R,
};

/** What a cast is aimed at. Targeted abilities use Actor; locations arrive with skillshots. */
USTRUCT()
struct FVeyraCastTarget
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> Actor = nullptr;
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
	OnCooldown,
	/** Not enough resource for the cost (Combat Bible §27). */
	InsufficientResource,
	/** No target, the caster itself, or something that is not a combatant. */
	InvalidTarget,
	TargetDead,
	/** The target is on the caster's side. */
	NotHostile,
	OutOfRange,
	/** The match refuses casts in this phase. */
	WrongPhase,
	/** The match is paused (Match Flow Bible §10.2). */
	Paused,
	/** The Gameplay Ability System refused to activate the ability. */
	ActivationFailed,
};

VEYRAABILITIES_API const TCHAR* LexToString(EVeyraCastRejection Rejection);
