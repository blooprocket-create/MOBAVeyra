// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Components/ActorTestSpawner.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Layout/VeyraLayout.h"
#include "Layout/VeyraRiver.h"
#include "Materials/MaterialInterface.h"
#include "Movement/VeyraMovementComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "VeyraVanguardCharacter.h"
#include "Water/VeyraWaterSubsystem.h"
#include "Tuning/VeyraWorldTuning.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "Water/VeyraWaterRipples.h"
#include "Water/VeyraWaterSettings.h"

namespace VeyraWaterTests
{
	// Veyra.UI.Water.*: the river answering the units wading it (ADR-076): the window its ripples follow, how hard a unit
	// stirs them, where a unit wades, and the shipped settings and ripple material.
	TEST_CLASS(Water, "Veyra.UI")
	{
		static constexpr double Size = 6000.0;
		static constexpr int32 Resolution = 512;
		static constexpr double Slack = 1e-6;
		static constexpr double Lift = 10.0;

		TEST_METHOD(TheShippedWaterSettingsAreValid)
		{
			const TArray<FString> Problems = GetDefault<UVeyraWaterSettings>()->Validate();
			ASSERT_THAT(IsTrue(Problems.IsEmpty(), *FString::Join(Problems, TEXT("; "))));
		}

		TEST_METHOD(BrokenSettingsAreRefused)
		{
			// A fresh object starts from the shipped settings, its archetype.
			UVeyraWaterSettings* Broken = NewObject<UVeyraWaterSettings>(GetTransientPackage());
			Broken->MaxSources = UVeyraWaterSettings::SourceSlots + 1;
			Broken->Damping = 1.0f;
			Broken->HeightParameter = Broken->WindowParameter;
			const TArray<FString> Problems = Broken->Validate();
			ASSERT_THAT(IsTrue(Problems.ContainsByPredicate([](const FString& Problem) { return Problem.StartsWith(TEXT("MaxSources")); })));
			ASSERT_THAT(IsTrue(Problems.ContainsByPredicate([](const FString& Problem) { return Problem.StartsWith(TEXT("Damping")); })));
			ASSERT_THAT(IsTrue(Problems.ContainsByPredicate([](const FString& Problem) { return Problem.StartsWith(TEXT("StateParameter")); }), TEXT("names must differ")));
		}

		TEST_METHOD(TheWindowFollowsTheViewInWholeTexels)
		{
			const double Texel = Size / Resolution;
			const FVeyraRippleWindow First = VeyraWaterRipples::Follow(FVeyraRippleWindow(), FVector2D(1234.5, -987.6), Size, Resolution);
			ASSERT_THAT(IsTrue(First.bPlaced));
			ASSERT_THAT(IsTrue(First.Shift == FIntPoint::ZeroValue, TEXT("a window just placed has not moved")));
			ASSERT_THAT(IsNear(FMath::Fmod(FMath::Abs(First.Origin.X), Texel), 0.0, 1e-3, TEXT("its corner sits on a texel")));
			ASSERT_THAT(IsTrue(FMath::Abs(First.Origin.X + Size * 0.5 - 1234.5) <= Texel, TEXT("about the focus")));
			const FVeyraRippleWindow Moved = VeyraWaterRipples::Follow(First, FVector2D(1234.5 + 3.0 * Texel, -987.6 - 2.0 * Texel), Size, Resolution);
			ASSERT_THAT(IsTrue(Moved.Shift == FIntPoint(3, -2), TEXT("moved by whole texels, and says so")));
			const FVeyraRippleWindow Still = VeyraWaterRipples::Follow(Moved, FVector2D(1234.5 + 3.2 * Texel, -987.6 - 2.0 * Texel), Size, Resolution);
			ASSERT_THAT(IsTrue(Still.Shift == FIntPoint::ZeroValue || Still.Shift == FIntPoint(1, 0), TEXT("less than a texel moves it at most one")));
		}

		TEST_METHOD(AUnitStirsHarderTheFasterItMoves)
		{
			ASSERT_THAT(IsNear(VeyraWaterRipples::StrengthAt(0.0, 0.2, 1.0, 400.0), 0.2, Slack, TEXT("standing")));
			ASSERT_THAT(IsNear(VeyraWaterRipples::StrengthAt(200.0, 0.2, 1.0, 400.0), 0.6, Slack, TEXT("halfway to full speed")));
			ASSERT_THAT(IsNear(VeyraWaterRipples::StrengthAt(900.0, 0.2, 1.0, 400.0), 1.0, Slack, TEXT("no harder past full speed")));
		}

		TEST_METHOD(AStirIsTakenAsSharesOfItsWindowAndDroppedOutsideIt)
		{
			const FVeyraRippleWindow Window = VeyraWaterRipples::Follow(FVeyraRippleWindow(), FVector2D::ZeroVector, Size, Resolution);
			const TOptional<FLinearColor> Inside = VeyraWaterRipples::SourceIn(Window, Size, Window.Origin + FVector2D(Size * 0.25, Size * 0.75), 60.0, 0.5);
			ASSERT_THAT(IsTrue(Inside.IsSet()));
			ASSERT_THAT(IsNear(Inside->R, 0.25f, 1e-5f));
			ASSERT_THAT(IsNear(Inside->G, 0.75f, 1e-5f));
			ASSERT_THAT(IsNear(Inside->B, static_cast<float>(60.0 / Size), 1e-6f, TEXT("its radius as a share")));
			ASSERT_THAT(IsNear(Inside->A, 0.5f, 1e-6f, TEXT("its strength")));
			ASSERT_THAT(IsFalse(VeyraWaterRipples::SourceIn(Window, Size, Window.Origin + FVector2D(Size * 2.0, 0.0), 60.0, 0.5).IsSet()));
			ASSERT_THAT(IsFalse(VeyraWaterRipples::SourceIn(FVeyraRippleWindow(), Size, FVector2D::ZeroVector, 60.0, 0.5).IsSet(), TEXT("no window, no stir")));
		}

		TEST_METHOD(OnlyFeetUnderTheRiversSurfaceWadeIt)
		{
			const FVeyraBattlegroundLayout& Layout = UVeyraWorldTuningSubsystem::Get().Layout;
			const FVeyraRiverShape& River = VeyraRiver::ShapeOf(Layout);
			const FVeyraRiverChannel& Main = River.GetChannels()[0];
			const FVector2D InWater = Main.Samples[Main.Samples.Num() / 2].Point;
			const double Surface = Layout.River.SurfaceZ;
			ASSERT_THAT(IsTrue(VeyraWaterRipples::Wades(River, Surface, FVector(InWater, Surface - Lift))));
			ASSERT_THAT(IsFalse(VeyraWaterRipples::Wades(River, Surface, FVector(InWater, Surface + Lift)), TEXT("above the surface is not in it")));
			const FVector2D Ashore = VeyraLayout::Fountain(Layout, EVeyraTeam::A);
			ASSERT_THAT(IsFalse(VeyraWaterRipples::Wades(River, Surface, FVector(Ashore, Surface - Lift)), TEXT("not ashore, however low")));
		}

		TEST_METHOD(TheViewFallsOnTheWaterWhereTheCameraLooks)
		{
			const FVector Camera(0.0, 0.0, 1000.0);
			const FVector Down45 = FVector(1.0, 0.0, -1.0).GetSafeNormal();
			ASSERT_THAT(IsTrue(VeyraWaterRipples::FocusOf(Camera, Down45, 0.0).Equals(FVector2D(1000.0, 0.0), 1e-3)));
			ASSERT_THAT(IsTrue(VeyraWaterRipples::FocusOf(Camera, FVector::ForwardVector, 0.0).Equals(FVector2D::ZeroVector, 1e-3), TEXT("level: right under it")));
		}

		TEST_METHOD(TheRippleMaterialTakesEveryParameterItsSettingsName)
		{
			const UVeyraWaterSettings& Settings = *GetDefault<UVeyraWaterSettings>();
			const UMaterialInterface* Material = Settings.RippleMaterial.LoadSynchronous();
			ASSERT_THAT(IsNotNull(Material, TEXT("run BuildTerrainArt.ps1")));
			TArray<FMaterialParameterInfo> Infos;
			TArray<FGuid> Ids;
			TSet<FName> Named;
			Material->GetAllScalarParameterInfo(Infos, Ids);
			for (const FMaterialParameterInfo& Info : Infos)
			{
				Named.Add(Info.Name);
			}
			Material->GetAllVectorParameterInfo(Infos, Ids);
			for (const FMaterialParameterInfo& Info : Infos)
			{
				Named.Add(Info.Name);
			}
			Material->GetAllTextureParameterInfo(Infos, Ids);
			for (const FMaterialParameterInfo& Info : Infos)
			{
				Named.Add(Info.Name);
			}
			for (const FName Name : { Settings.StateParameter, Settings.TexelParameter, Settings.ShiftParameter, Settings.DampingParameter,
					 Settings.EdgeFadeParameter, Settings.PhaseParameter })
			{
				ASSERT_THAT(IsTrue(Named.Contains(Name), *Name.ToString()));
			}
			for (int32 Slot = 0; Slot < UVeyraWaterSettings::SourceSlots; ++Slot)
			{
				ASSERT_THAT(IsTrue(Named.Contains(Settings.SourceParameter(Slot)), *Settings.SourceParameter(Slot).ToString()));
			}
		}
	};

