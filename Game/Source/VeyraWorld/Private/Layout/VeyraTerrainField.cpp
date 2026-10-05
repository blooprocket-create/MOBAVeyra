// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Layout/VeyraTerrainField.h"

#include "Layout/VeyraLayout.h"
#include "Tuning/VeyraWorldTuning.h"

namespace
{
	/** A cubic ease from 0 to 1 as T runs from 0 to 1: the shape of every transition, not a tuning curve. */
	double Smooth(double T)
	{
		const double Clamped = FMath::Clamp(T, 0.0, 1.0);
		return Clamped * Clamped * (3.0 - 2.0 * Clamped);
	}

	/** Under the water, the bed rises to the waterline over this share of a bank's run: the shore's underwater shelf. */
	constexpr double UnderwaterShelfShare = 0.25;

	/** How much of a camp's leash radius stays level as its clearing, and how far beyond it the ground returns. */
	constexpr double ClearingCoreShare = 0.5;
	constexpr double ClearingFadeShare = 0.8;

	/** A ridge's crest swells over a shorter span than the jungle does. */
	constexpr double CrestWavelengthShare = 0.35;

	/** Offsets the relief's noise by its seed: any fixed irrational-looking step keeps seeds apart. */
	const FVector2D SeedStep(17.31, -9.77);

	/** How deep inside Box Point lies, from its nearest edge: negative outside it. */
	double DepthInside(const FVeyraTerrainBox& Box, const FVector2D& Point)
	{
		const FVector2D Local = Point - Box.Centre;
		const FVector2D Across(-Box.Facing.Y, Box.Facing.X);
		const double Inside = FMath::Min(Box.Thickness / 2.0 - FMath::Abs(FVector2D::DotProduct(Local, Box.Facing)),
			Box.Length / 2.0 - FMath::Abs(FVector2D::DotProduct(Local, Across)));
		return Inside > 0.0 ? Inside : -Box.DistanceTo(Point);
	}
}

FVeyraTerrainField::FVeyraTerrainField(const FVeyraWorldTuning& InTuning, const FVeyraTerrainRelief& InRelief)
	: Tuning(InTuning)
	, Relief(InRelief)
	, River(VeyraRiver::ShapeOf(InTuning.Layout))
	, Walls(VeyraLayout::Walls(InTuning.Layout))
{
	const FVeyraBattlegroundLayout& Layout = Tuning.Layout;
	for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
	{
		Pads.Add(VeyraLayout::ForTeam(VeyraLayout::ToVector(Layout.Base.PrimeWell), Team));
		for (const FVeyraCampTuning& Camp : Tuning.Wildlife.Camps)
		{
			Clearings.Add({ VeyraLayout::ForTeam(VeyraLayout::ToVector(Camp.Center), Team), Camp.LeashRadius });
		}
	}
	for (const FVeyraMapPoint& Site : Tuning.FluxWells.Sites)
	{
		Wells.Add(VeyraLayout::ToVector(Site));
	}
}

double FVeyraTerrainField::ReliefAt(const FVector2D& Point, double Wavelength, double Amplitude) const
{
	if (Amplitude <= 0.0 || Wavelength <= 0.0)
	{
		return 0.0;
	}
	const FVector2D Offset = SeedStep * Relief.Seed;
	// Averaged with its rotation, so both teams' ground is the same.
	const double Here = FMath::PerlinNoise2D(Point / Wavelength + Offset);
	const double There = FMath::PerlinNoise2D(VeyraLayout::Rotate(Point) / Wavelength + Offset);
	return Amplitude * 0.5 * (Here + There);
}

