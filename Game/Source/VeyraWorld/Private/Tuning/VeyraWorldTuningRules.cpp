// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "Layout/VeyraLayout.h"
#include "Layout/VeyraRiver.h"
#include "Tuning/VeyraWorldTuning.h"

namespace VeyraWorld
{
namespace
{
	// How finely validation probes geometry: a wall's footprint, in steps along each side, and the river around a Well,
	// in directions and units a step. Resolution of the checks, not tuning.
	constexpr int32 WallProbeSteps = 8;
	constexpr int32 IslandProbeDirections = 24;
	constexpr double IslandProbeStep = 100.0;

	// The most samples a river span may take, as the schema allows.
	constexpr int32 MaxRiverSamplesPerSegment = 64;

	bool IsOnFloor(const FVeyraMapPoint& Point, double HalfExtent)
	{
		return FMath::Abs(Point.X) <= HalfExtent && FMath::Abs(Point.Y) <= HalfExtent;
	}

	double BodyRadius(const FVeyraWorldTuning& Tuning, EVeyraStructureKind Kind)
	{
		switch (Kind)
		{
		case EVeyraStructureKind::LaneSpire:
			return Tuning.Structures.LaneSpire.CapsuleRadius;
		case EVeyraStructureKind::BaseTower:
			return Tuning.Structures.BaseTower.CapsuleRadius;
		case EVeyraStructureKind::Inhibitor:
			return Tuning.Structures.Inhibitor.CapsuleRadius;
		case EVeyraStructureKind::PrimeWell:
			return Tuning.Structures.PrimeWell.CapsuleRadius;
		}
		return 0.0;
	}

