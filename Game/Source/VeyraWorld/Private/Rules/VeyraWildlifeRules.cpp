// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Rules/VeyraWildlifeRules.h"

namespace VeyraWildlifeRules
{
TArray<FVector2D> Positions(const FVector2D& Center, int32 Count, double Spacing)
{
	TArray<FVector2D> Out;
	if (Count <= 1)
	{
		Out.Add(Center);
		return Out;
	}
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const double Angle = UE_TWO_PI * Index / Count;
		Out.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Spacing);
	}
	return Out;
}

bool IsWithinLeash(const FVector2D& Center, double LeashRadius, const FVector2D& Point)
{
	return FVector2D::Distance(Center, Point) <= LeashRadius;
}

bool KeepsFighting(const FVector2D& Center, double LeashRadius, const FVector2D& Self, const FVector2D& Target, bool bTargetValid)
{
	return bTargetValid && IsWithinLeash(Center, LeashRadius, Self) && IsWithinLeash(Center, LeashRadius, Target);
}
}
