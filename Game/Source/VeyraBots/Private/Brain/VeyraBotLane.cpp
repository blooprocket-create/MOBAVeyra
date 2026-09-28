// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Brain/VeyraBotLane.h"

namespace VeyraBotLane
{
double DistanceAlong(TConstArrayView<FVector2D> Path, const FVector2D& Location)
{
	if (Path.Num() < 2)
	{
		return 0.0;
	}
	double Walked = 0.0;
	double Best = 0.0;
	double BestGap = TNumericLimits<double>::Max();
	for (int32 Index = 0; Index + 1 < Path.Num(); ++Index)
	{
		const FVector2D Start = Path[Index];
		const FVector2D Segment = Path[Index + 1] - Start;
		const double Length = Segment.Size();
		const double Along = Length > 0.0 ? FMath::Clamp(FVector2D::DotProduct(Location - Start, Segment) / Length, 0.0, Length) : 0.0;
		const double Gap = FVector2D::Distance(Location, Length > 0.0 ? Start + Segment / Length * Along : Start);
		if (Gap < BestGap)
		{
			BestGap = Gap;
			Best = Walked + Along;
		}
		Walked += Length;
	}
	return Best;
}

FVector2D PointAt(TConstArrayView<FVector2D> Path, double Distance)
{
	if (Path.IsEmpty())
	{
		return FVector2D::ZeroVector;
	}
	double Left = FMath::Max(0.0, Distance);
	for (int32 Index = 0; Index + 1 < Path.Num(); ++Index)
	{
		const FVector2D Segment = Path[Index + 1] - Path[Index];
		const double Length = Segment.Size();
		if (Left <= Length && Length > 0.0)
		{
			return Path[Index] + Segment / Length * Left;
		}
		Left -= Length;
	}
	return Path.Last();
}

double HoldDistance(TOptional<double> WaveFront, double OwnStructure, TOptional<double> EnemyReach, bool bWaveHoldsTower, double FollowDistance)
{
	double Hold = WaveFront.IsSet() ? WaveFront.GetValue() - FollowDistance : OwnStructure;
	if (EnemyReach.IsSet() && !bWaveHoldsTower)
	{
		Hold = FMath::Min(Hold, EnemyReach.GetValue());
	}
	return FMath::Max(0.0, Hold);
}
}
