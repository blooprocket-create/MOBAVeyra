// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraOwnLead.h"

#include "Math/UnrealMathUtility.h"

double VeyraOwnLead::LengthOf(const TArray<FVector2D>& Path)
{
	double Length = 0.0;
	for (int32 Index = 1; Index < Path.Num(); ++Index)
	{
		Length += FVector2D::Distance(Path[Index - 1], Path[Index]);
	}
	return Length;
}

FVector2D VeyraOwnLead::PointAt(const TArray<FVector2D>& Path, double Arc)
{
	if (Path.IsEmpty())
	{
		return FVector2D::ZeroVector;
	}
	double Remaining = FMath::Max(0.0, Arc);
	for (int32 Index = 1; Index < Path.Num(); ++Index)
	{
		const double Segment = FVector2D::Distance(Path[Index - 1], Path[Index]);
		if (Remaining <= Segment && Segment > 0.0)
		{
			return Path[Index - 1] + (Path[Index] - Path[Index - 1]) * (Remaining / Segment);
		}
		Remaining -= Segment;
	}
	return Path.Last();
}

double VeyraOwnLead::YawAt(const TArray<FVector2D>& Path, double Arc)
{
	// The segment the point lies on, the arriving one at a bend, the last past the end.
	FVector2D Way = FVector2D::ZeroVector;
	double Remaining = FMath::Max(0.0, Arc);
	for (int32 Index = 1; Index < Path.Num(); ++Index)
	{
		const FVector2D Segment = Path[Index] - Path[Index - 1];
		const double Length = Segment.Size();
		if (Length > 0.0)
		{
			Way = Segment / Length;
			if (Remaining < Length)
			{
				break;
			}
		}
		Remaining -= Length;
	}
	return FMath::RadiansToDegrees(FMath::Atan2(Way.Y, Way.X));
}

double VeyraOwnLead::ArcNearest(const TArray<FVector2D>& Path, const FVector2D& Point, double* OutDistance)
{
	double Best = TNumericLimits<double>::Max();
	double BestArc = 0.0;
	double Along = 0.0;
	if (Path.Num() == 1)
	{
		Best = FVector2D::Distance(Path[0], Point);
	}
	for (int32 Index = 1; Index < Path.Num(); ++Index)
	{
		const FVector2D From = Path[Index - 1];
		const FVector2D Segment = Path[Index] - From;
		const double LengthSquared = Segment.SizeSquared();
		const double Share = LengthSquared > 0.0 ? FMath::Clamp(FVector2D::DotProduct(Point - From, Segment) / LengthSquared, 0.0, 1.0) : 0.0;
		const double Distance = FVector2D::Distance(From + Segment * Share, Point);
		const double Length = FMath::Sqrt(LengthSquared);
		if (Distance < Best)
		{
			Best = Distance;
			BestArc = Along + Length * Share;
		}
		Along += Length;
	}
	if (OutDistance)
	{
		*OutDistance = Path.IsEmpty() ? 0.0 : Best;
	}
	return BestArc;
}

double VeyraOwnLead::ArcWithin(const TArray<FVector2D>& Path, const FVector2D& Centre, double Radius)
{
	if (Path.IsEmpty())
	{
		return 0.0;
	}
	if (FVector2D::Distance(Path[0], Centre) <= Radius)
	{
		return 0.0;
	}
	double Along = 0.0;
	for (int32 Index = 1; Index < Path.Num(); ++Index)
	{
		// Where the segment first enters the circle: the smaller root of |From + t·Segment − Centre| = Radius.
		const FVector2D From = Path[Index - 1];
		const FVector2D Segment = Path[Index] - From;
		const double Length = Segment.Size();
		const FVector2D Away = From - Centre;
		const double A = Segment.SizeSquared();
		const double B = 2.0 * FVector2D::DotProduct(Away, Segment);
		const double C = Away.SizeSquared() - Radius * Radius;
		const double Discriminant = B * B - 4.0 * A * C;
		if (A > 0.0 && Discriminant >= 0.0)
		{
			const double Enters = (-B - FMath::Sqrt(Discriminant)) / (2.0 * A);
			if (Enters >= 0.0 && Enters <= 1.0)
			{
				return Along + Length * Enters;
			}
		}
		Along += Length;
	}
	return Along;
}

