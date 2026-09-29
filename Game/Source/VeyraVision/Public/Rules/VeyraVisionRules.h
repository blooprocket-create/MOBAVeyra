// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Shapes/VeyraShapes.h"
#include "Math/Vector2D.h"
#include "Teams/VeyraTeam.h"

/** Something that gives its team vision around it (Vision Bible §1). */
struct FVeyraSightSource
{
	EVeyraTeam Team = EVeyraTeam::None;
	FVector2D Position = FVector2D::ZeroVector;
	/** How far it sees, in units; above 0. */
	double Radius = 0.0;

	/** Whether it detects Camouflage: a Vanguard or a standing structure, never a ward (ADR-018 §4). */
	bool bDetects = false;

	/** A lit shape, which sees exactly what lies inside it; unset for a circle of Radius (ADR-018 §5). */
	TOptional<FVeyraPlacedShape> Shape;
};

/** A Dense Fog circle (Vision Bible §2): the battleground's bush, authored on the map or made by an ability. */
struct FVeyraFogCircle
{
	FVector2D Center = FVector2D::ZeroVector;
	/** In units; above 0. */
	double Radius = 0.0;
};

/** Vision's rules, as plain functions of positions (ADR-016 §2). */
namespace VeyraVisionRules
{
	/** Whether one of Team's Sources has Point within its sight: its circle, or its shape. */
	VEYRAVISION_API bool IsSeenBy(EVeyraTeam Team, TConstArrayView<FVeyraSightSource> Sources, const FVector2D& Point);

	/**
	 * Whether one of Team's detecting Sources has Point within both its sight and DetectionRadius: how
	 * a Camouflaged unit is seen (Combat Bible §11; ADR-018 §4).
	 */
	VEYRAVISION_API bool IsDetectedBy(EVeyraTeam Team, TConstArrayView<FVeyraSightSource> Sources, const FVector2D& Point, double DetectionRadius);

	/**
	 * The fog volumes: circles that overlap or touch are one volume while they do (Vision Bible §2).
	 * For each circle, its volume's number, from 0, in the order volumes first appear.
	 */
	VEYRAVISION_API TArray<int32> ConnectVolumes(TConstArrayView<FVeyraFogCircle> Circles);

	/** The volume holding Point, as numbered by ConnectVolumes (Volumes), or INDEX_NONE outside every circle. */
	VEYRAVISION_API int32 VolumeAt(TConstArrayView<FVeyraFogCircle> Circles, TConstArrayView<int32> Volumes, const FVector2D& Point);

	/** The first of Circles holding Point, or INDEX_NONE: the circle a presence ping names (ADR-016 §5). */
	VEYRAVISION_API int32 CircleAt(TConstArrayView<FVeyraFogCircle> Circles, const FVector2D& Point);
}