	/**
	 * Each of Team A's walls (ADR-043 §1): sized, on the floor, wholly in Team A's half so it never meets
	 * its rotation, WallClearance from everything placed in a straight line and from the river's water. Team B's are the
	 * rotation, so they are too.
	 */
	void ValidateWalls(const FVeyraWorldTuning& Tuning, TArray<FString>& Problems)
	{
		const FVeyraBattlegroundLayout& Layout = Tuning.Layout;
		if (!Layout.Walls.IsEmpty() && Layout.WallHalfHeight <= 0.0)
		{
			Problems.Add(TEXT("/layout/wallHalfHeight: walls need a height"));
		}
		const double Clearance = Layout.WallClearance;
		TArray<TPair<FVector2D, double>> Keeps;
		for (const FVeyraCampTuning& Camp : Tuning.Wildlife.Camps)
		{
			const FVeyraWildlifeSpecies* Species = Tuning.FindSpecies(Camp.Species);
			Keeps.Add({ VeyraLayout::ToVector(Camp.Center), Camp.Spacing + (Species ? Species->CapsuleRadius : 0.0) });
		}
		for (const FVeyraMapPoint& Site : Tuning.FluxWells.Sites)
		{
			Keeps.Add({ VeyraLayout::ToVector(Site), Tuning.FluxWells.Radius });
		}
		for (const FVeyraStructurePlacement& Structure : VeyraLayout::Structures(Layout))
		{
			if (Structure.Team == EVeyraTeam::A)
			{
				Keeps.Add({ Structure.Location, BodyRadius(Tuning, Structure.Kind) });
			}
		}
		Keeps.Add({ VeyraLayout::ToVector(Layout.Base.Fountain), Layout.Base.FountainRadius });
		for (const FVeyraFogLayout& Fog : Layout.DenseFog)
		{
			Keeps.Add({ VeyraLayout::ToVector(Fog.Center), Fog.Radius });
		}

		for (int32 Index = 0; Index < Layout.Walls.Num(); ++Index)
		{
			const FVeyraWallLayout& Entry = Layout.Walls[Index];
			const FString Pointer = FString::Printf(TEXT("/layout/walls/%d"), Index);
			if (Entry.Length <= 0.0 || Entry.Thickness <= 0.0)
			{
				Problems.Add(Pointer + TEXT(": a wall needs a length and a thickness"));
				continue;
			}
			const FVeyraTerrainBox Box = VeyraLayout::Wall(Entry, EVeyraTeam::A);
			bool bOnFloor = true;
			bool bInTeamAsHalf = true;
			for (const FVector2D& Corner : Box.Corners())
			{
				bOnFloor &= FMath::Abs(Corner.X) <= Layout.HalfExtent && FMath::Abs(Corner.Y) <= Layout.HalfExtent;
				bInTeamAsHalf &= VeyraLayout::DepthInTeamAHalf(Layout, Corner) > 0.0;
			}
			if (!bOnFloor)
			{
				Problems.Add(Pointer + TEXT(": the wall must lie on the floor"));
			}
			if (!bInTeamAsHalf)
			{
				Problems.Add(Pointer + TEXT(": the wall must lie wholly in Team A's half; Team B's is its rotation"));
			}
			for (const FVeyraLaneLayout& Lane : Layout.Lanes)
			{
				for (int32 Point = 1; Point < Lane.Points.Num(); ++Point)
				{
					const double Apart = Box.DistanceToSegment(VeyraLayout::ToVector(Lane.Points[Point - 1]), VeyraLayout::ToVector(Lane.Points[Point]));
					if (Apart < Lane.Width / 2.0 + Clearance)
					{
						Problems.Add(Pointer + FString::Printf(TEXT(": the wall must keep wallClearance from the %s lane's road"), *UEnum::GetValueAsString(Lane.Lane)));
						break;
					}
				}
			}
			for (const TPair<FVector2D, double>& Keep : Keeps)
			{
				if (Box.DistanceTo(Keep.Key) < Keep.Value + Clearance)
				{
					Problems.Add(Pointer + FString::Printf(TEXT(": the wall must keep wallClearance from what stands at (%.0f, %.0f): a camp, a Flux Well, a structure, the fountain or Dense Fog"),
						Keep.Key.X, Keep.Key.Y));
				}
			}
			// The terrain field raises a ridge on every wall, which must stand on dry ground.
			const FVeyraRiverShape& River = VeyraRiver::ShapeOf(Layout);
			const FVector2D Across(-Box.Facing.Y, Box.Facing.X);
			bool bDry = true;
			for (int32 Along = 0; Along <= WallProbeSteps && bDry; ++Along)
			{
				for (int32 Deep = 0; Deep <= WallProbeSteps && bDry; ++Deep)
				{
					const double U = (static_cast<double>(Along) / WallProbeSteps - 0.5) * Box.Length;
					const double V = (static_cast<double>(Deep) / WallProbeSteps - 0.5) * Box.Thickness;
					bDry = River.SignedDistance(Box.Centre + Across * U + Box.Facing * V) >= Clearance;
				}
			}
			if (!bDry)
			{
				Problems.Add(Pointer + TEXT(": the wall must keep wallClearance from the river's water"));
			}
		}
	}

