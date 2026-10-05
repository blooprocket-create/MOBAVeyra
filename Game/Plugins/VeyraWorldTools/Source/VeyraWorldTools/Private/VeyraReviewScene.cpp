// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraWorldBuild.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "Input/VeyraCameraSettings.h"
#include "Layout/VeyraLayout.h"
#include "Layout/VeyraRiver.h"
#include "Terrain/VeyraGround.h"
#include "Tuning/VeyraWorldTuning.h"

namespace
{
	FLinearColor ColourOf(const FJsonObject& Look, const TCHAR* Field)
	{
		const TArray<TSharedPtr<FJsonValue>>& Values = Look.GetArrayField(Field);
		return FLinearColor(Values[0]->AsNumber(), Values[1]->AsNumber(), Values[2]->AsNumber());
	}

	/** The sun, the sky and the air (World Production Bible §12): a warm late-afternoon light over the ruin, a haze that leaves
	 * the play space clear and softens the vista, and a restrained grade. Presentation only. */
	void Light(UWorld& World, const FJsonObject& Style)
	{
		const FJsonObject& Look = *Style.GetObjectField(TEXT("lighting"));
		ADirectionalLight* Sun = World.SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(Look.GetNumberField(TEXT("sunPitch")), Look.GetNumberField(TEXT("sunYaw")), 0.0));
		Sun->SetActorLabel(TEXT("Crucible_Sun"));
		Sun->GetRootComponent()->SetMobility(EComponentMobility::Movable);
		UDirectionalLightComponent* SunLight = CastChecked<UDirectionalLightComponent>(Sun->GetLightComponent());
		SunLight->SetIntensity(Look.GetNumberField(TEXT("sunLux")));
		SunLight->bUseTemperature = true;
		SunLight->Temperature = Look.GetNumberField(TEXT("sunTemperature"));
		SunLight->bAtmosphereSunLight = true;
		SunLight->DynamicShadowDistanceMovableLight = Look.GetNumberField(TEXT("shadowDistance"));

		World.SpawnActor<ASkyAtmosphere>()->SetActorLabel(TEXT("Crucible_Sky"));
		ASkyLight* Sky = World.SpawnActor<ASkyLight>();
		Sky->SetActorLabel(TEXT("Crucible_SkyLight"));
		Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		Sky->GetLightComponent()->SetIntensity(Look.GetNumberField(TEXT("skyIntensity")));
		Sky->GetLightComponent()->SetRealTimeCapture(true);

		AExponentialHeightFog* Haze = World.SpawnActor<AExponentialHeightFog>(FVector(0.0, 0.0, Look.GetNumberField(TEXT("hazeBaseZ"))), FRotator::ZeroRotator);
		Haze->SetActorLabel(TEXT("Crucible_Haze"));
		UExponentialHeightFogComponent* Fog = Haze->GetComponent();
		Fog->SetFogDensity(Look.GetNumberField(TEXT("hazeDensity")));
		Fog->SetFogHeightFalloff(Look.GetNumberField(TEXT("hazeFalloff")));
		Fog->SetStartDistance(Look.GetNumberField(TEXT("hazeStartDistance")));
		Fog->SetFogInscatteringColor(ColourOf(Look, TEXT("hazeColour")));
		Fog->SetDirectionalInscatteringColor(ColourOf(Look, TEXT("hazeSunColour")));
		Fog->SetVolumetricFog(false);

