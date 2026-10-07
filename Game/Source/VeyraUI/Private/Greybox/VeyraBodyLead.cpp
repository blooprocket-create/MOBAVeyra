// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraBodyLead.h"

FVeyraBodyLead VeyraBodyLead::For(const FVector& Point, double OrderAge, const FVector& BodyLocation, const FVector& Velocity, double MoveSpeed, bool bRun,
	double LeadSeconds, double AlignDegrees, double ArrivalRadius)
{
	FVeyraBodyLead Lead;
	const FVector Toward = (Point - BodyLocation).GetSafeNormal2D();
	if (OrderAge < 0.0 || OrderAge >= LeadSeconds || FVector::Dist2D(Point, BodyLocation) <= ArrivalRadius || Toward.IsNearlyZero())
	{
		return Lead;
	}
	// The server's movement has taken over once the body heads that way itself.
	const FVector Heading = Velocity.GetSafeNormal2D();
	if (!Heading.IsNearlyZero() && FVector::DotProduct(Heading, Toward) >= FMath::Cos(FMath::DegreesToRadians(AlignDegrees)))
	{
		return Lead;
	}
	Lead.bLeads = true;
	Lead.Yaw = Toward.Rotation().Yaw;
	Lead.GroundSpeed = bRun ? MoveSpeed : 0.0;
	return Lead;
}