	// Veyra.UI.WaterWorld.*: the river answering a Vanguard in a world (ADR-076), with the committed river and the shipped
	// settings and effects. Tests run with no renderer, so the ripples are not stepped; their stirs and the wakes are.
	TEST_CLASS(WaterWorld, "Veyra.UI")
	{
		static constexpr double Lift = 10.0;
		static constexpr double Fast = 600.0;
		static constexpr float Frame = 1.0f / 30.0f;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Wader = nullptr;
		UVeyraWaterSubsystem* Water = nullptr;
		FVector2D InWater = FVector2D::ZeroVector;
		FVector2D Ashore = FVector2D::ZeroVector;
		double Surface = 0.0;

		BEFORE_EACH()
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			Wader = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			// The river's drawn surface, as the battleground's map holds it: tagged, with the river's material.
			AStaticMeshActor& River = Spawner.SpawnActor<AStaticMeshActor>();
			River.Tags.Add(GetDefault<UVeyraWaterSettings>()->RiverTag);
			River.GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
			River.GetStaticMeshComponent()->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Veyra/World/Environment/Terrain/M_CrucibleWater.M_CrucibleWater")));
			const FVeyraBattlegroundLayout& Layout = UVeyraWorldTuningSubsystem::Get().Layout;
			const FVeyraRiverChannel& Main = VeyraRiver::ShapeOf(Layout).GetChannels()[0];
			InWater = Main.Samples[Main.Samples.Num() / 2].Point;
			Ashore = VeyraLayout::Fountain(Layout, EVeyraTeam::A);
			Surface = Layout.River.SurfaceZ;
			Water = Spawner.GetWorld().GetSubsystem<UVeyraWaterSubsystem>();
		}

