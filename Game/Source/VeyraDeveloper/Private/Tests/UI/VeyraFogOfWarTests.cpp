// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Hud/VeyraFogOfWarModel.h"
#include "Hud/VeyraMinimapModel.h"
#include "State/VeyraVisionTeamState.h"

namespace VeyraFogOfWarTests
{
	// Veyra.UI.FogOfWar.*: the ground the viewer's side does not see, darkened in the world and on the minimap
	// (ADR-054 §3). Fixture values: a 4-by-4 grid of 100-wide cells from (-200, -200).
	TEST_CLASS(FogOfWar, "Veyra.UI")
	{
		static FVeyraSeenGround GroundSeeing(TConstArrayView<FIntPoint> Seen)
		{
			FVeyraSeenGround Ground;
			Ground.Min = FVector2D(-200.0, -200.0);
			Ground.CellSize = 100.0;
			Ground.CellsAcross = 4;
			Ground.Cells.SetNumZeroed(2);
			for (const FIntPoint Cell : Seen)
			{
				const int32 Index = Cell.Y * Ground.CellsAcross + Cell.X;
				Ground.Cells[Index / 8] |= static_cast<uint8>(1u << (Index % 8));
			}
			return Ground;
		}

		TEST_METHOD(UnseenCellsJoinIntoRunsRowByRow)
		{
			// Row 0 sees its two middle cells; the other rows see nothing.
			const FVeyraSeenGround Ground = GroundSeeing({ FIntPoint(1, 0), FIntPoint(2, 0) });
			const TArray<FVeyraUnseenRun> Runs = VeyraFogOfWar::UnseenRuns(Ground);
			ASSERT_THAT(AreEqual(5, Runs.Num(), TEXT("two in the first row, one in each of the others")));
			ASSERT_THAT(IsTrue(Runs[0].Row == 0 && Runs[0].First == 0 && Runs[0].Last == 0));
			ASSERT_THAT(IsTrue(Runs[1].Row == 0 && Runs[1].First == 3 && Runs[1].Last == 3));
			ASSERT_THAT(IsTrue(Runs[2].Row == 1 && Runs[2].First == 0 && Runs[2].Last == 3));
			const FBox2D Corner = VeyraFogOfWar::BoundsOf(Ground, Runs[1]);
			ASSERT_THAT(IsTrue(Corner.Min.Equals(FVector2D(100.0, -200.0)) && Corner.Max.Equals(FVector2D(200.0, -100.0)), TEXT("the cell it covers")));
			const FBox2D Row = VeyraFogOfWar::BoundsOf(Ground, Runs[2]);
			ASSERT_THAT(IsTrue(Row.Min.Equals(FVector2D(-200.0, -100.0)) && Row.Max.Equals(FVector2D(200.0, 0.0)), TEXT("a whole row as one")));
		}

		TEST_METHOD(SeenGroundIsNeverDarkenedAndAChangeRedrawsIt)
		{
			TArray<FIntPoint> Every;
			for (int32 Y = 0; Y < 4; ++Y)
			{
				for (int32 X = 0; X < 4; ++X)
				{
					Every.Add(FIntPoint(X, Y));
				}
			}
			const FVeyraSeenGround All = GroundSeeing(Every);
			ASSERT_THAT(IsTrue(VeyraFogOfWar::UnseenRuns(All).IsEmpty()));
			const FVeyraSeenGround Fewer = GroundSeeing({ FIntPoint(0, 0) });
			ASSERT_THAT(IsTrue(VeyraFogOfWar::SignatureOf(&All) != VeyraFogOfWar::SignatureOf(&Fewer)));
			ASSERT_THAT(IsTrue(VeyraFogOfWar::SignatureOf(&All) == VeyraFogOfWar::SignatureOf(&All)));
			ASSERT_THAT(AreEqual(0u, VeyraFogOfWar::SignatureOf(nullptr), TEXT("none for no ground")));
		}

		TEST_METHOD(TheMinimapDarkensTheSameGround)
		{
			// A map reaching 200 each way, on a 100-pixel minimap from (10, 20): +X is up and +Y right.
			const FVeyraMinimapFrame Frame = VeyraMinimap::FrameFor(FVector2D(110.0, 120.0), 100.0, 0.0, 200.0);
			const FVeyraSeenGround Ground = GroundSeeing({ FIntPoint(1, 0), FIntPoint(2, 0) });
			const TArray<FBox2D> Fog = VeyraMinimap::DescribeFog(Frame, Ground);
			ASSERT_THAT(AreEqual(VeyraFogOfWar::UnseenRuns(Ground).Num(), Fog.Num()));
			// The corner cell (x 100–200, y -200–-100) is the map's top-left corner, a quarter of its width each way.
			const FVector2D TopLeft = VeyraMinimap::ToMap(Frame, FVector(200.0, -200.0, 0.0));
			const FVector2D Across = VeyraMinimap::ToMap(Frame, FVector(100.0, -100.0, 0.0));
			ASSERT_THAT(IsTrue(Fog[1].Min.Equals(TopLeft) && Fog[1].Max.Equals(Across), TEXT("its screen box, whatever the axes")));
		}
	};
}

#endif