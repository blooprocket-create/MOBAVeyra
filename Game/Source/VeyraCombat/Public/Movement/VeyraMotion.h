// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "VeyraMotion.generated.h"

/** What a unit's body is doing, as the server tells every machine that sees it (ADR-079 §1). */
UENUM()
enum class EVeyraMotionKind : uint8
{
	/** It stands where it began, facing its yaw. */
	Stand,
	/** It walks its corners at its speed: a path, a pursuit, a ride. */
	Walk,
	/** Something other than its orders moves it along its corners: a dash, a displacement, a Fear. */
	Forced,
	/** It holds on to its host, at the host's back. */
	Attached,
};

/**
 * A unit's motion (ADR-079 §1): where it began, when on the server's clock, and the corners it walks through at its speed.
 * Every machine works out where the body is at any moment from it, so a body is drawn smoothly between the server's
 * messages however rarely they come, and stops, turns and lands exactly where the server's own does.
 */
USTRUCT()
struct VEYRACOMBAT_API FVeyraMotion
{
	GENERATED_BODY()

	/** When it began, in the server's world seconds. */
	UPROPERTY()
	double StartTime = 0.0;

	/** Where the body's centre stood as it began. */
	UPROPERTY()
	FVector_NetQuantize Start = FVector::ZeroVector;

	/** The corners it walks through after Start, in order; the last is where it stops. None for Stand and Attached. */
	UPROPERTY()
	TArray<FVector_NetQuantize> Corners;

	/** How fast it walks its corners, in units a second. */
	UPROPERTY()
	float Speed = 0.0f;

	/** Its facing as it began, in degrees; a standing body turns to it. */
	UPROPERTY()
	float Yaw = 0.0f;

	UPROPERTY()
	EVeyraMotionKind Kind = EVeyraMotionKind::Stand;

	/** Changes each time the body is set down somewhere else at once (a blink, a respawn): it is drawn there at once, never glided. */
	UPROPERTY()
	uint8 SetDownSerial = 0;

	/** The player's order this motion answers, 0 for none: its own player's machine draws it ahead by what the order waited (§3). */
	UPROPERTY()
	uint16 OrderNumber = 0;

	/** Attached: the host it holds on to. */
	UPROPERTY()
	TObjectPtr<AActor> Host = nullptr;

	bool operator==(const FVeyraMotion& Other) const;
	bool operator!=(const FVeyraMotion& Other) const { return !(*this == Other); }
};

/** Where a motion has a body at some moment. */
struct FVeyraMotionSample
{
	FVector Location = FVector::ZeroVector;
	/** Its velocity there: zero once it stands. */
	FVector Velocity = FVector::ZeroVector;
	/** Whether it has walked all its corners, or never had any. */
	bool bEnded = true;
};

/** Pure rules of a motion (ADR-079 §1). */
namespace VeyraMotion
{
	/** Where Motion has its body at Time (server seconds): at Start before it began, at its last corner once it ends. */
	VEYRACOMBAT_API FVeyraMotionSample Sample(const FVeyraMotion& Motion, double Time);

	/** The length of its corners from Start. */
	VEYRACOMBAT_API double Length(const FVeyraMotion& Motion);

	/** When it ends, in server seconds: its StartTime for one with nothing to walk. */
	VEYRACOMBAT_API double EndTime(const FVeyraMotion& Motion);

	/** Whether it walks: it has corners to walk and a speed to walk them at. */
	VEYRACOMBAT_API bool Walks(const FVeyraMotion& Motion);

	/**
	 * The corners from Start along Path (each a corner, in order) cut off after MaxLength, so a motion tells no more of a
	 * path than its body walks in the next while (§2). Corners on top of Start or of each other are left out.
	 */
	VEYRACOMBAT_API TArray<FVector_NetQuantize> CornersWithin(const FVector& Start, TConstArrayView<FVector> Path, double MaxLength);
}