		APostProcessVolume* Grade = World.SpawnActor<APostProcessVolume>();
		Grade->SetActorLabel(TEXT("Crucible_Grade"));
		Grade->bUnbound = true;
		FPostProcessSettings& Settings = Grade->Settings;
		// Explicit manual exposure is independent of the project's extended-luminance toggle.
		Settings.bOverride_AutoExposureMethod = true;
		Settings.AutoExposureMethod = AEM_Manual;
		Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		Settings.AutoExposureApplyPhysicalCameraExposure = false;
		Settings.bOverride_AutoExposureBias = true;
		Settings.AutoExposureBias = -Look.GetNumberField(TEXT("exposureEV100"));
		Settings.bOverride_ColorSaturation = true;
		Settings.ColorSaturation = FVector4(1.0, 1.0, 1.0, Look.GetNumberField(TEXT("saturation")));
		Settings.bOverride_ColorContrast = true;
		Settings.ColorContrast = FVector4(1.0, 1.0, 1.0, Look.GetNumberField(TEXT("contrast")));
		Settings.bOverride_ColorGain = true;
		Settings.ColorGain = FVector4(ColourOf(Look, TEXT("gain")), 1.0);
		Settings.bOverride_BloomIntensity = true;
		Settings.BloomIntensity = Look.GetNumberField(TEXT("bloom"));
		Settings.bOverride_VignetteIntensity = true;
		Settings.VignetteIntensity = Look.GetNumberField(TEXT("vignette"));
		Settings.bOverride_AmbientOcclusionIntensity = true;
		Settings.AmbientOcclusionIntensity = Look.GetNumberField(TEXT("ambientOcclusion"));
	}
}

