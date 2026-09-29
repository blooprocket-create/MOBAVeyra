// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Rules/VeyraVisionRules.h"

namespace VeyraVisionRules
{
bool IsSeenBy(EVeyraTeam Team, TConstArrayView<FVeyraSightSource> Sources, const FVector2D& Point)
{
	for (const FVeyraSightSource& Source : Sources)
	{
		if (Source.Team == Team && FVector2D::DistSquared(Source.Position, Point) <= FMath::Square(Source.Radius))
		{
			return true;
		}
	}
	return false;
}
}
