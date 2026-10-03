// Copyright © 2026 Wayfinder Studios. All rights reserved.
#include "Layout/VeyraTerrainProfile.h"
#include "Layout/VeyraLayout.h"
#include "Tuning/VeyraWorldTuning.h"

namespace VeyraTerrainProfile
{
namespace
{
	double Blend(double Distance, double Width)
	{
		const double T = FMath::Clamp(Distance / Width, 0.0, 1.0);
		return T * T * (3.0 - 2.0 * T); // Cubic smoothstep, not a tuning curve.
	}
}

TArray<FVeyraRiverSample> River(const FVeyraTerrainTuning& Terrain, bool bMirror)
{
	TArray<FVeyraRiverSample> Result;
	const auto& Controls = Terrain.RiverControls;
	if (Controls.Num() < 2 || Terrain.RiverSamplesPerSegment < 1) { return Result; }
	auto Point = [&Controls](int32 Index) {
		const auto& P = Controls[FMath::Clamp(Index, 0, Controls.Num() - 1)];
		return FVector2D(P.X, P.Y);
	};
	for (int32 Index = 0; Index < Controls.Num() - 1; ++Index)
	{
		for (int32 Step = 0; Step < Terrain.RiverSamplesPerSegment; ++Step)
		{
			const double T = static_cast<double>(Step) / Terrain.RiverSamplesPerSegment;
			// Catmull-Rom tangents. Both rendering and gameplay consume these same samples.
			FVector2D P = FMath::CubicInterp(Point(Index), (Point(Index + 1) - Point(Index - 1)) / 2.0,
				Point(Index + 1), (Point(Index + 2) - Point(Index)) / 2.0, T);
			if (bMirror) { P = VeyraLayout::Mirror(P); }
			Result.Add({P, FMath::Lerp(Controls[Index].Width, Controls[Index + 1].Width, T)});
		}
	}
	Result.Add({ bMirror ? VeyraLayout::Mirror(Point(Controls.Num() - 1)) : Point(Controls.Num() - 1), Controls.Last().Width });
	return Result;
}

double DistanceToBranch(TConstArrayView<FVeyraRiverSample> Samples, const FVector2D& Point)
{
	double Nearest = TNumericLimits<double>::Max();
	for (int32 I = 1; I < Samples.Num(); ++I)
	{
		const auto& A = Samples[I - 1];
		const auto& B = Samples[I];
		const FVector2D Delta = B.Point - A.Point;
		const double T = Delta.IsNearlyZero() ? 0.0 : FMath::Clamp(FVector2D::DotProduct(Point - A.Point, Delta) / Delta.SizeSquared(), 0.0, 1.0);
		Nearest = FMath::Min(Nearest, FVector2D::Distance(Point, A.Point + Delta * T) - FMath::Lerp(A.Width, B.Width, T) / 2.0);
	}
	return Nearest;
}

double RiverDistance(const FVeyraTerrainTuning& Terrain, const FVector2D& Point)
{
	return FMath::Min(DistanceToBranch(River(Terrain, false), Point), DistanceToBranch(River(Terrain, true), Point));
}

double Height(const FVeyraWorldTuning& Tuning, const FVector2D& Point)
{
	return FVeyraTerrainSampler(Tuning).Height(Point);
}

bool AllowsDressing(const FVeyraWorldTuning& Tuning, const FVector2D& Point, double Radius)
{
	const auto& Layout = Tuning.Layout;
	for (const auto& Lane : Layout.Lanes)
	{
		if (VeyraLayout::DistanceToPath(Lane.Points, Point) < Lane.Width / 2.0 + Layout.WallClearance + Radius) { return false; }
	}
	for (const auto& Fog : VeyraLayout::DenseFog(Layout))
	{
		if (FVector2D::Distance(Point, Fog.Center) < Fog.Radius + Radius) { return false; }
	}
	for (const auto& Structure : VeyraLayout::Structures(Layout))
	{
		if (FVector2D::Distance(Point, Structure.Location) < Tuning.Structures.PrimeWell.CapsuleRadius + Layout.WallClearance + Radius) { return false; }
	}
	for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
	{
		if (FVector2D::Distance(Point, VeyraLayout::Fountain(Layout, Team)) < Layout.Base.FountainRadius + Radius) { return false; }
		for (const auto& Camp : Tuning.Wildlife.Camps)
		{
			if (FVector2D::Distance(Point, VeyraLayout::ForTeam(VeyraLayout::ToVector(Camp.Center), Team)) < Camp.LeashRadius + Radius) { return false; }
		}
	}
	for (const auto& Site : Tuning.FluxWells.Sites)
	{
		if (FVector2D::Distance(Point, VeyraLayout::ToVector(Site)) < Tuning.FluxWells.Radius + Layout.WallClearance + Radius) { return false; }
	}
	return true;
}
}

FVeyraTerrainSampler::FVeyraTerrainSampler(const FVeyraWorldTuning& InTuning)
	: Tuning(InTuning), BranchA(VeyraTerrainProfile::River(Tuning.Layout.Terrain, false)), BranchB(VeyraTerrainProfile::River(Tuning.Layout.Terrain, true)) {}

double FVeyraTerrainSampler::RiverDistance(const FVector2D& Point) const
{
	return FMath::Min(VeyraTerrainProfile::DistanceToBranch(BranchA, Point), VeyraTerrainProfile::DistanceToBranch(BranchB, Point));
}

double FVeyraTerrainSampler::Height(const FVector2D& Point) const
{
	const auto& Layout = Tuning.Layout;
	const auto& Terrain = Layout.Terrain;
	const double Bank = RiverDistance(Point);
	double Z = FMath::Lerp(Terrain.RiverBedZ, Terrain.JungleZ, VeyraTerrainProfile::Blend(Bank, Terrain.BankBlend));
	for (const auto& Lane : Layout.Lanes)
	{
		const double Distance = VeyraLayout::DistanceToPath(Lane.Points, Point) - Lane.Width / 2.0;
		const double LaneHeight = FMath::Lerp(Terrain.RiverBedZ, Terrain.LaneZ, VeyraTerrainProfile::Blend(Bank, Terrain.BankBlend));
		Z = FMath::Lerp(LaneHeight, Z, VeyraTerrainProfile::Blend(Distance, Terrain.LaneShoulder));
	}
	for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
	{
		const double BaseDistance = FVector2D::Distance(Point, VeyraLayout::ForTeam(VeyraLayout::ToVector(Layout.Base.PrimeWell), Team)) - Layout.Base.PadRadius;
		Z = FMath::Lerp(Terrain.BaseZ, Z, VeyraTerrainProfile::Blend(BaseDistance, Terrain.LaneShoulder));
	}
	const double Outside = FMath::Max(FMath::Abs(Point.X), FMath::Abs(Point.Y)) - Layout.HalfExtent;
	return FMath::Lerp(Z, Terrain.ExteriorZ, VeyraTerrainProfile::Blend(Outside, Terrain.ExteriorWidth));
}