double FVeyraTerrainField::Ground(const FVector2D& Point, FVeyraTerrainSample& Out) const
{
	const FVeyraBattlegroundLayout& Layout = Tuning.Layout;
	const FVeyraTerrainTuning& Terrain = Layout.Terrain;

	// The jungle's shelves, swelling gently, level in the camps' clearings.
	double Swell = 1.0;
	for (const TPair<FVector2D, double>& Clearing : Clearings)
	{
		const double Beyond = FVector2D::Distance(Point, Clearing.Key) - Clearing.Value * ClearingCoreShare;
		Swell = FMath::Min(Swell, Smooth(Beyond / (Clearing.Value * ClearingFadeShare)));
	}
	const double Jungle = Terrain.JungleZ + Swell * ReliefAt(Point, Relief.Wavelength, Relief.Amplitude);

	// Each lane's road, level to its shoulder, the ground climbing from it to the jungle.
	double FromRoad = TNumericLimits<double>::Max();
	for (const FVeyraLaneLayout& Lane : Layout.Lanes)
	{
		FromRoad = FMath::Min(FromRoad, VeyraLayout::DistanceToPath(Lane.Points, Point) - Lane.Width / 2.0);
	}
	Out.Road = 1.0 - Smooth(FromRoad / FMath::Max(Terrain.LaneShoulder, UE_DOUBLE_KINDA_SMALL_NUMBER));
	double Height = FMath::Lerp(Terrain.LaneZ, Jungle, Smooth((FromRoad - Terrain.LaneShoulder) / Terrain.JungleRise));

	// Each base's pad, raised, the lanes climbing into it.
	double FromPad = TNumericLimits<double>::Max();
	for (const FVector2D& Pad : Pads)
	{
		FromPad = FMath::Min(FromPad, FVector2D::Distance(Point, Pad) - Layout.Base.PadRadius);
	}
	Out.Pad = 1.0 - Smooth(FromPad / Terrain.JungleRise);
	Height = FMath::Lerp(Terrain.BaseZ, Height, Smooth(FromPad / Terrain.JungleRise));

	// Each Flux Well's island, a low platform around the Well.
	double FromWell = TNumericLimits<double>::Max();
	for (const FVector2D& Well : Wells)
	{
		FromWell = FMath::Min(FromWell, FVector2D::Distance(Point, Well) - Tuning.FluxWells.Radius);
	}
	Height = FMath::Lerp(Terrain.IslandZ, Height, Smooth(FromWell / Terrain.JungleRise));
	return Height;
}

FVeyraTerrainSample FVeyraTerrainField::Sample(const FVector2D& Point) const
{
	const FVeyraBattlegroundLayout& Layout = Tuning.Layout;
	const FVeyraTerrainTuning& Terrain = Layout.Terrain;
	const FVeyraRiverLayout& Water = Layout.River;
	FVeyraTerrainSample Out;
	double Height = Ground(Point, Out);

	// The river: its bed under the water, rising to the waterline at the shore, and its banks climbing to the ground.
	Out.WaterDistance = River.SignedDistance(Point);
	if (Out.WaterDistance < 0.0)
	{
		const double Shelf = Terrain.BankWidth * UnderwaterShelfShare;
		Height = FMath::Lerp(Water.BedZ, Water.SurfaceZ, Smooth((Out.WaterDistance + Shelf) / Shelf));
	}
	else
	{
		Height = FMath::Lerp(Water.SurfaceZ, Height, Smooth(Out.WaterDistance / Terrain.BankWidth));
	}

	// A ridge on every wall, raised after the river cut its banks so a wall by the water keeps its cliff. Its cliff
	// rises within the wall's footprint, so all of it stands inside the wall's collision and the ground around a wall
	// stays as walkable as it was.
	double Inside = -TNumericLimits<double>::Max();
	for (const FVeyraTerrainBox& Wall : Walls)
	{
		Inside = FMath::Max(Inside, DepthInside(Wall, Point));
	}
	Out.Ridge = Smooth(Inside / Terrain.RidgeSkirt);
	if (Out.Ridge > 0.0)
	{
		const double Crest = Terrain.RidgeZ + ReliefAt(Point, Relief.Wavelength * CrestWavelengthShare, Relief.CrestAmplitude);
		Height = FMath::Lerp(Height, Crest, Out.Ridge);
	}

	// The rim beyond the floor's edge, open where the river runs through it.
	const double Beyond = FMath::Max(FMath::Abs(Point.X), FMath::Abs(Point.Y)) - Layout.HalfExtent;
	if (Beyond > 0.0)
	{
		const double Gorge = Smooth((Out.WaterDistance - Terrain.BankWidth) / Terrain.BankWidth);
		Out.Rim = Smooth(Beyond / Terrain.BoundaryWidth) * Gorge;
		Height = FMath::Lerp(Height, Terrain.BoundaryZ, Out.Rim);
	}
	Out.Height = Height;
	return Out;
}
