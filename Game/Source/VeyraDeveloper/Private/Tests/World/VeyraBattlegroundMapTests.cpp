// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Battleground/VeyraBattlegroundBuilder.h"
#include "Battleground/VeyraBattlegroundMapCommandlet.h"
#include "Battleground/VeyraBattlegroundMarker.h"
#include "Components/BrushComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CQTest.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Layout/VeyraLayout.h"
#include "Misc/PackageName.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "VeyraTeamStart.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraWorldTests
{
	// Veyra.World.BattlegroundMap.*: the saved battleground still matches World.json's layout. When it
	// does not, run Game/Scripts/BuildBattlegroundMap.ps1.
	TEST_CLASS(BattlegroundMap, "Veyra.World")
	{
		// Positions are saved as doubles; this only absorbs transform round-off.
		static constexpr double Tolerance = 0.01;

		TEST_METHOD(SavedMapMatchesTheLayout)
		{
			FVeyraGreyboxLayout Greybox;
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Greybox).IsEmpty()));
			const FVeyraBattlegroundLayout& Layout = UVeyraWorldTuningSubsystem::Get().Layout;
			const FVeyraGreyboxLayout Floor = VeyraBattlegroundBuilder::AsGreybox(Layout, Greybox);

			const FString MapPackage(UVeyraBattlegroundMapCommandlet::MapPackageName);
			const UWorld* Map = LoadObject<UWorld>(nullptr, *(MapPackage + TEXT(".") + FPackageName::GetShortName(MapPackage)));
			ASSERT_THAT(IsNotNull(Map));

			TMap<EVeyraTeam, FVector> Starts;
			TArray<FBox> NavigationBounds;
			int32 Landscapes = 0;
			int32 Markers = 0;
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
				else if (Actor && Actor->ActorHasTag(TEXT("Veyra.AuthoredTerrain")))
				{
					ASSERT_THAT(IsTrue(Actor->GetClass()->GetFName() == TEXT("Landscape"), TEXT("continuous terrain is a Landscape")));
					++Landscapes;
				}
				else if (Cast<AVeyraBattlegroundMarker>(Actor))
				{
					++Markers;
				}
			}

			ASSERT_THAT(IsTrue(Markers == 1, TEXT("the server spawns the structures only on a map that is marked")));
			ASSERT_THAT(AreEqual(2, Starts.Num()));
			for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
			{
				ASSERT_THAT(IsTrue(Starts.Contains(Team)));
				const FVector2D Fountain = VeyraLayout::Fountain(Layout, Team);
				ASSERT_THAT(IsTrue(FVector2D(Starts[Team]).Equals(Fountain, Tolerance), TEXT("each team starts at its fountain")));
			}

			ASSERT_THAT(AreEqual(1, NavigationBounds.Num()));
			ASSERT_THAT(IsTrue(NavigationBounds[0].GetSize().Equals(FVector(Layout.HalfExtent * 2.0, Layout.HalfExtent * 2.0, Layout.Surface.MaxZ - Layout.Surface.MinZ), Tolerance)));

			ASSERT_THAT(AreEqual(1, Landscapes));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
