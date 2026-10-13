// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"

#include "VeyraOwnPath.generated.h"

/**
 * The path the server walks a player's own Vanguard along, sent to that player's machine alone, which draws the body ahead
 * along it (ADR-077 §3). Presentation only: the server never reads it back, and no other machine receives it, so it tells
 * no one else where the Vanguard is going.
 */
USTRUCT()
struct VEYRAMATCH_API FVeyraOwnPath
{
	GENERATED_BODY()

	/** The body the path moves: the player's Vanguard, or a body the player commands in its stead. */
	UPROPERTY()
	TObjectPtr<APawn> Body;

	/** The path's points, from where the body stood as the server planned it to where it ends. */
	UPROPERTY()
	TArray<FVector_NetQuantize10> Points;

	/** The point the order it answers named, as the player's machine sent it: a move's or an Attack Move's. */
	UPROPERTY()
	FVector_NetQuantize10 Ordered = FVector::ZeroVector;

	/**
	 * The unit the path chases, where the server stops the body within its attack's reach: an attack order's, or the enemy an
	 * Attack Move took on its way; none while it walks to a point.
	 */
	UPROPERTY()
	TObjectPtr<AActor> Target;

	/** Counts each path the server sends, so the player's machine tells a new one from the last. */
	UPROPERTY()
	uint16 Serial = 0;
};
