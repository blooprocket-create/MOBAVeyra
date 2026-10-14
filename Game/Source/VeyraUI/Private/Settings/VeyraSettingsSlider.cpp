// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Settings/VeyraSettingsSlider.h"

void UVeyraSettingsSlider::Bind(TFunction<void(float)> Moved, TFunction<void(float)> Settled)
{
	OnMoved = MoveTemp(Moved);
	OnSettled = MoveTemp(Settled);
	OnValueChanged.AddUniqueDynamic(this, &UVeyraSettingsSlider::HandleValueChanged);
	OnMouseCaptureEnd.AddUniqueDynamic(this, &UVeyraSettingsSlider::HandleCaptureEnd);
	OnControllerCaptureEnd.AddUniqueDynamic(this, &UVeyraSettingsSlider::HandleCaptureEnd);
}

void UVeyraSettingsSlider::Settle()
{
	HandleCaptureEnd();
}

void UVeyraSettingsSlider::HandleValueChanged(float InValue)
{
	if (OnMoved)
	{
		OnMoved(InValue);
	}
}

void UVeyraSettingsSlider::HandleCaptureEnd()
{
	if (OnSettled)
	{
		OnSettled(GetValue());
	}
}
