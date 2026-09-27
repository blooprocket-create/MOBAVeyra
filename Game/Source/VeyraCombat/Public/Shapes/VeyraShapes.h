// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Math/Vector.h"
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
	 * then in a stable order (ADR-009 §4). Server only.
	 */
	VEYRACOMBAT_API TArray<AActor*> GatherUnits(const UWorld& World, const FVeyraPlacedShape& Placed, TFunctionRef<bool(const AActor&)> Include);
}
