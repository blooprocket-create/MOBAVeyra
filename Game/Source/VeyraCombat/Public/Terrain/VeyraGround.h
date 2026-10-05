// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Engine/EngineTypes.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"

class UPrimitiveComponent;
class UWorld;

/**
 * The playable ground (ADR-040 §4). The battleground's Landscape and generated floors sit on their own collision object
 * channel (DefaultEngine.ini: VeyraGround), so a query for ground finds only ground, never a wall or a unit standing on
 * it, and a sweep for walls (WorldStatic) never meets a slope. Every body blocks the channel, so units walk on it.
 */
namespace VeyraGround
{
	/** The ground's object channel (DefaultEngine.ini: VeyraGround). */
	inline constexpr ECollisionChannel Channel = ECC_GameTraceChannel4;

	/** The collision profile that makes a component playable ground (DefaultEngine.ini). */
	VEYRACOMBAT_API FName ProfileName();

	/** Makes Component playable ground: on the ground's channel, blocking what ground blocks. */
	VEYRACOMBAT_API void MakeGround(UPrimitiveComponent& Component);

	/** The ground's surface under Point, the topmost between TopZ and BottomZ, with its normal; false where none is. */
	VEYRACOMBAT_API bool Find(const UWorld& World, const FVector2D& Point, double TopZ, double BottomZ, FHitResult& OutHit);

	/**
	 * The ground's surface under Near's place, looked for Combat tuning's search height above and below Near: where a
	 * body or a point near that height stands. False where there is no ground, as in a world built without any.
	 */
	VEYRACOMBAT_API bool Under(const UWorld& World, const FVector& Near, FVector& OutSurface);

	/**
	 * Where a body HalfHeight tall would stand at Point's place: its ground's surface raised by HalfHeight, looked for
	 * around Point's height. Point itself where there is no ground, so a world without ground keeps its old heights.
	 */
	VEYRACOMBAT_API FVector StandingAt(const UWorld& World, const FVector& Point, double HalfHeight);

	/**
	 * From carried to To's place: as high above the ground there as From is above its own, so a body's centre stays a
	 * body's centre up a slope or down into the river. At From's height where either place has no ground.
	 */
	VEYRACOMBAT_API FVector Carried(const UWorld& World, const FVector& From, const FVector2D& To);
}
