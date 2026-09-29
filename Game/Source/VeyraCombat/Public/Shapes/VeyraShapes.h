// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Math/Vector.h"
#include "Misc/Optional.h"
#include "Targeting/VeyraTargeting.h"
#include "Templates/Function.h"
#include "UObject/ObjectMacros.h"

#include "VeyraShapes.generated.h"

class AActor;
class UWorld;

/** The hit shapes of areas and sweeps (ADR-009 §4). */
UENUM()
enum class EVeyraShapeKind : uint8
{
	/** Radius around the origin. */
	Circle,
	/** An arc or cone of Radius, ArcDegrees wide, centred on the direction. */
	Sector,
	/** Length along the direction from the origin, Width across it, centred on the direction. */
	Rectangle,
};

/** A hit shape's size, as data. Each kind reads its own fields; the others are 0 (VeyraShapes::Validate). */
USTRUCT()
struct VEYRACOMBAT_API FVeyraShape
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraShapeKind Kind = EVeyraShapeKind::Circle;

	/** Circle and Sector, in units. */
	UPROPERTY()
	double Radius = 0.0;

	/** Sector: the whole angle of the arc, in degrees; above 0 and at most 360. */
	UPROPERTY()
	double ArcDegrees = 0.0;

	/** Rectangle, in units. */
	UPROPERTY()
	double Length = 0.0;

	UPROPERTY()
	double Width = 0.0;
};

/** A shape placed on the ground: its origin, and the direction a sector or rectangle faces. */
struct FVeyraPlacedShape
{
	FVeyraShape Shape;
	FVector Origin = FVector::ZeroVector;
	FVector Direction = FVector::ForwardVector;
};

/** A unit a moving circle reaches, and how far along its path it first touches the unit's body. */
struct FVeyraPathHit
{
	AActor* Unit = nullptr;
	double Distance = 0.0;
};

/** Combat's hit geometry (Combat Bible §13, §40), on the ground plane. */
namespace VeyraShapes
{
	/** Problems with Shape, each a field name and a message; empty when it can be used. */
	VEYRACOMBAT_API TArray<FString> Validate(const FVeyraShape& Shape);

	/** Whether a body circle of BodyRadius at Center touches the placed shape, edges included (§40). */
	VEYRACOMBAT_API bool Touches(const FVeyraPlacedShape& Placed, const FVector& Center, double BodyRadius);

	/** The farthest any point of Shape lies from its origin. */
	VEYRACOMBAT_API double Reach(const FVeyraShape& Shape);

	/**
	 * The living units whose bodies touch Placed and that Include accepts, nearest the origin first,
	 * then in a stable order (ADR-009 §4). Structures are gathered only when Structures allows them:
	 * areas, cleaves and impacts never hit them (Combat Bible §33), but an attack-move may pick one.
	 * Server only.
	 */
	VEYRACOMBAT_API TArray<AActor*> GatherUnits(const UWorld& World, const FVeyraPlacedShape& Placed, TFunctionRef<bool(const AActor&)> Include,
		EVeyraStructureTargeting Structures = EVeyraStructureTargeting::Refuse);

	/**
	 * How far along the path from Start to End a circle of Radius moving along it first touches a body
	 * circle of BodyRadius at Center, edges included; nothing if it never does. 0 when they touch at Start.
	 */
	VEYRACOMBAT_API TOptional<double> FirstContactAlong(const FVector& Start, const FVector& End, double Radius, const FVector& Center, double BodyRadius);

	/**
	 * The living units that a circle of Radius touches moving from Start to End and that Include
	 * accepts, in the order it reaches them, then in a stable order (ADR-009 §4). Structures are never
	 * gathered (Combat Bible §33). Server only.
	 */
	VEYRACOMBAT_API TArray<FVeyraPathHit> GatherUnitsAlong(const UWorld& World, const FVector& Start, const FVector& End, double Radius,
		TFunctionRef<bool(const AActor&)> Include);
}
