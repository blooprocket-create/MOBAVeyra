// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AActor;

/** What the local player's last order was, as its mark on the ground shows it (ADR-062 §6). */
enum class EVeyraOrderMarkKind : uint8
{
	Move,
	AttackMove,
	/** An attack order naming a unit: the mark closes on the unit. */
	Attack,
};

/**
 * The local player's last order as they gave it, for the mark the presentation draws at once, before the
 * server answers. Presentation only: the server never reads it and it decides nothing.
 */
struct FVeyraOrderMark
{
	EVeyraOrderMarkKind Kind = EVeyraOrderMarkKind::Move;

	/** Where a move or an Attack Move was ordered. */
	FVector Location = FVector::ZeroVector;

	/** The unit an attack order named. */
	TWeakObjectPtr<const AActor> Target;

	/** When it was given, in this machine's real seconds. */
	double GivenAt = 0.0;

	/** Which of the player's orders it is: the server names it when it refuses the order. Never 0. */
	uint16 Number = 0;

	/** Whether the server refused it: its body is no longer drawn heading its way (ADR-077 §2). */
	bool bRefused = false;
};
