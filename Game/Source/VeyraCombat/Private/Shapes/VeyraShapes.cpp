// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shapes/VeyraShapes.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Targeting/VeyraTargeting.h"
#include "Units/VeyraUnit.h"

namespace VeyraShapes
{
namespace
{
	// A full turn, which a sector as wide covers every direction of.
	constexpr double FullTurnDegrees = 360.0;

	FVector2D Flat(const FVector& Vector)
	{
		return FVector2D(Vector.X, Vector.Y);
	}

	/** Distance from Point to the segment from A to B, on the ground. */
	double DistanceToSegment(const FVector2D& Point, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D AB = B - A;
		const double LengthSquared = AB.SizeSquared();
		const double T = LengthSquared > 0.0 ? FMath::Clamp(FVector2D::DotProduct(Point - A, AB) / LengthSquared, 0.0, 1.0) : 0.0;
		return FVector2D::Distance(Point, A + AB * T);
	}

	bool IsPositiveFinite(double Value)
	{
		return FMath::IsFinite(Value) && Value > 0.0;
	}

	/**
	 * Visits each living unit in World that Include accepts. Structures are among them only when
	 * Structures allows: areas, skillshots, cleaves and secondary impacts do not hit structures
	 * (Combat Bible §33).
	 */
	template <typename VisitorType>
	void ForEachUnit(const UWorld& World, TFunctionRef<bool(const AActor&)> Include, EVeyraStructureTargeting Structures, VisitorType&& Visit)
	{
		for (TActorIterator<APawn> It(&World); It; ++It)
		{
			APawn* Unit = *It;
			const TOptional<EVeyraUnitKind> Kind = VeyraUnits::KindOf(Unit);
			const bool bExcluded = Kind.IsSet() && Kind.GetValue() == EVeyraUnitKind::Structure && Structures == EVeyraStructureTargeting::Refuse;
			if (Kind.IsSet() && !bExcluded && VeyraTargeting::IsAlive(Unit) && Include(*Unit))
			{
				Visit(*Unit);
			}
		}
	}

