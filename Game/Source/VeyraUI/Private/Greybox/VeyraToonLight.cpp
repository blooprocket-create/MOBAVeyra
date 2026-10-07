// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraToonLight.h"

#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"

FVeyraToonSun UVeyraToonLight::Of(const UDirectionalLightComponent& Sun)
{
	FVeyraToonSun Light;
	Light.ToSun = FLinearColor(-Sun.GetForwardVector());
	FLinearColor Color = Sun.GetLightColor();
	if (Sun.bUseTemperature)
	{
		Color *= FLinearColor::MakeFromColorTemperature(Sun.Temperature);
	}
	const float Brightest = Color.GetMax();
	Light.Color = Brightest > UE_KINDA_SMALL_NUMBER ? Color / Brightest : FLinearColor::White;
	Light.Color.A = 1.0f;
	return Light;
}

UDirectionalLightComponent* UVeyraToonLight::BrightestSun(const UWorld& World)
{
	UDirectionalLightComponent* Brightest = nullptr;
	// Only directional lights are walked.
	for (TActorIterator<ADirectionalLight> It(&World); It; ++It)
	{
		UDirectionalLightComponent* Candidate = Cast<UDirectionalLightComponent>(It->GetLightComponent());
		if (Candidate && Candidate->IsVisible() && (!Brightest || Candidate->Intensity > Brightest->Intensity))
		{
			Brightest = Candidate;
		}
	}
	return Brightest;
}

bool UVeyraToonLight::Apply(UWorld& World, const FVeyraToonSun& Sun)
{
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	UMaterialParameterCollectionInstance* Instance = World.GetParameterCollectionInstance(Settings.ToonLight.LoadSynchronous());
	if (!Instance)
	{
		return false;
	}
	Instance->SetVectorParameterValue(Settings.ToonSunDirectionParameter, Sun.ToSun);
	Instance->SetVectorParameterValue(Settings.ToonSunColorParameter, Sun.Color);
	return true;
}

bool UVeyraToonLight::LightBySun(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	const UDirectionalLightComponent* Sun = World ? BrightestSun(*World) : nullptr;
	return Sun && Apply(*World, Of(*Sun));
}
