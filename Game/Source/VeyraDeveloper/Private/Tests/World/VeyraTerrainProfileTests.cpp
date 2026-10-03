// Copyright © 2026 Wayfinder Studios. All rights reserved.
#include "CQTest.h"
#include <limits>
#include "Layout/VeyraLayout.h"
#include "Layout/VeyraTerrainProfile.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER
namespace VeyraWorldTests
{
TEST_CLASS(TerrainProfile, "Veyra.World")
{
    TEST_METHOD(BanksAndElevationAreEqualForBothTeams)
    {
        const auto& Tuning = UVeyraWorldTuningSubsystem::Get();
        const FVeyraTerrainSampler Surface(Tuning);
        // Dense test sampling, not a gameplay or authoring resolution.
        for (double X = -Tuning.Layout.HalfExtent; X <= Tuning.Layout.HalfExtent; X += 150.0)
        {
            for (double Y = -Tuning.Layout.HalfExtent; Y <= Tuning.Layout.HalfExtent; Y += 150.0)
            {
                const FVector2D A(X, Y);
                const FVector2D B = VeyraLayout::Mirror(A);
                ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Surface.Height(A), Surface.Height(B), 1e-6), TEXT("mirrored terrain heights")));
                ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Surface.RiverDistance(A), Surface.RiverDistance(B), 1e-6), TEXT("mirrored river banks")));
            }
        }
    }

    TEST_METHOD(RiverHasCurvatureWidthVariationAndLowerGround)
    {
        const auto& Tuning = UVeyraWorldTuningSubsystem::Get();
        const auto& Terrain = Tuning.Layout.Terrain;
        const auto Samples = VeyraTerrainProfile::River(Terrain, false);
        ASSERT_THAT(IsTrue(Samples.Num() > Terrain.RiverControls.Num()));
        bool bCurved = false;
        bool bWidthVaries = false;
        for (const auto& Sample : Samples)
        {
            bCurved |= FMath::Abs(Sample.Point.X + Sample.Point.Y) > 1.0;
            bWidthVaries |= !FMath::IsNearlyEqual(Sample.Width, Samples[0].Width);
        }
        ASSERT_THAT(IsTrue(bCurved && bWidthVaries));
        const FVeyraTerrainSampler Surface(Tuning);
        ASSERT_THAT(IsTrue(Surface.Height(FVector2D::ZeroVector) < Terrain.LaneZ));
        ASSERT_THAT(IsTrue(Terrain.JungleZ > Terrain.LaneZ && Terrain.ExteriorZ > Terrain.JungleZ));
    }

    TEST_METHOD(NonFiniteTerrainAndUnqueryableHeightsAreRejected)
    {
        auto Broken = UVeyraWorldTuningSubsystem::Get();
        Broken.Layout.Terrain.BankBlend = std::numeric_limits<double>::quiet_NaN();
        ASSERT_THAT(IsFalse(VeyraWorld::Validate(Broken).IsEmpty()));
        Broken = UVeyraWorldTuningSubsystem::Get();
        Broken.Layout.Terrain.JungleZ = Broken.Layout.Surface.MaxZ;
        ASSERT_THAT(IsFalse(VeyraWorld::Validate(Broken).IsEmpty()));
    }

    TEST_METHOD(DressingCannotOccupyProtectedAnchors)
    {
        const auto& Tuning = UVeyraWorldTuningSubsystem::Get();
        for (const auto& Placement : VeyraLayout::Structures(Tuning.Layout))
        {
            ASSERT_THAT(IsFalse(VeyraTerrainProfile::AllowsDressing(Tuning, Placement.Location, 10.0)));
        }
        for (const auto& Fog : VeyraLayout::DenseFog(Tuning.Layout))
        {
            ASSERT_THAT(IsFalse(VeyraTerrainProfile::AllowsDressing(Tuning, Fog.Center, 10.0)));
        }
        for (const auto& Camp : Tuning.Wildlife.Camps)
        {
            for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
            {
                ASSERT_THAT(IsFalse(VeyraTerrainProfile::AllowsDressing(Tuning, VeyraLayout::ForTeam(VeyraLayout::ToVector(Camp.Center), Team), 10.0)));
            }
        }
    }
};
}
#endif
