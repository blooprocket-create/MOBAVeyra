// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Damage/VeyraDamageTypes.h"
#include "Engine/NetSerialization.h"
#include "GameFramework/Actor.h"

#include "VeyraCombatTextTypes.generated.h"

/** What a floating combat text number tells its player (Settings Bible §3.4; ADR-052 §1). */
UENUM()
enum class EVeyraCombatTextKind : uint8
{
	/** Damage the player, or a unit it owns, dealt to an enemy. */
	DamageDealt,
	/** Damage dealt to the player's Vanguard. */
	DamageReceived,
	/** Health a unit restored, given or received by the player. */
	Healing,
	/** A shield granted, given or received by the player. */
	Shielding,
	/** Gold the player earned (ADR-065 §4). */
	Gold,
};

/**
 * One combat text number, as the server sends it to the one player it concerns (ADR-052 §1). Presentation only:
 * nothing reads it to decide anything.
 */
USTRUCT()
struct VEYRAMATCH_API FVeyraCombatTextLine
{
	GENERATED_BODY()

	/** The unit the number shows at: the one damaged, healed or shielded. */
	UPROPERTY()
	TObjectPtr<AActor> Unit;

	/** The unit at the other end, while the player's side sees it; Reduced density totals by it. */
	UPROPERTY()
	TObjectPtr<AActor> Other;

	UPROPERTY()
	EVeyraCombatTextKind Kind = EVeyraCombatTextKind::DamageDealt;

	/** For damage, its type; each type of a hit is its own number. */
	UPROPERTY()
	EVeyraDamageType DamageType = EVeyraDamageType::Physical;

	/** A basic attack that crit dealt it. */
	UPROPERTY()
	bool bCritical = false;

	UPROPERTY()
	float Amount = 0.0f;

	/**
	 * Gold earned by a unit's fall shows where it fell, and stays there though the body goes (ADR-065 §4); every other
	 * number follows its unit.
	 */
	UPROPERTY()
	bool bFixed = false;

	UPROPERTY()
	FVector_NetQuantize Where = FVector::ZeroVector;
};
