// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Layout/VeyraLayout.h"
#include "Tuning/VeyraWorldTuning.h"

namespace VeyraWorld
{
namespace
{
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
	 * Each of Team A's walls (ADR-042 §1): sized, on the floor, wholly in Team A's half so it never meets
	 * its mirror, and WallClearance from everything placed in a straight line. Team B's are the mirror,
	 * so they are too.
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
				Problems.Add(Pointer + TEXT(": the wall must lie wholly in Team A's half; Team B's is its mirror"));
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
		}
	}
}

TArray<FString> Validate(const FVeyraWorldTuning& Tuning)
{
	TArray<FString> Problems;
	const FVeyraBattlegroundLayout& Layout = Tuning.Layout;

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
		if (!VeyraLayout::MirrorsOntoItself(Lane))
		{
			Problems.Add(Pointer + TEXT("/points: the lane must be its own mirror across Y = -X, reversed, so both teams walk the same distances"));
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
	// Each fog circle lies wholly on the floor and in Team A's half, so it and its mirror are apart: a
	// circle touching the dividing line would touch its mirror there, and touching circles are one
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
			Problems.Add(Pointer + TEXT(": the fog must lie wholly in Team A's half, clear of the dividing line; Team B's is its mirror"));
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
	// Each of Team A's camps, its leash and all, on the floor, on Team A's half clear of the river, and
	// clear of every lane; Team B's are their mirror, so they are too.
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
		if (VeyraLayout::DepthInTeamAHalf(Layout, Center) < Layout.RiverWidth / 2.0 + Reach)
		{
			Problems.Add(Pointer + TEXT("/center: the camp must lie on Team A's half, its leash clear of the river"));
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

	// The Flux Wells stand on the river, each its own mirror, clear of every lane (§6; ADR-014 §4).
	const FVeyraFluxWellsTuning& Wells = Tuning.FluxWells;
	for (int32 Index = 0; Index < Wells.Sites.Num(); ++Index)
	{
		const FString Pointer = FString::Printf(TEXT("/fluxWells/sites/%d"), Index);
		const FVector2D Site = VeyraLayout::ToVector(Wells.Sites[Index]);
		if (!IsOnFloor(Wells.Sites[Index], Layout.HalfExtent))
		{
			Problems.Add(Pointer + TEXT(": must lie on the floor"));
		}
		if (FMath::Abs(VeyraLayout::DepthInTeamAHalf(Layout, Site)) > Layout.RiverWidth / 2.0)
		{
			Problems.Add(Pointer + TEXT(": must lie on the river, so both teams reach it alike"));
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
