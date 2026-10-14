// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/Slider.h"

#include "VeyraSettingsSlider.generated.h"

/**
 * A range setting's slider on the Settings page (ADR-083 §3), with native callbacks: OnMoved as its value changes, to show
 * the number it would set, and OnSettled when the player lets go, to set it. Setting the value only once it settles keeps the
 * page from rebuilding under the pointer while it is dragged.
 */
UCLASS()
class VEYRAUI_API UVeyraSettingsSlider : public USlider
{
	GENERATED_BODY()

public:
	/** Binds its callbacks; Moved and Settled receive its value, 0 to 1. */
	void Bind(TFunction<void(float)> Moved, TFunction<void(float)> Settled);

	/** Lets go of it as the player would, settling its value. For tests and scripts. */
	void Settle();

private:
	UFUNCTION()
	void HandleValueChanged(float InValue);

	UFUNCTION()
	void HandleCaptureEnd();

	TFunction<void(float)> OnMoved;
	TFunction<void(float)> OnSettled;
};