FVeyraOwnLead VeyraOwnLead::Step(FVeyraOwnLeadState& State, const FVeyraOwnLeadInputs& In, const FVeyraOwnLeadShape& Shape)
{
	const double Delta = FMath::Max(0.0, In.DeltaSeconds);
	const FVector2D WasShown = State.Shown.Get(In.Drawn);

	// A new order: a straight line from where the body is shown now to its goal, until the server's path comes. The body
	// walks off from where it was shown, so it never jumps; it may lead at once.
	if (In.GivenAt != State.GivenAt)
	{
		State.GivenAt = In.GivenAt;
		State.Serial = INDEX_NONE;
		State.Path = { WasShown, In.Goal };
		State.Arc = 0.0;
		State.Ideal = 0.0;
		State.Step = FVector2D::ZeroVector;
		State.StandingFor = 0.0;
		State.Weight = In.bMayLead ? 1.0 : State.Weight;
	}
	else if (State.Serial == INDEX_NONE && State.Path.Num() == 2)
	{
		// Until then the line follows a chased unit as it moves.
		State.Path[1] = In.Goal;
	}
	// The server's path, once it comes, taken up where the body is shown: the step between the two closes gently.
	if (In.ServerPath.Num() >= 2 && In.ServerPathSerial != State.Serial)
	{
		const FVector2D Shown = PointAt(State.Path, State.Arc) + State.Step;
		const double IdealGap = State.Ideal - State.Arc;
		State.Path = In.ServerPath;
		State.Serial = In.ServerPathSerial;
		State.Arc = ArcNearest(State.Path, Shown);
		State.Ideal = State.Arc + IdealGap;
		State.Step = Shown - PointAt(State.Path, State.Arc);
	}

	// How fast the body truly walks: as the server moves it, slows and all, once it walks.
	const bool bServerWalks = In.ServerSpeed > In.Speed * Shape.StandingSpeedShare;
	if (bServerWalks)
	{
		State.WalkingSpeed = In.ServerSpeed;
	}
	const double Speed = State.WalkingSpeed > 0.0 ? State.WalkingSpeed : In.Speed;
	State.Ideal += Speed * Delta;

	// Where it stops: an attack within its reach of its target, as the server stops it to attack; a move where the server's
	// path following accepts it as arrived, short of the path's end by the arrival tolerance, so it never steps back to it.
	const double End = In.StopWithin ? ArcWithin(State.Path, In.Goal, *In.StopWithin) : FMath::Max(0.0, LengthOf(State.Path) - Shape.ArrivalTolerance);
	double OffPath = 0.0;
	const double ServerArc = ArcNearest(State.Path, In.Drawn, &OffPath);
	const bool bArrived = ServerArc >= End - Shape.ArrivalTolerance;

	// The server's body stands while its path still runs on: held by a stun, or stopped to attack. Its order needs a
	// moment to reach the server first.
	const bool bStanding = !bServerWalks && !bArrived && In.Now - In.GivenAt > In.LeadSeconds;
	State.StandingFor = bStanding ? State.StandingFor + Delta : 0.0;
	// Done once the server's body stands where it stops: the drawn body waits there for it, and only then lets the lead go,
	// with nothing left between them to pull back.
	const bool bDone = bArrived && !bServerWalks;
	const bool bLeads = In.bMayLead && !bDone && OffPath <= Shape.OffPathDistance && State.StandingFor <= Shape.StallSeconds;

	// Ahead of the server's body by what the order will have done by now, but never past what it could have done walking since
	// it was given, nor past the path's end; moving on no faster than its catch-up speed, and never back while it leads.
	// Where the server's body already is, the order has taken it at least; the body catches up to it at its catch-up speed.
	State.Ideal = FMath::Max(State.Ideal, ServerArc);
	const double Wanted = FMath::Min3(ServerArc + Speed * In.LeadSeconds, State.Ideal, End);
	const double WasArc = State.Arc;
	if (Wanted > State.Arc)
	{
		State.Arc = FMath::Min(Wanted, State.Arc + Speed * Shape.CatchUpScale * Delta);
	}
	// Never farther ahead of the server's body than the most a lead may be, and never past where it stops, even as a chased
	// unit comes nearer.
	State.Arc = FMath::Min3(State.Arc, ServerArc + Shape.MaxDistance, FMath::Max(End, ServerArc));

	// The step between paths closes, and the lead comes or goes, over their times.
	State.Step = Shape.SwitchSeconds > 0.0 ? State.Step * FMath::Max(0.0, 1.0 - Delta / Shape.SwitchSeconds) : FVector2D::ZeroVector;
	const double Toward = bLeads ? 1.0 : 0.0;
	State.Weight = Shape.BlendSeconds > 0.0 ? FMath::FInterpConstantTo(State.Weight, Toward, Delta, 1.0 / Shape.BlendSeconds) : Toward;

	FVeyraOwnLead Lead;
	const FVector2D Shown = PointAt(State.Path, State.Arc) + State.Step;
	Lead.Offset = (Shown - In.Drawn) * State.Weight;
	if (Lead.Offset.Size() > Shape.MaxDistance)
	{
		Lead.Offset = Lead.Offset.GetSafeNormal() * Shape.MaxDistance;
	}
	State.Shown = In.Drawn + Lead.Offset;
	if (State.Weight > 0.0)
	{
		Lead.Yaw = YawAt(State.Path, State.Arc);
		Lead.bWalks = bLeads && State.Arc > WasArc;
		Lead.Speed = Speed;
	}
	return Lead;
}
