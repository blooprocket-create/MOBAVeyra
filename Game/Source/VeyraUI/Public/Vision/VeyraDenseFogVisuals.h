// Copyright © 2026 Wayfinder Studios. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VeyraDenseFogVisuals.generated.h"

class AActor;
class UWorld;
class UNiagaraSystem;
struct FVeyraSurfaceTuning;

/** Presentation only: the Vision system still owns membership and concealment. */
UCLASS(Config=Game, DefaultConfig)
class VEYRAUI_API UVeyraDenseFogVisualSettings : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(Config, EditAnywhere, Category="Dense Fog")
	float RadialDensity = 0.0f;
	UPROPERTY(Config, EditAnywhere, Category="Dense Fog")
	float HeightDensity = 0.0f;
	UPROPERTY(Config, EditAnywhere, Category="Dense Fog")
	float HeightFalloff = 0.0f;
	UPROPERTY(Config, EditAnywhere, Category="Dense Fog")
	FLinearColor Albedo = FLinearColor::Black;
	UPROPERTY(Config, EditAnywhere, Category="Dense Fog")
	TSoftObjectPtr<UNiagaraSystem> Wisps;
	UPROPERTY(Config, EditAnywhere, Category="Dense Fog")
	float WispLiftRatio = 0.0f;
};

/** Shared by runtime presentation and the editor's generated review world. */
namespace VeyraDenseFogVisuals
{
	VEYRAUI_API bool Add(AActor& Owner, const FVector2D& Center, double Radius, const FVeyraSurfaceTuning& Surface);
	VEYRAUI_API bool HasAuthoredVisuals(const UWorld& World);
	VEYRAUI_API FName AuthoredTag();
}
