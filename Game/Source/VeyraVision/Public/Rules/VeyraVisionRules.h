// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Math/Vector2D.h"
#include "Teams/VeyraTeam.h"

/** Something that gives its team vision around it (Vision Bible §1). */
struct FVeyraSightSource
{
	EVeyraTeam Team = EVeyraTeam::None;
	FVector2D Position = FVector2D::ZeroVector;
	/** How far it sees, in units; above 0. */
	double Radius = 0.0;
};

/** Vision's rules, as plain functions of positions (ADR-016 §2). */
namespace VeyraVisionRules
{
	/** Whether one of Team's Sources has Point within its sight. */
	VEYRAVISION_API bool IsSeenBy(EVeyraTeam Team, TConstArrayView<FVeyraSightSource> Sources, const FVector2D& Point);
}