	/** Nearest first, then in a stable order, so every run resolves hits alike. */
	void SortByDistance(TArray<FVeyraPathHit>& Hits)
	{
		Hits.Sort([](const FVeyraPathHit& A, const FVeyraPathHit& B) {
			return A.Distance != B.Distance ? A.Distance < B.Distance : A.Unit->GetUniqueID() < B.Unit->GetUniqueID();
		});
	}
}

TArray<FString> Validate(const FVeyraShape& Shape)
{
	TArray<FString> Problems;
	const bool bRound = Shape.Kind == EVeyraShapeKind::Circle || Shape.Kind == EVeyraShapeKind::Sector;
	if (bRound != IsPositiveFinite(Shape.Radius) || (!bRound && Shape.Radius != 0.0))
	{
		Problems.Add(TEXT("radius: above 0 for a circle or sector, and 0 otherwise"));
	}
	const bool bSector = Shape.Kind == EVeyraShapeKind::Sector;
	if (bSector ? !(IsPositiveFinite(Shape.ArcDegrees) && Shape.ArcDegrees <= FullTurnDegrees) : Shape.ArcDegrees != 0.0)
	{
		Problems.Add(TEXT("arcDegrees: above 0 and at most 360 for a sector, and 0 otherwise"));
	}
	const bool bRectangle = Shape.Kind == EVeyraShapeKind::Rectangle;
	if (bRectangle ? !(IsPositiveFinite(Shape.Length) && IsPositiveFinite(Shape.Width)) : (Shape.Length != 0.0 || Shape.Width != 0.0))
	{
		Problems.Add(TEXT("length: length and width are above 0 for a rectangle, and 0 otherwise"));
	}
	return Problems;
}

bool Touches(const FVeyraPlacedShape& Placed, const FVector& Center, double BodyRadius)
{
	const FVeyraShape& Shape = Placed.Shape;
	const FVector2D Origin = Flat(Placed.Origin);
	const FVector2D Point = Flat(Center);
	const FVector2D Facing = Flat(Placed.Direction).GetSafeNormal();
	const FVector2D Across(-Facing.Y, Facing.X);
	const FVector2D Offset = Point - Origin;

	switch (Shape.Kind)
	{
	case EVeyraShapeKind::Circle:
		return Offset.Size() <= Shape.Radius + BodyRadius;

	case EVeyraShapeKind::Rectangle:
	{
		// The rectangle's point nearest the body's centre, in the rectangle's own axes.
		const double Along = FVector2D::DotProduct(Offset, Facing);
		const double Side = FVector2D::DotProduct(Offset, Across);
		const FVector2D Nearest(FMath::Clamp(Along, 0.0, Shape.Length), FMath::Clamp(Side, -Shape.Width / 2.0, Shape.Width / 2.0));
		return FVector2D::Distance(FVector2D(Along, Side), Nearest) <= BodyRadius;
	}

	case EVeyraShapeKind::Sector:
	{
		const double Distance = Offset.Size();
		if (Distance > Shape.Radius + BodyRadius)
		{
			return false;
		}
		if (Distance <= BodyRadius || Shape.ArcDegrees >= FullTurnDegrees || Facing.IsNearlyZero())
		{
			return true;
		}
		const double HalfArc = FMath::DegreesToRadians(Shape.ArcDegrees / 2.0);
		const double Angle = FMath::Acos(FMath::Clamp(FVector2D::DotProduct(Offset / Distance, Facing), -1.0, 1.0));
		if (Angle <= HalfArc)
		{
			return true;
		}
		// Outside the arc's angle, a body can still overlap one of its two straight edges.
		for (const double Sign : { -1.0, 1.0 })
		{
			const double EdgeAngle = Sign * HalfArc;
			const FVector2D Edge = Facing * FMath::Cos(EdgeAngle) + Across * FMath::Sin(EdgeAngle);
			if (DistanceToSegment(Point, Origin, Origin + Edge * Shape.Radius) <= BodyRadius)
			{
				return true;
			}
		}
		return false;
	}
	}
	return false;
}

double Reach(const FVeyraShape& Shape)
{
	switch (Shape.Kind)
	{
	case EVeyraShapeKind::Circle:
	case EVeyraShapeKind::Sector:
		return Shape.Radius;
	case EVeyraShapeKind::Rectangle:
		return FMath::Sqrt(FMath::Square(Shape.Length) + FMath::Square(Shape.Width / 2.0));
	}
	return 0.0;
}

TArray<AActor*> GatherUnits(const UWorld& World, const FVeyraPlacedShape& Placed, TFunctionRef<bool(const AActor&)> Include,
	EVeyraStructureTargeting Structures)
{
	TArray<FVeyraPathHit> Hits;
	ForEachUnit(World, Include, Structures, [&Placed, &Hits](AActor& Unit) {
		if (Touches(Placed, Unit.GetActorLocation(), Unit.GetSimpleCollisionRadius()))
		{
			Hits.Add({ &Unit, FVector::Dist2D(Unit.GetActorLocation(), Placed.Origin) });
		}
	});
	SortByDistance(Hits);
	TArray<AActor*> Units;
	for (const FVeyraPathHit& Hit : Hits)
	{
		Units.Add(Hit.Unit);
	}
	return Units;
}

TOptional<double> FirstContactAlong(const FVector& Start, const FVector& End, double Radius, const FVector& Center, double BodyRadius)
{
	const FVector2D From = Flat(Start);
	const FVector2D Path = Flat(End) - From;
	const FVector2D Offset = Flat(Center) - From;
	const double TouchingSquared = FMath::Square(Radius + BodyRadius);
	if (Offset.SizeSquared() <= TouchingSquared)
	{
		return 0.0;
	}
	const double Length = Path.Size();
	if (!(Length > 0.0))
	{
		return {};
	}
	// The circles first touch where the moving centre comes within Radius + BodyRadius of the body's.
	const double Along = FVector2D::DotProduct(Offset, Path / Length);
	const double AcrossSquared = Offset.SizeSquared() - FMath::Square(Along);
	if (AcrossSquared > TouchingSquared)
	{
		return {};
	}
	const double Contact = Along - FMath::Sqrt(FMath::Max(0.0, TouchingSquared - AcrossSquared));
	// They do not touch at Start, so a contact before it means the body lies behind the path.
	if (Contact < 0.0 || Contact > Length)
	{
		return {};
	}
	return Contact;
}

TArray<FVeyraPathHit> GatherUnitsAlong(const UWorld& World, const FVector& Start, const FVector& End, double Radius, TFunctionRef<bool(const AActor&)> Include)
{
	TArray<FVeyraPathHit> Hits;
	ForEachUnit(World, Include, EVeyraStructureTargeting::Refuse, [&](AActor& Unit) {
		if (const TOptional<double> Contact = FirstContactAlong(Start, End, Radius, Unit.GetActorLocation(), Unit.GetSimpleCollisionRadius()))
		{
			Hits.Add({ &Unit, Contact.GetValue() });
		}
	});
	SortByDistance(Hits);
	return Hits;
}
}
