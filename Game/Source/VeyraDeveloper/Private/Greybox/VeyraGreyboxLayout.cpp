// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraGreyboxLayout.h"

#include "Components/BrushComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "PhysicsEngine/BodySetup.h"
#include "VeyraTeamStart.h"

namespace VeyraGreybox
{
namespace
{
	const TCHAR* const CubeMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");

	FString LayoutPath(const TCHAR* FileName)
	{
		return FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("VeyraDeveloper"), TEXT("Greybox"), FileName);
	}
}

VeyraTuning::FErrors LoadLayout(FVeyraGreyboxLayout& OutLayout)
{
	VeyraTuning::FErrors Errors;
	TArray<uint8> DocumentBytes;
	TArray<uint8> SchemaBytes;
	for (const auto& [Path, Bytes] : { TPair<FString, TArray<uint8>*>(LayoutPath(TEXT("Greybox.json")), &DocumentBytes),
		TPair<FString, TArray<uint8>*>(LayoutPath(TEXT("Greybox.schema.json")), &SchemaBytes) })
	{
		if (!FFileHelper::LoadFileToArray(*Bytes, *Path, FILEREAD_Silent))
		{
			Errors.Add(FString::Printf(TEXT("%s: cannot be read"), *Path));
		}
	}
	if (!Errors.IsEmpty())
	{
		return Errors;
	}

	FVeyraGreyboxLayout Layout;
	Errors = VeyraTuning::ValidateAndBind(VeyraTuning::DecodeUtf8(DocumentBytes), VeyraTuning::DecodeUtf8(SchemaBytes),
		FVeyraGreyboxLayout::SchemaVersion, Layout);
	if (Errors.IsEmpty() && Layout.TeamStarts.DistanceFromCenterX >= Layout.Floor.LengthX / 2.0)
	{
		Errors.Add(TEXT("/teamStarts/distanceFromCenterX: the starts must lie on the floor"));
	}
	if (Errors.IsEmpty())
	{
		OutLayout = Layout;
	}
	return Errors;
}

FVector NavigationBoundsSize(const FVeyraGreyboxLayout& Layout)
{
	return FVector(Layout.Floor.LengthX, Layout.Floor.WidthY, Layout.Navigation.HeightZ);
}

void SpawnFloor(UWorld& World, const FVeyraGreyboxLayout& Layout, EComponentMobility::Type Mobility)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, CubeMeshPath);
	checkf(Cube, TEXT("Engine mesh %s is missing."), CubeMeshPath);

	// Scale the engine's cube, centred on its origin, to the floor's size, top face at height 0.
	const FVector Size(Layout.Floor.LengthX, Layout.Floor.WidthY, Layout.Floor.ThicknessZ);
	const FTransform Transform(FRotator::ZeroRotator, FVector(0.0, 0.0, -Size.Z / 2.0), Size / Cube->GetBoundingBox().GetSize());
	AStaticMeshActor* Floor = World.SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
	Floor->SetMobility(Mobility);
	Floor->GetStaticMeshComponent()->SetStaticMesh(Cube);
}

void SpawnTeamStarts(UWorld& World, const FVeyraGreyboxLayout& Layout)
{
	for (const auto& [Team, Direction] : { TPair<EVeyraTeam, double>(EVeyraTeam::A, -1.0), TPair<EVeyraTeam, double>(EVeyraTeam::B, 1.0) })
	{
		AVeyraTeamStart* Start = World.SpawnActorDeferred<AVeyraTeamStart>(AVeyraTeamStart::StaticClass(), FTransform::Identity);
		Start->SetVeyraTeam(Team);
		// Stand the start's capsule on the floor, facing the centre of the lane.
		const FVector Location(Direction * Layout.TeamStarts.DistanceFromCenterX, 0.0, Start->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		Start->FinishSpawning(FTransform(FVector(-Direction, 0.0, 0.0).Rotation(), Location));
	}
}

void SpawnRuntimeNavigationBounds(UWorld& World, const FVeyraGreyboxLayout& Layout)
{
	// The volume registers with navigation as it spawns, before the box exists, so navigation is
	// told again once the bounds are real.
	ANavMeshBoundsVolume* Bounds = World.SpawnActor<ANavMeshBoundsVolume>();
	UBrushComponent* Brush = Bounds->GetBrushComponent();
	UBodySetup* Box = NewObject<UBodySetup>(Brush, NAME_None, RF_Transient);
	const FVector Size = NavigationBoundsSize(Layout);
	Box->AggGeom.BoxElems.Emplace(Size.X, Size.Y, Size.Z);
	Brush->BrushBodySetup = Box;
	Brush->UpdateBounds();
	if (UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&World))
	{
		Navigation->OnNavigationBoundsUpdated(Bounds);
	}
}
}