		/** Stands the Vanguard with its feet just under the water's surface at At, moving at Speed. */
		void Stand(const FVector2D& At, double Speed)
		{
			Wader->SetActorLocation(FVector(At, Surface - Lift + Wader->GetSimpleCollisionHalfHeight()));
			Wader->GetVeyraMovement()->Velocity = FVector(Speed, 0.0, 0.0);
		}

		TEST_METHOD(AVanguardInTheWaterStirsItAndWearsAWake)
		{
			ASSERT_THAT(IsNotNull(Water));
			ASSERT_THAT(IsTrue(Water->IsReady(), TEXT("its settings, material and effects load")));
			Stand(InWater, 0.0);
			Water->Refresh(Frame, InWater);
			ASSERT_THAT(IsTrue(Water->HasRiver()));
			ASSERT_THAT(IsTrue(Water->GetSources().Num() == 1, TEXT("it stirs the water")));
			ASSERT_THAT(IsTrue(Water->IsWading(*Wader), TEXT("and wades it, which its wake follows")));
			Stand(Ashore, 0.0);
			Water->Refresh(Frame, InWater);
			ASSERT_THAT(IsTrue(Water->GetSources().IsEmpty(), TEXT("ashore it stirs nothing")));
			ASSERT_THAT(IsFalse(Water->IsWading(*Wader), TEXT("nor wades it")));
			ASSERT_THAT(IsNull(Water->FindWake(*Wader), TEXT("and has no wake")));
		}

		TEST_METHOD(RunningInThrowsOneSplashAndWalkingInNone)
		{
			ASSERT_THAT(IsNotNull(Water));
			Stand(Ashore, Fast);
			Water->Refresh(Frame, InWater);
			Stand(InWater, Fast);
			Water->Refresh(Frame, InWater);
			ASSERT_THAT(IsTrue(Water->GetSplashCount() == 1, TEXT("running in throws a splash")));
			Water->Refresh(Frame, InWater);
			ASSERT_THAT(IsTrue(Water->GetSplashCount() == 1, TEXT("once, not every frame it wades")));
			Stand(Ashore, 0.0);
			Water->Refresh(Frame, InWater);
			Stand(InWater, 0.0);
			Water->Refresh(Frame, InWater);
			ASSERT_THAT(IsTrue(Water->GetSplashCount() == 1, TEXT("walking in slowly throws none")));
		}
	};
}

#endif
