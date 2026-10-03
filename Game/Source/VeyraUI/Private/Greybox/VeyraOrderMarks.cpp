// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraOrderMarks.h"

#include "GameFramework/Actor.h"
#include "Greybox/VeyraGreyboxSettings.h"

namespace VeyraOrderMarks
{
TOptional<FVeyraOrderMarkRing> Describe(const FVeyraOrderMark& Mark, double Now, bool bStill, const UVeyraGreyboxSettings& Settings)
{
	const double Elapsed = Now - Mark.GivenAt;
	if (Elapsed < 0.0 || Elapsed >= Settings.OrderMarkSeconds)
	{
		return {};
	}
	FVeyraOrderMarkRing Ring;
	double Edge = 0.0;
	if (Mark.Kind == EVeyraOrderMarkKind::Attack)
	{
		const AActor* Target = Mark.Target.Get();
		if (!Target || Target->IsHidden())
		{
			return {};
		}
		float Radius = 0.0f;
		float HalfHeight = 0.0f;
		Target->GetSimpleCollisionCylinder(Radius, HalfHeight);
		Ring.Centre = Target->GetActorLocation();
		Edge = Radius;
	}
	else
	{
		Ring.Centre = Mark.Location;
	}
	const double Progress = Elapsed / Settings.OrderMarkSeconds;
	// It closes fast, then settles, as an ease out does.
	const double Closed = 1.0 - FMath::Square(1.0 - Progress);
	Ring.Radius = Edge + (bStill ? Settings.OrderMarkEndRadius : FMath::Lerp(Settings.OrderMarkStartRadius, Settings.OrderMarkEndRadius, Closed));
	Ring.Color = Mark.Kind == EVeyraOrderMarkKind::Move ? Settings.OrderMoveColor : Settings.OrderAttackColor;
	Ring.Color.A *= static_cast<float>(1.0 - Progress);
	return Ring;
}
}
