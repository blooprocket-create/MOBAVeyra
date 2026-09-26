// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/BrushComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Greybox/VeyraGreyboxLayout.h"
#include "Greybox/VeyraGreyboxMapCommandlet.h"
#include "Misc/PackageName.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "VeyraTeamStart.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraMatchTests
{
	// Veyra.Match.GreyboxMap.*: the saved grey-box map still matches Greybox.json. When it does not,
	// run Game/Scripts/BuildGreyboxMap.ps1.
	TEST_CLASS(GreyboxMap, "Veyra.Match")
	{
		// Positions are saved as doubles; this only absorbs transform round-off.
		static constexpr double Tolerance = 0.01;

		TEST_METHOD(SavedMapMatchesTheLayout)
		{
			FVeyraGreyboxLayout Layout;
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));

			const FString MapPackage(UVeyraGreyboxMapCommandlet::MapPackageName);
			const UWorld* Map = LoadObject<UWorld>(nullptr, *(MapPackage + TEXT(".") + FPackageName::GetShortName(MapPackage)));
			ASSERT_THAT(IsNotNull(Map));

			TMap<EVeyraTeam, FVector> Starts;
			TArray<FBox> NavigationBounds;
			TArray<FVector> FloorSizes;
			for (const AActor* Actor : Map->PersistentLevel->Actors)
			{
				if (const AVeyraTeamStart* Start = Cast<AVeyraTeamStart>(Actor))
				{
					Starts.Add(Start->GetVeyraTeam(), Start->GetActorLocation());
				}
				else if (const ANavMeshBoundsVolume* Bounds = Cast<ANavMeshBoundsVolume>(Actor))
				{
					NavigationBounds.Add(Bounds->GetBrushComponent()->CalcBounds(Bounds->GetActorTransform()).GetBox());
				}
				else if (const AStaticMeshActor* Floor = Cast<AStaticMeshActor>(Actor))
				{
					const UStaticMesh* Mesh = Floor->GetStaticMeshComponent()->GetStaticMesh();
					ASSERT_THAT(IsNotNull(Mesh));
					FloorSizes.Add(Mesh->GetBoundingBox().GetSize() * Floor->GetActorScale3D());
				}
			}

			ASSERT_THAT(AreEqual(Starts.Num(), 2));
			ASSERT_THAT(IsTrue(Starts.Contains(EVeyraTeam::A) && Starts.Contains(EVeyraTeam::B)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Starts[EVeyraTeam::A].X, -Layout.TeamStarts.DistanceFromCenterX, Tolerance)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Starts[EVeyraTeam::B].X, Layout.TeamStarts.DistanceFromCenterX, Tolerance)));

			ASSERT_THAT(AreEqual(NavigationBounds.Num(), 1));
			ASSERT_THAT(IsTrue(NavigationBounds[0].GetSize().Equals(VeyraGreybox::NavigationBoundsSize(Layout), Tolerance)));

			ASSERT_THAT(AreEqual(FloorSizes.Num(), 1));
			ASSERT_THAT(IsTrue(FloorSizes[0].Equals(FVector(Layout.Floor.LengthX, Layout.Floor.WidthY, Layout.Floor.ThicknessZ), Tolerance)));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
