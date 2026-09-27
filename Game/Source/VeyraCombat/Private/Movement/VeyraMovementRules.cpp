// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Movement/VeyraMovementRules.h"

#include "Tuning/VeyraCombatTuning.h"

namespace VeyraMovementRules
{
double ApplySoftCaps(double Speed, TConstArrayView<FVeyraSpeedSoftCap> SoftCaps)
{
	// Each band runs from one cap's start to the next's; speed inside it counts at the band's rate.
	double Result = Speed;
	for (int32 Index = 0; Index < SoftCaps.Num(); ++Index)
	{
		const FVeyraSpeedSoftCap& Cap = SoftCaps[Index];
		if (Speed <= Cap.From)
		{
			break;
		}
		const double BandEnd = SoftCaps.IsValidIndex(Index + 1) ? FMath::Min(Speed, SoftCaps[Index + 1].From) : Speed;
		Result -= (BandEnd - Cap.From) * (1.0 - Cap.Retained);
	}
	return Result;
}

double EffectiveSpeed(const FVeyraSpeedInputs& Inputs, const FVeyraMovementTuning& Tuning)
{
	if (Inputs.bStunned)
	{
		return 0.0;
	}
	double Speed = Inputs.MoveSpeed * (1.0 + Inputs.ConditionalBonus);
	Speed *= 1.0 - Inputs.StrongestSlow;
	Speed = ApplySoftCaps(Speed, Tuning.SoftCaps);
	const double Floor = FMath::Min(Tuning.SlowFloor, Inputs.BaseMoveSpeed);
	return FMath::Max(Speed, FMath::Max(Floor, 0.0));
}

bool IsHeadingToward(const FVector& From, const FVector& Heading, const FVector& Target, double MaxAngleDegrees)
{
	const FVector Moving = Heading.GetSafeNormal2D();
	const FVector ToTarget = (Target - From).GetSafeNormal2D();
	if (Moving.IsNearlyZero() || ToTarget.IsNearlyZero())
	{
		return false;
	}
	return FVector::DotProduct(Moving, ToTarget) >= FMath::Cos(FMath::DegreesToRadians(MaxAngleDegrees));
}
}
