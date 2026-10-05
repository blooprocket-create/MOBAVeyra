// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include <limits>

#include "Layout/VeyraDressingRules.h"
#include "Layout/VeyraLayout.h"
#include "Layout/VeyraRiver.h"
#include "Layout/VeyraTerrainField.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"

namespace VeyraWorldTests
{
	// Veyra.World.TerrainField.*: the battleground's ground as World authoring composes it (ADR-040 §3), from the committed
	// layout: the same for both teams, with real relief, walkable where it is walked, the water below its banks and a ridge
	// on every wall.
	TEST_CLASS(TerrainField, "Veyra.World")
	{
		// Fixture values: how densely the ground is probed, how far apart a slope is measured, and slack in units.
		static constexpr double ProbeStep = 250.0;
		static constexpr double SlopeSpan = 50.0;
		static constexpr double HeightSlack = 1.0;
		// A swell large enough to show in the symmetry check.
		static constexpr double TestReliefAmplitude = 120.0;
		static constexpr double TestReliefWavelength = 2500.0;
		// A small piece of dressing.
		static constexpr double DressingRadius = 10.0;

		static const FVeyraWorldTuning& Committed()
		{
			return UVeyraWorldTuningSubsystem::Get();
		}

		/** Calls Visit on every probe point over the floor and Reach beyond its edge. */
		template <typename FVisit>
		static void ForEachProbe(double Reach, FVisit&& Visit)
		{
			const double Extent = Committed().Layout.HalfExtent + Reach;
			for (double X = -Extent; X <= Extent; X += ProbeStep)
			{
				for (double Y = -Extent; Y <= Extent; Y += ProbeStep)
				{
					Visit(FVector2D(X, Y));
				}
			}
		}

		TEST_METHOD(TheGroundIsTheSameForBothTeams)
		{
			FVeyraTerrainRelief Relief;
			Relief.Amplitude = TestReliefAmplitude;
			Relief.Wavelength = TestReliefWavelength;
			Relief.CrestAmplitude = TestReliefAmplitude;
			const FVeyraTerrainField Field(Committed(), Relief);
			ForEachProbe(Committed().Layout.Terrain.BoundaryWidth, [&](const FVector2D& Point) {
				ASSERT_THAT(IsNear(Field.Height(Point), Field.Height(VeyraLayout::Rotate(Point)), HeightSlack, TEXT("a point and its rotation stand at one height")));
			});
		}

		TEST_METHOD(TheGroundHasRealRelief)
		{
			const FVeyraWorldTuning& Tuning = Committed();
			const FVeyraTerrainTuning& Terrain = Tuning.Layout.Terrain;
			const FVeyraTerrainField Field(Tuning);
			// The river at the centre lies on its bed, below the lanes; each Well's island stands above the water, no higher
			// than its platform.
			ASSERT_THAT(IsNear(Field.Height(FVector2D::ZeroVector), Tuning.Layout.River.BedZ, HeightSlack));
			for (const FVeyraMapPoint& Site : Tuning.FluxWells.Sites)
			{
				const double Island = Field.Height(VeyraLayout::ToVector(Site));
				ASSERT_THAT(IsTrue(Island > Tuning.Layout.River.SurfaceZ && Island <= Terrain.IslandZ + HeightSlack));
			}
			// A base's Prime Well stands on its pad, a wall on its ridge.
			ASSERT_THAT(IsNear(Field.Height(VeyraLayout::ToVector(Tuning.Layout.Base.PrimeWell)), Terrain.BaseZ, HeightSlack));
			for (const FVeyraTerrainBox& Wall : VeyraLayout::Walls(Tuning.Layout))
			{
				ASSERT_THAT(IsNear(Field.Height(Wall.Centre), Terrain.RidgeZ, HeightSlack));
			}
			// Beyond the floor's corner, away from the river, the rim rises.
			const double Beyond = Tuning.Layout.HalfExtent + Terrain.BoundaryWidth;
			ASSERT_THAT(IsNear(Field.Height(FVector2D(-Beyond, -Beyond)), Terrain.BoundaryZ, HeightSlack));
			ASSERT_THAT(IsTrue(Terrain.RidgeZ - Tuning.Layout.River.BedZ > Terrain.JungleZ - Terrain.LaneZ, TEXT("metres of relief, not centimetres")));
		}

		TEST_METHOD(EveryLanesRoadRunsLevelExceptWhereItFords)
		{
			const FVeyraWorldTuning& Tuning = Committed();
			const FVeyraTerrainField Field(Tuning);
			for (const FVeyraLaneLayout& Lane : Tuning.Layout.Lanes)
			{
				const double Length = VeyraLayout::Length(Lane.Points);
				for (double Along = 0.0; Along <= Length; Along += ProbeStep)
				{
					const FVector2D Point = VeyraLayout::PointAlong(Lane.Points, Along);
					const FVeyraTerrainSample Sample = Field.Sample(Point);
					if (Sample.WaterDistance > Tuning.Layout.Terrain.BankWidth && Sample.Pad <= 0.0 && Sample.Ridge <= 0.0)
					{
						ASSERT_THAT(IsNear(Sample.Height, Tuning.Layout.Terrain.LaneZ, HeightSlack));
					}
				}
			}
		}

		TEST_METHOD(WalkedGroundIsWalkable)
		{
			// Everywhere on the floor but a ridge's cliff, every slope is gentler than the surface allows a unit to stand on.
			const FVeyraWorldTuning& Tuning = Committed();
			const FVeyraTerrainField Field(Tuning);
			const double MaxSlope = Tuning.Layout.Surface.MaxSlopeDegrees;
			const TArray<FVeyraTerrainBox> Walls = VeyraLayout::Walls(Tuning.Layout);
			double Steepest = 0.0;
			ForEachProbe(-SlopeSpan, [&](const FVector2D& Point) {
				const bool bByARidge = Walls.ContainsByPredicate([&](const FVeyraTerrainBox& Wall) {
					return Wall.DistanceTo(Point) < Tuning.Layout.Terrain.RidgeSkirt + SlopeSpan;
				});
				if (bByARidge)
				{
					return;
				}
				const double Rise = FMath::Max(FMath::Abs(Field.Height(Point + FVector2D(SlopeSpan, 0.0)) - Field.Height(Point - FVector2D(SlopeSpan, 0.0))),
					FMath::Abs(Field.Height(Point + FVector2D(0.0, SlopeSpan)) - Field.Height(Point - FVector2D(0.0, SlopeSpan))));
				Steepest = FMath::Max(Steepest, FMath::RadiansToDegrees(FMath::Atan(Rise / (2.0 * SlopeSpan))));
			});
			ASSERT_THAT(IsTrue(Steepest < MaxSlope, *FString::Printf(TEXT("the steepest walked slope is %.1f degrees"), Steepest)));
		}

		TEST_METHOD(TheWaterLiesBelowItsBanks)
		{
			const FVeyraWorldTuning& Tuning = Committed();
			const FVeyraTerrainField Field(Tuning);
			ForEachProbe(0.0, [&](const FVector2D& Point) {
				const FVeyraTerrainSample Sample = Field.Sample(Point);
				if (Sample.WaterDistance < 0.0)
				{
					ASSERT_THAT(IsTrue(Sample.Height <= Tuning.Layout.River.SurfaceZ + HeightSlack, TEXT("the river's ground is under its water")));
				}
				else
				{
					ASSERT_THAT(IsTrue(Sample.Height >= Tuning.Layout.River.SurfaceZ - HeightSlack, TEXT("its banks are not")));
				}
			});
		}

		TEST_METHOD(DressingKeepsOffWhatPlayReads)
		{
			const FVeyraWorldTuning& Tuning = Committed();
			for (const FVeyraStructurePlacement& Placement : VeyraLayout::Structures(Tuning.Layout))
			{
				ASSERT_THAT(IsFalse(VeyraDressing::Allows(Tuning, Placement.Location, DressingRadius)));
			}
			for (const FVeyraFogPlacement& Fog : VeyraLayout::DenseFog(Tuning.Layout))
			{
				ASSERT_THAT(IsFalse(VeyraDressing::Allows(Tuning, Fog.Center, DressingRadius)));
			}
			for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
			{
				ASSERT_THAT(IsFalse(VeyraDressing::Allows(Tuning, VeyraLayout::Fountain(Tuning.Layout, Team), DressingRadius)));
				for (const FVeyraCampTuning& Camp : Tuning.Wildlife.Camps)
				{
					ASSERT_THAT(IsFalse(VeyraDressing::Allows(Tuning, VeyraLayout::ForTeam(VeyraLayout::ToVector(Camp.Center), Team), DressingRadius)));
				}
			}
			for (const FVeyraMapPoint& Site : Tuning.FluxWells.Sites)
			{
				ASSERT_THAT(IsFalse(VeyraDressing::Allows(Tuning, VeyraLayout::ToVector(Site), DressingRadius)));
			}
			for (const FVeyraLaneLayout& Lane : Tuning.Layout.Lanes)
			{
				ASSERT_THAT(IsFalse(VeyraDressing::Allows(Tuning, VeyraLayout::PointAlong(Lane.Points, VeyraLayout::Length(Lane.Points) / 2.0), DressingRadius)));
			}
			const FVeyraRiverChannel& Main = VeyraRiver::ShapeOf(Tuning.Layout).GetChannels()[0];
			ASSERT_THAT(IsFalse(VeyraDressing::Allows(Tuning, Main.Samples[Main.Samples.Num() / 2].Point, DressingRadius), TEXT("not in the water")));
			// And somewhere in the jungle, it may.
			bool bAnywhere = false;
			ForEachProbe(0.0, [&](const FVector2D& Point) { bAnywhere |= VeyraDressing::Allows(Tuning, Point, DressingRadius); });
			ASSERT_THAT(IsTrue(bAnywhere));
		}
		TEST_METHOD(NonFiniteTerrainAndUnqueryableHeightsAreRejected)
		{
			FVeyraWorldTuning Broken = Committed();
			Broken.Layout.Terrain.BankWidth = std::numeric_limits<double>::quiet_NaN();
			ASSERT_THAT(IsFalse(VeyraWorld::Validate(Broken).IsEmpty()));
			Broken = Committed();
			Broken.Layout.Terrain.JungleZ = Broken.Layout.Surface.MaxZ;
			ASSERT_THAT(IsFalse(VeyraWorld::Validate(Broken).IsEmpty()));
			Broken = Committed();
			Broken.Layout.Terrain.JungleRise = 1.0;
			ASSERT_THAT(IsTrue(FString::Join(VeyraWorld::Validate(Broken), TEXT(" | ")).Contains(TEXT("steeper than surface.maxSlopeDegrees"))));
		}
	};
}

#endif