namespace VeyraWorldBuild
{
	void ReviewScene(UWorld& World, const FVeyraWorldTuning& Tuning, const FJsonObject& Style)
	{
		Light(World, Style);
		const FVeyraBattlegroundLayout& Layout = Tuning.Layout;
		const auto GroundAt = [&World, &Layout](const FVector2D& Point) {
			FHitResult Hit;
			return VeyraGround::Find(World, Point, Layout.Surface.MaxZ, Layout.Surface.MinZ, Hit) ? Hit.ImpactPoint.Z : 0.0;
		};

		// Beauty views: a raised three-quarter view for art review, and a top-down overview.
		const auto Beauty = [&](const FString& Name, const FVector2D& Point, bool bReverse = false, bool bOverview = false) {
			const double Height = Style.GetNumberField(bOverview ? TEXT("overviewHeight") : TEXT("reviewHeight"));
			const FRotator Rotation(bOverview ? -90.0 : Style.GetNumberField(TEXT("reviewPitch")), Style.GetNumberField(TEXT("reviewYaw")) + (bReverse ? 180.0 : 0.0), 0.0);
			const FVector Target(Point, GroundAt(Point));
			ACameraActor* View = World.SpawnActor<ACameraActor>(Target - Rotation.Vector() * (Height / -Rotation.Vector().Z), Rotation);
			View->SetActorLabel(TEXT("Review_") + Name);
			View->Tags.Add(TEXT("Veyra.ReviewCamera"));
			View->GetCameraComponent()->FieldOfView = Style.GetNumberField(TEXT("reviewFOV"));
		};
		// Gameplay views: exactly the player's camera (Veyra Camera settings), centred on the point a body stands at.
		const UVeyraCameraSettings& Player = *GetDefault<UVeyraCameraSettings>();
		const auto Play = [&](const FString& Name, const FVector2D& Point) {
			const FRotator Rotation(Player.PitchDegrees, 0.0, 0.0);
			const FVector Target(Point, GroundAt(Point));
			ACameraActor* View = World.SpawnActor<ACameraActor>(Target - Rotation.Vector() * Player.Distance, Rotation);
			View->SetActorLabel(TEXT("Play_") + Name);
			View->Tags.Add(TEXT("Veyra.ReviewCamera"));
			View->Tags.Add(TEXT("Veyra.GameplayCamera"));
		};

		Beauty(TEXT("Overview"), FVector2D::ZeroVector, false, true);
		for (const FVeyraLaneLayout& Lane : Layout.Lanes)
		{
			const double Length = VeyraLayout::Length(Lane.Points);
			const FVector2D Middle = VeyraLayout::PointAlong(Lane.Points, Length / 2.0);
			const FString Name = UEnum::GetValueAsString(Lane.Lane).RightChop(FString(TEXT("EVeyraLane::")).Len());
			Beauty(Name + TEXT("_A"), Middle);
			Beauty(Name + TEXT("_B"), Middle, true);
			// Both teams' outer Spires and the lane's middle, as each team plays it.
			const double Outer = Lane.InhibitorDistance + (Lane.SpireDistances.IsEmpty() ? 0.0 : Lane.SpireDistances.Last());
			Play(Name + TEXT("_SpireA"), VeyraLayout::PointAlong(Lane.Points, Outer));
			Play(Name + TEXT("_SpireB"), VeyraLayout::PointAlong(Lane.Points, Length - Outer));
			Play(Name + TEXT("_Middle"), Middle);
		}
		// Where each lane crosses the river, and the river's widest bends.
		const FVeyraRiverShape& River = VeyraRiver::ShapeOf(Layout);
		for (const FVeyraLaneLayout& Lane : Layout.Lanes)
		{
			const double Length = VeyraLayout::Length(Lane.Points);
			for (double Along = 0.0; Along < Length; Along += 100.0)
			{
				const FVector2D Point = VeyraLayout::PointAlong(Lane.Points, Along);
				if (River.IsWater(Point))
				{
					Play(TEXT("Crossing_") + UEnum::GetValueAsString(Lane.Lane).RightChop(FString(TEXT("EVeyraLane::")).Len()), Point);
					break;
				}
			}
		}
		const TArray<FVeyraRiverSample>& Main = River.GetChannels()[0].Samples;
		Play(TEXT("RiverBend_A"), Main[Main.Num() * 3 / 4].Point);
		Play(TEXT("RiverBend_B"), Main[Main.Num() / 4].Point);
		for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
		{
			const FString Side = Team == EVeyraTeam::A ? TEXT("A") : TEXT("B");
			const FVector2D Well = VeyraLayout::ForTeam(VeyraLayout::ToVector(Layout.Base.PrimeWell), Team);
			Beauty(TEXT("PrimeWell_") + Side, Well, Team == EVeyraTeam::B);
			Play(TEXT("PrimeWell_") + Side, Well);
			Play(TEXT("BaseApproach_") + Side, Well * (1.0 - Layout.Base.PadRadius / Well.Size()));
			Beauty(TEXT("BaseApproach_") + Side, Well * (1.0 - Layout.Base.PadRadius / Well.Size()), Team == EVeyraTeam::B);
			for (int32 Index = 0; Index < Tuning.Wildlife.Camps.Num(); ++Index)
			{
				const FVector2D Camp = VeyraLayout::ForTeam(VeyraLayout::ToVector(Tuning.Wildlife.Camps[Index].Center), Team);
				Play(FString::Printf(TEXT("Jungle_%s%d"), *Side, Index), Camp);
			}
		}
		for (int32 Index = 0; Index < Tuning.FluxWells.Sites.Num(); ++Index)
		{
			const FVector2D Site = VeyraLayout::ToVector(Tuning.FluxWells.Sites[Index]);
			Beauty(FString::Printf(TEXT("FluxWell_%d"), Index), Site);
			Play(FString::Printf(TEXT("FluxWell_%d"), Index), Site);
		}
		if (!Layout.DenseFog.IsEmpty())
		{
			Play(TEXT("DenseFog"), VeyraLayout::ToVector(Layout.DenseFog[0].Center));
		}
		// The camp farthest from the centre: the outer jungle, and the densest foliage to benchmark.
		const FVeyraCampTuning* Outer = nullptr;
		for (const FVeyraCampTuning& Camp : Tuning.Wildlife.Camps)
		{
			if (!Outer || VeyraLayout::ToVector(Camp.Center).Size() > VeyraLayout::ToVector(Outer->Center).Size())
			{
				Outer = &Camp;
			}
		}
		if (Outer)
		{
			Play(TEXT("FoliageBenchmark"), VeyraLayout::ToVector(Outer->Center));
		}
		Play(TEXT("CombatBenchmark"), FVector2D::ZeroVector);
	}
}