	/** The ground's levels (ADR-040 §3): finite, inside the surface search, in order, and their nominal slopes walkable. */
	void ValidateTerrain(const FVeyraBattlegroundLayout& Layout, TArray<FString>& Problems)
	{
		const FVeyraTerrainTuning& Terrain = Layout.Terrain;
		const double Heights[] = { Terrain.LaneZ, Terrain.BaseZ, Terrain.JungleZ, Terrain.IslandZ, Terrain.RidgeZ, Terrain.BoundaryZ };
		const double Runs[] = { Terrain.JungleRise, Terrain.BankWidth, Terrain.RidgeSkirt, Terrain.BoundaryWidth };
		for (const double Height : Heights)
		{
			if (!FMath::IsFinite(Height) || Height <= Layout.Surface.MinZ || Height >= Layout.Surface.MaxZ)
			{
				Problems.Add(TEXT("/layout/terrain: every level must be finite and inside the surface search's bounds"));
				break;
			}
		}
		for (const double Run : Runs)
		{
			if (!FMath::IsFinite(Run) || Run <= 0.0)
			{
				Problems.Add(TEXT("/layout/terrain: every run must be finite and above 0"));
				break;
			}
		}
		if (!FMath::IsFinite(Terrain.LaneShoulder) || Terrain.LaneShoulder < 0.0 || !FMath::IsFinite(Terrain.WallFootingClearance) || Terrain.WallFootingClearance < 0.0)
		{
			Problems.Add(TEXT("/layout/terrain: laneShoulder and wallFootingClearance must be finite and at least 0"));
		}
		if (Terrain.RidgeZ <= Terrain.JungleZ || Terrain.BoundaryZ <= Terrain.JungleZ || Terrain.IslandZ <= Layout.River.SurfaceZ)
		{
			Problems.Add(TEXT("/layout/terrain: ridges and the rim must rise above the jungle, and islands above the water"));
		}
		// The walkable climbs, at their nominal grades: road to jungle, road to base, water's edge to the highest bank.
		const auto Steepest = [&Layout](double Rise, double Run) { return Run > 0.0 && FMath::RadiansToDegrees(FMath::Atan(FMath::Abs(Rise) / Run)) >= Layout.Surface.MaxSlopeDegrees; };
		if (Steepest(Terrain.JungleZ - Terrain.LaneZ, Terrain.JungleRise) || Steepest(Terrain.BaseZ - Terrain.LaneZ, Terrain.JungleRise)
			|| Steepest(FMath::Max(Terrain.JungleZ, Terrain.LaneZ) - Layout.River.BedZ, Terrain.BankWidth))
		{
			Problems.Add(TEXT("/layout/terrain: a walkable climb (road to jungle or base, or a bank) is steeper than surface.maxSlopeDegrees"));
		}
	}

