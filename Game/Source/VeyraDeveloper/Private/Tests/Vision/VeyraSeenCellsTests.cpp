// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Rules/VeyraVisionRules.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraSeenCellsTests
{
	// Veyra.Vision.SeenCells.*: the ground a side sees, as a grid of cells (ADR-054 §2), by the gate's own sight rules.
	// Fixture values: a 10-by-10 grid over a square reaching 1000 from the origin, so a cell is 200 wide and cell 5's
	// centre lies at 100.
	TEST_CLASS(SeenCells, "Veyra.Vision")
	{
		static constexpr int32 Across = 10;
		static constexpr double HalfExtent = 1000.0;
		const FVeyraSeenGrid Grid = FVeyraSeenGrid::Over(FVector2D::ZeroVector, HalfExtent, Across);

		static FVeyraSightSource SourceAt(EVeyraTeam Team, const FVector2D& Where, double Radius)
		{
			FVeyraSightSource Source;
			Source.Team = Team;
			Source.Position = Where;
			Source.Radius = Radius;
			return Source;
		}

		TEST_METHOD(ASourceLightsTheCellsWithinItsSightForItsSideAlone)
		{
			const TArray<FVeyraSightSource> Sources = { SourceAt(EVeyraTeam::A, FVector2D::ZeroVector, 300.0) };
			const TArray<uint8> Seen = VeyraVisionRules::SeenCells(EVeyraTeam::A, Sources, Grid);
			ASSERT_THAT(AreEqual((Across * Across + 7) / 8, Seen.Num(), TEXT("a bit a cell")));
			// The four cells round the origin have their centres 141 away; the next ones out, 316.
			for (const FIntPoint Cell : { FIntPoint(4, 4), FIntPoint(5, 4), FIntPoint(4, 5), FIntPoint(5, 5) })
			{
				ASSERT_THAT(IsTrue(VeyraVisionRules::IsCellSeen(Seen, Grid, Cell.X, Cell.Y)));
			}
			ASSERT_THAT(IsFalse(VeyraVisionRules::IsCellSeen(Seen, Grid, 6, 5), TEXT("beyond its sight")));
			int32 Count = 0;
			for (int32 Y = 0; Y < Across; ++Y)
			{
				for (int32 X = 0; X < Across; ++X)
				{
					Count += VeyraVisionRules::IsCellSeen(Seen, Grid, X, Y) ? 1 : 0;
				}
			}
			ASSERT_THAT(AreEqual(4, Count));
			const TArray<uint8> Other = VeyraVisionRules::SeenCells(EVeyraTeam::B, Sources, Grid);
			ASSERT_THAT(IsFalse(Other.ContainsByPredicate([](uint8 Byte) { return Byte != 0; }), TEXT("the other side sees none of it")));
			ASSERT_THAT(IsFalse(VeyraVisionRules::IsCellSeen(Seen, Grid, -1, 4) || VeyraVisionRules::IsCellSeen(Seen, Grid, Across, 4), TEXT("nothing off the grid")));
		}

		TEST_METHOD(AWallHidesTheGroundBehindItSaveFromALitArea)
		{
			// A wall across the line x = 200, so cell 6's centre, at x = 300, lies behind it.
			const FVeyraSightWalls Walls({ FVeyraTerrainBox{ FVector2D(200.0, 0.0), FVector2D(1.0, 0.0), 2.0 * HalfExtent, 20.0 } });
			TArray<FVeyraSightSource> Sources = { SourceAt(EVeyraTeam::A, FVector2D::ZeroVector, 800.0) };
			const TArray<uint8> Seen = VeyraVisionRules::SeenCells(EVeyraTeam::A, Sources, Grid, Walls);
			ASSERT_THAT(IsTrue(VeyraVisionRules::IsCellSeen(Seen, Grid, 4, 5), TEXT("this side of the wall")));
			ASSERT_THAT(IsFalse(VeyraVisionRules::IsCellSeen(Seen, Grid, 6, 5), TEXT("behind it")));
			// A lit area lights what lies inside it directly (ADR-043 §3).
			Sources[0].bThroughWalls = true;
			ASSERT_THAT(IsTrue(VeyraVisionRules::IsCellSeen(VeyraVisionRules::SeenCells(EVeyraTeam::A, Sources, Grid, Walls), Grid, 6, 5)));
		}
	};
}

#endif