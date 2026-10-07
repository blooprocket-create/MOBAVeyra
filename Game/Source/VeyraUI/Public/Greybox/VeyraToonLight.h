// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "VeyraToonLight.generated.h"

class UDirectionalLightComponent;
class UWorld;

/** The toon light a sun gives the generated bodies (ADR-068 §2). */
struct FVeyraToonSun
{
	/** Toward the sun, against the way its light travels. */
	FLinearColor ToSun = FLinearColor::Transparent;

	/** Its colour, as warm or cold as its temperature makes it, at its brightest channel's full strength. */
	FLinearColor Color = FLinearColor::White;
};

/**
 * The toon light every generated body is shaded by (ADR-068 §2): the settings' material parameter collection, lit by the
 * map's sun. The presentation lights it each frame; a review capture, which runs no presentation, lights it once.
 */
UCLASS()
class VEYRAUI_API UVeyraToonLight : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** What Sun gives the toon light: how bright a lit body reads is the toon material's own, not the sun's intensity. */
	static FVeyraToonSun Of(const UDirectionalLightComponent& Sun);

	/** World's brightest visible sun; null in a world without one. */
	static UDirectionalLightComponent* BrightestSun(const UWorld& World);

	/** Sets World's toon light to Sun's light; false when the world has no instance of the settings' collection. */
	static bool Apply(UWorld& World, const FVeyraToonSun& Sun);

	/**
	 * Lights the toon light of the world WorldContextObject is in by its brightest sun, as the presentation does; false
	 * without a sun or the collection. For the review captures.
	 */
	UFUNCTION(BlueprintCallable, Category = "Veyra|Toon", meta = (WorldContext = "WorldContextObject"))
	static bool LightBySun(const UObject* WorldContextObject);
};
