// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Movement/VeyraMotion.h"

bool FVeyraMotion::operator==(const FVeyraMotion& Other) const
{
	return StartTime == Other.StartTime && Start == Other.Start && Corners == Other.Corners && Speed == Other.Speed && Yaw == Other.Yaw
		&& Kind == Other.Kind && bHeld == Other.bHeld && SetDownSerial == Other.SetDownSerial && OrderNumber == Other.OrderNumber
		&& Host == Other.Host;
}

namespace VeyraMotion
{
	bool Walks(const FVeyraMotion& Motion)
	{
		return (Motion.Kind == EVeyraMotionKind::Walk || Motion.Kind == EVeyraMotionKind::Forced) && !Motion.Corners.IsEmpty()
			&& FMath::IsFinite(Motion.Speed) && Motion.Speed > 0.0f;
	}

	double Length(const FVeyraMotion& Motion)
	{
		// Across the ground, as the server walks and forces a body: a slope's rise adds nothing to it.
		double Total = 0.0;
		FVector From = Motion.Start;
		for (const FVector& Corner : Motion.Corners)
		{
			Total += FVector::Dist2D(From, Corner);
			From = Corner;
		}
		return Total;
	}

	double EndTime(const FVeyraMotion& Motion)
	{
		return Walks(Motion) ? Motion.StartTime + Length(Motion) / Motion.Speed : Motion.StartTime;
	}

	FVeyraMotionSample Sample(const FVeyraMotion& Motion, double Time)
	{
		FVeyraMotionSample Result;
		Result.Location = Motion.Start;
		if (!Walks(Motion))
		{
			return Result;
		}
		// Before it began, it stands where it began; the distance across the ground runs on at its speed from then, as the
		// server's body covers it, and its height follows each leg.
		double Left = FMath::Max(0.0, Time - Motion.StartTime) * Motion.Speed;
		FVector From = Motion.Start;
		for (const FVector& Corner : Motion.Corners)
		{
			const FVector Leg = Corner - From;
			const double LegLength = Leg.Size2D();
			if (Left < LegLength)
			{
				const FVector PerUnit = Leg / LegLength;
				Result.Location = From + PerUnit * Left;
				Result.Velocity = PerUnit * Motion.Speed;
				Result.bEnded = false;
				return Result;
			}
			Left -= LegLength;
			From = Corner;
		}
		Result.Location = From;
		return Result;
	}

	TArray<FVector_NetQuantize> CornersWithin(const FVector& Start, TConstArrayView<FVector> Path, double MaxLength)
	{
		// A corner closer than this to the one before, across the ground, adds nothing a body would show.
		constexpr double SameCornerDistance = 1.0;
		TArray<FVector_NetQuantize> Corners;
		if (!(MaxLength > 0.0))
		{
			return Corners;
		}
		double Left = MaxLength;
		FVector From = Start;
		for (const FVector& Corner : Path)
		{
			const double LegLength = FVector::Dist2D(From, Corner);
			if (LegLength < SameCornerDistance)
			{
				continue;
			}
			if (LegLength >= Left)
			{
				Corners.Add(From + (Corner - From) / LegLength * Left);
				return Corners;
			}
			Corners.Add(Corner);
			Left -= LegLength;
			From = Corner;
		}
		return Corners;
	}
}