	/**
	 * The river (ADR-040 §6; author ruling 2026-10-05): its main channel from the centre off the floor, each island's
	 * channel leaving and rejoining it, every Flux Well dry on an island, and the sites each other's rotation.
	 */
	void ValidateRiver(const FVeyraWorldTuning& Tuning, TArray<FString>& Problems)
	{
		const FVeyraBattlegroundLayout& Layout = Tuning.Layout;
		const FVeyraRiverLayout& River = Layout.River;
		if (River.SamplesPerSegment < 1 || River.SamplesPerSegment > MaxRiverSamplesPerSegment)
		{
			Problems.Add(FString::Printf(TEXT("/layout/river/samplesPerSegment: must be 1 to %d"), MaxRiverSamplesPerSegment));
		}
		if (!FMath::IsFinite(River.SurfaceZ) || !FMath::IsFinite(River.BedZ) || River.BedZ >= River.SurfaceZ || River.BedZ <= Layout.Surface.MinZ
			|| River.SurfaceZ >= Layout.Terrain.LaneZ)
		{
			Problems.Add(TEXT("/layout/river: the bed must lie below the water's surface, inside the surface search, and the water below the lanes"));
		}
		if (!FMath::IsFinite(River.FlowSpeed) || River.FlowSpeed < 0.0)
		{
			Problems.Add(TEXT("/layout/river/flowSpeed: must be finite and at least 0"));
		}
		const auto ValidControls = [](TConstArrayView<FVeyraRiverPoint> Controls) {
			return Controls.Num() >= 2 && Algo::AllOf(Controls, [](const FVeyraRiverPoint& Control) {
				return FMath::IsFinite(Control.X) && FMath::IsFinite(Control.Y) && FMath::IsFinite(Control.Width) && Control.Width > 0.0;
			});
		};
		if (!ValidControls(River.Main))
		{
			Problems.Add(TEXT("/layout/river/main: needs at least two controls, finite, with widths above 0"));
			return;
		}
		if (River.Main[0].X != 0.0 || River.Main[0].Y != 0.0)
		{
			Problems.Add(TEXT("/layout/river/main/0: must be the centre, where Team A's half of the river meets its rotation"));
		}
		const FVeyraRiverPoint& Last = River.Main.Last();
		if (FMath::Max(FMath::Abs(Last.X), FMath::Abs(Last.Y)) <= Layout.HalfExtent)
		{
			Problems.Add(TEXT("/layout/river/main: must run off the floor"));
		}
		const FVeyraRiverShape& Shape = VeyraRiver::ShapeOf(Layout);
		for (int32 Index = 0; Index < River.Islands.Num(); ++Index)
		{
			const FVeyraRiverIsland& Island = River.Islands[Index];
			const FString Pointer = FString::Printf(TEXT("/layout/river/islands/%d"), Index);
			if (!Tuning.FluxWells.Sites.IsValidIndex(Island.Site))
			{
				Problems.Add(Pointer + TEXT("/site: must name a Flux Well site"));
			}
			if (!ValidControls(Island.Channel))
			{
				Problems.Add(Pointer + TEXT("/channel: needs at least two controls, finite, with widths above 0"));
				continue;
			}
			const FVeyraRiverChannel& Main = Shape.GetChannels()[0];
			for (const FVeyraRiverPoint& End : { Island.Channel[0], Island.Channel.Last() })
			{
				if (FVeyraRiverShape::SignedDistance(Main, FVector2D(End.X, End.Y)) >= 0.0)
				{
					Problems.Add(Pointer + TEXT("/channel: must leave and rejoin the main channel, its ends in its water"));
					break;
				}
			}
		}
		const FVeyraFluxWellsTuning& Wells = Tuning.FluxWells;
		for (int32 Index = 0; Index < Wells.Sites.Num(); ++Index)
		{
			const FString Pointer = FString::Printf(TEXT("/fluxWells/sites/%d"), Index);
			const FVector2D Site = VeyraLayout::ToVector(Wells.Sites[Index]);
			if (Shape.SignedDistance(Site) < Wells.CapsuleRadius)
			{
				Problems.Add(Pointer + TEXT(": the Well's body must stand on dry ground"));
			}
			else if (!Shape.IsOnIsland(Site, Layout.HalfExtent * 2.0, IslandProbeDirections, IslandProbeStep))
			{
				Problems.Add(Pointer + TEXT(": the Well must stand on an island, the river closing around it"));
			}
			const bool bRotatesOntoASite = Algo::AnyOf(Wells.Sites, [&Site](const FVeyraMapPoint& Other) {
				return VeyraLayout::Rotate(Site).Equals(VeyraLayout::ToVector(Other), UE_DOUBLE_KINDA_SMALL_NUMBER);
			});
			if (!bRotatesOntoASite)
			{
				Problems.Add(Pointer + TEXT(": its rotation must be a site too, so both teams have the same Wells"));
			}
		}
	}
}

TArray<FString> Validate(const FVeyraWorldTuning& Tuning)
{
	TArray<FString> Problems;
	const FVeyraBattlegroundLayout& Layout = Tuning.Layout;
	if (!FMath::IsFinite(Layout.Surface.MinZ) || !FMath::IsFinite(Layout.Surface.MaxZ)
		|| Layout.Surface.MinZ >= Layout.Surface.MaxZ || !FMath::IsFinite(Layout.Surface.MaxSlopeDegrees)
		|| Layout.Surface.MaxSlopeDegrees < 0.0 || Layout.Surface.MaxSlopeDegrees >= 90.0)
	{
		Problems.Add(TEXT("/layout/surface: requires finite ascending height bounds and a slope in [0, 90) degrees"));
	}


	ValidateTerrain(Layout, Problems);
	ValidateRiver(Tuning, Problems);
	// Canon's three Fluxways, each once (Battleground Bible §2).
	for (const EVeyraLane Lane : { EVeyraLane::Top, EVeyraLane::Mid, EVeyraLane::Bottom })
	{
		const int32 Count = Layout.Lanes.FilterByPredicate([Lane](const FVeyraLaneLayout& Entry) { return Entry.Lane == Lane; }).Num();
		if (Count != 1)
		{
			Problems.Add(FString::Printf(TEXT("/layout/lanes: needs exactly one %s lane, not %d"), *UEnum::GetValueAsString(Lane), Count));
		}
	}

	for (int32 Index = 0; Index < Layout.Lanes.Num(); ++Index)
	{
		const FVeyraLaneLayout& Lane = Layout.Lanes[Index];
		const FString Pointer = FString::Printf(TEXT("/layout/lanes/%d"), Index);
		if (Lane.Points.Num() < 2)
		{
			Problems.Add(Pointer + TEXT("/points: a lane needs at least two points"));
			continue;
		}
		for (const FVeyraMapPoint& Point : Lane.Points)
		{
			if (!IsOnFloor(Point, Layout.HalfExtent))
			{
				Problems.Add(Pointer + TEXT("/points: every point must lie on the floor"));
				break;
			}
		}
		if (!VeyraLayout::RotatesOntoALane(Lane, Layout.Lanes))
		{
			Problems.Add(Pointer + TEXT("/points: the lane, turned half a turn about the centre and reversed, must be one of the lanes, so both teams walk the same distances"));
		}
		double Previous = 0.0;
		for (const double Distance : Lane.SpireDistances)
		{
			if (Distance <= Previous)
			{
				Problems.Add(Pointer + TEXT("/spireDistances: must be above 0 and strictly rising"));
				break;
			}
			Previous = Distance;
		}
		if (Lane.FluxbornSpawnDistance >= VeyraLayout::Length(Lane.Points) / 2.0)
		{
			Problems.Add(Pointer + TEXT("/fluxbornSpawnDistance: Team A's Fluxborn must spawn on its half of the lane"));
		}
		// The widest Fluxborn must fit between the lane's structures where they spawn, or it starts inside one.
		double WidestFluxborn = 0.0;
		for (const TPair<FVeyraContentId, FVeyraFluxbornDefinition>& Unit : Tuning.Fluxborn.Units)
		{
			WidestFluxborn = FMath::Max(WidestFluxborn, Unit.Value.CapsuleRadius);
		}
		const bool bClearOfInhibitor = FMath::Abs(Lane.FluxbornSpawnDistance - Lane.InhibitorDistance) > Tuning.Structures.Inhibitor.CapsuleRadius + WidestFluxborn;
		bool bClearOfSpires = true;
		for (const double Distance : Lane.SpireDistances)
		{
			bClearOfSpires &= FMath::Abs(Lane.FluxbornSpawnDistance - (Lane.InhibitorDistance + Distance)) > Tuning.Structures.LaneSpire.CapsuleRadius + WidestFluxborn;
		}
		if (!bClearOfInhibitor || !bClearOfSpires)
		{
			Problems.Add(Pointer + TEXT("/fluxbornSpawnDistance: Fluxborn would spawn inside the lane's inhibitor or a Spire"));
		}
		const double Outermost = Lane.InhibitorDistance + (Lane.SpireDistances.IsEmpty() ? 0.0 : Lane.SpireDistances.Last());
		if (Outermost >= VeyraLayout::Length(Lane.Points) / 2.0)
		{
			Problems.Add(Pointer + TEXT("/spireDistances: Team A's structures must stay on its half of the lane"));
		}
	}

	const FVeyraBaseLayout& Base = Layout.Base;
	TArray<FVeyraMapPoint> BasePoints = Base.BaseTowers;
	BasePoints.Add(Base.PrimeWell);
	BasePoints.Add(Base.Fountain);
	for (const FVeyraMapPoint& Point : BasePoints)
	{
		if (!IsOnFloor(Point, Layout.HalfExtent))
		{
			Problems.Add(TEXT("/layout/base: every point must lie on the floor"));
			break;
		}
	}
	if (Base.BaseTowers.IsEmpty())
	{
		Problems.Add(TEXT("/layout/base/baseTowers: the Prime Well needs its base-defense towers (Battleground Bible §18)"));
	}
	// Each fog circle lies wholly on the floor and in Team A's half, so it and its rotation are apart: a
	// circle touching the dividing line could touch its rotation there, and touching circles are one
	// volume (ADR-016 §4).
	for (int32 Index = 0; Index < Layout.DenseFog.Num(); ++Index)
	{
		const FVeyraFogLayout& Circle = Layout.DenseFog[Index];
		const FVector2D Center = VeyraLayout::ToVector(Circle.Center);
		const FString Pointer = FString::Printf(TEXT("/layout/denseFog/%d"), Index);
		if (FMath::Abs(Center.X) + Circle.Radius > Layout.HalfExtent || FMath::Abs(Center.Y) + Circle.Radius > Layout.HalfExtent)
		{
			Problems.Add(Pointer + TEXT(": the fog must lie on the floor"));
		}
		if (VeyraLayout::DepthInTeamAHalf(Layout, Center) <= Circle.Radius)
		{
			Problems.Add(Pointer + TEXT(": the fog must lie wholly in Team A's half, clear of the dividing line; Team B's is its rotation"));
		}
	}

	// Every Fluxborn attacks with the profile Vanguards use, so the same rules check it.
	if (Tuning.Fluxborn.Units.IsEmpty())
	{
		Problems.Add(TEXT("/fluxborn/units: needs at least one kind of Fluxborn"));
	}
	for (const TPair<FVeyraContentId, FVeyraFluxbornDefinition>& Unit : Tuning.Fluxborn.Units)
	{
		const FString Pointer = TEXT("/fluxborn/units/") + Unit.Key.ToString();
		for (const FString& AttackProblem : VeyraBasicAttacks::Validate(Unit.Value.BasicAttack))
		{
			Problems.Add(Pointer + TEXT("/basicAttack: ") + AttackProblem);
		}
		if (Unit.Value.CapsuleHalfHeight < Unit.Value.CapsuleRadius)
		{
			Problems.Add(Pointer + TEXT("/capsuleHalfHeight: must be at least capsuleRadius"));
		}
	}

	// Waves: phases in order from 0, and every unit a kind of Fluxborn (Battleground Bible §17).
	const FVeyraWavesTuning& Waves = Tuning.Waves;
	if (Waves.Phases.IsEmpty() || Waves.Phases[0].FromSeconds != 0.0)
	{
		Problems.Add(TEXT("/waves/phases: the first phase must begin at 0"));
	}
	for (int32 Index = 1; Index < Waves.Phases.Num(); ++Index)
	{
		if (Waves.Phases[Index].FromSeconds <= Waves.Phases[Index - 1].FromSeconds)
		{
			Problems.Add(FString::Printf(TEXT("/waves/phases/%d/fromSeconds: phases must begin in order"), Index));
		}
	}
	const auto CheckUnits = [&Problems, &Tuning](const TArray<FVeyraWaveUnitTuning>& Units, const TCHAR* Field) {
		for (int32 Index = 0; Index < Units.Num(); ++Index)
		{
			if (!Tuning.FindFluxborn(Units[Index].Unit))
			{
				Problems.Add(FString::Printf(TEXT("/waves/%s/%d/unit: %s is no kind of Fluxborn"), Field, Index, *Units[Index].Unit.ToString()));
			}
		}
	};
	CheckUnits(Waves.Units, TEXT("units"));
	CheckUnits(Waves.SiegeUnits, TEXT("siegeUnits"));
	CheckUnits(Waves.InhibitorDownUnits, TEXT("inhibitorDownUnits"));

	// Wildlife (Battleground Bible §7, §8, §17; ADR-014 §2).
	const FVeyraWildlifeTuning& Wildlife = Tuning.Wildlife;
	for (const TPair<FVeyraContentId, FVeyraWildlifeSpecies>& Species : Wildlife.Species)
	{
		const FString Pointer = TEXT("/wildlife/species/") + Species.Key.ToString();
		for (const FString& AttackProblem : VeyraBasicAttacks::Validate(Species.Value.BasicAttack))
		{
			Problems.Add(Pointer + TEXT("/basicAttack: ") + AttackProblem);
		}
		if (Species.Value.CapsuleHalfHeight < Species.Value.CapsuleRadius)
		{
			Problems.Add(Pointer + TEXT("/capsuleHalfHeight: must be at least capsuleRadius"));
		}
	}
	// Each of Team A's camps, its leash and all, on the floor, on Team A's half clear of the river's water, and
	// clear of every lane; Team B's are their rotation, so they are too.
	for (int32 Index = 0; Index < Wildlife.Camps.Num(); ++Index)
	{
		const FVeyraCampTuning& Camp = Wildlife.Camps[Index];
		const FString Pointer = FString::Printf(TEXT("/wildlife/camps/%d"), Index);
		if (!Tuning.FindSpecies(Camp.Species))
		{
			Problems.Add(Pointer + FString::Printf(TEXT("/species: %s is no species of wildlife"), *Camp.Species.ToString()));
		}
		const FVector2D Center = VeyraLayout::ToVector(Camp.Center);
		const double Reach = Camp.LeashRadius;
		if (FMath::Abs(Center.X) + Reach > Layout.HalfExtent || FMath::Abs(Center.Y) + Reach > Layout.HalfExtent)
		{
			Problems.Add(Pointer + TEXT("/center: the camp's leash must lie on the floor"));
		}
		if (VeyraLayout::DepthInTeamAHalf(Layout, Center) < Reach || VeyraRiver::ShapeOf(Layout).SignedDistance(Center) < Reach)
		{
			Problems.Add(Pointer + TEXT("/center: the camp must lie on Team A's half, its leash clear of the river's water"));
		}
		for (const FVeyraLaneLayout& Lane : Layout.Lanes)
		{
			if (VeyraLayout::DistanceToPath(Lane.Points, Center) < Lane.Width / 2.0 + Reach)
			{
				Problems.Add(Pointer + FString::Printf(TEXT("/center: the camp's leash must stay clear of the %s lane"), *UEnum::GetValueAsString(Lane.Lane)));
			}
		}
		if (Camp.Count > 1 && Camp.Spacing >= Reach)
		{
			Problems.Add(Pointer + TEXT("/spacing: the creatures must stand inside the camp's leash"));
		}
	}

	// The Flux Wells stand clear of every lane (§6; ADR-014 §4); ValidateRiver puts each on its island.
	const FVeyraFluxWellsTuning& Wells = Tuning.FluxWells;
	for (int32 Index = 0; Index < Wells.Sites.Num(); ++Index)
	{
		const FString Pointer = FString::Printf(TEXT("/fluxWells/sites/%d"), Index);
		const FVector2D Site = VeyraLayout::ToVector(Wells.Sites[Index]);
		if (!IsOnFloor(Wells.Sites[Index], Layout.HalfExtent))
		{
			Problems.Add(Pointer + TEXT(": must lie on the floor"));
		}
		for (const FVeyraLaneLayout& Lane : Layout.Lanes)
		{
			if (VeyraLayout::DistanceToPath(Lane.Points, Site) < Lane.Width / 2.0 + Wells.Radius)
			{
				Problems.Add(Pointer + FString::Printf(TEXT(": the Well's radius must stay clear of the %s lane"), *UEnum::GetValueAsString(Lane.Lane)));
			}
		}
	}
	if (Wells.CapsuleRadius >= Wells.Radius)
	{
		Problems.Add(TEXT("/fluxWells/radius: must reach beyond the Well's body"));
	}

	ValidateWalls(Tuning, Problems);
	return Problems;
}
}
