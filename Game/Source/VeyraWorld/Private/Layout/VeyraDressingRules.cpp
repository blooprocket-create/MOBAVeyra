// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Layout/VeyraDressingRules.h"

#include "Algo/AnyOf.h"
#include "Layout/VeyraLayout.h"
#include "Layout/VeyraRiver.h"
#include "Tuning/VeyraWorldTuning.h"

bool VeyraDressing::Allows(const FVeyraWorldTuning& Tuning, const FVector2D& Point, double Radius)
{
	const FVeyraBattlegroundLayout& Layout = Tuning.Layout;
	if (VeyraRiver::ShapeOf(Layout).SignedDistance(Point) < Radius)
	{
		return false;
	}
	const bool bOnALane = Algo::AnyOf(Layout.Lanes, [&](const FVeyraLaneLayout& Lane) {
		return VeyraLayout::DistanceToPath(Lane.Points, Point) < Lane.Width / 2.0 + Layout.WallClearance + Radius;
	});
	const bool bInFog = Algo::AnyOf(VeyraLayout::DenseFog(Layout), [&](const FVeyraFogPlacement& Fog) {
		return FVector2D::Distance(Point, Fog.Center) < Fog.Radius + Radius;
	});
	const bool bByAStructure = Algo::AnyOf(VeyraLayout::Structures(Layout), [&](const FVeyraStructurePlacement& Structure) {
		return FVector2D::Distance(Point, Structure.Location) < Tuning.Structures.PrimeWell.CapsuleRadius + Layout.WallClearance + Radius;
	});
	const bool bByAWell = Algo::AnyOf(Tuning.FluxWells.Sites, [&](const FVeyraMapPoint& Site) {
		return FVector2D::Distance(Point, VeyraLayout::ToVector(Site)) < Tuning.FluxWells.Radius + Layout.WallClearance + Radius;
	});
	if (bOnALane || bInFog || bByAStructure || bByAWell)
	{
		return false;
	}
	for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
	{
		if (FVector2D::Distance(Point, VeyraLayout::Fountain(Layout, Team)) < Layout.Base.FountainRadius + Radius)
		{
			return false;
		}
		const bool bInACamp = Algo::AnyOf(Tuning.Wildlife.Camps, [&](const FVeyraCampTuning& Camp) {
			return FVector2D::Distance(Point, VeyraLayout::ForTeam(VeyraLayout::ToVector(Camp.Center), Team)) < Camp.LeashRadius + Radius;
		});
		if (bInACamp)
		{
			return false;
		}
	}
	return true;
}
