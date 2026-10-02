// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Camera/VeyraCameraPreferences.h"

#include "Input/VeyraCameraSettings.h"
#include "VeyraSettingsStore.h"

namespace VeyraCameraPreferences
{
namespace
{
	FVeyraContentId IdOf(const TCHAR* Text)
	{
		// The registry's validation and the tests keep these real.
		return FVeyraContentId::FromText(Text).GetValue();
	}

	/** A speed setting in Store as a multiple of the developer's speed; 1 where the registry has no such range. */
	double SpeedOf(const FVeyraSettingsStore& Store, const FVeyraContentId& Id, const UVeyraCameraSettings& View)
	{
		const TOptional<FVeyraSettingInfo> Setting = VeyraSettings::Find(Store.GetRegistry(), Id);
		if (!Setting.IsSet() || !Setting->Range)
		{
			return 1.0;
		}
		const FVeyraRangeSetting& Range = *Setting->Range;
		return SpeedScale(Store.GetNumber(Id), Range.Minimum, Range.Default, Range.Maximum, View.SpeedSettingSlowest, View.SpeedSettingFastest);
	}
}

const FVeyraContentId& DefaultMode()
{
	static const FVeyraContentId Id = IdOf(TEXT("camera_default_mode"));
	return Id;
}

const FVeyraContentId& MoveSpeed()
{
	static const FVeyraContentId Id = IdOf(TEXT("camera_move_speed"));
	return Id;
}

const FVeyraContentId& EdgeScroll()
{
	static const FVeyraContentId Id = IdOf(TEXT("camera_edge_scroll"));
	return Id;
}

const FVeyraContentId& EdgeScrollSpeed()
{
	static const FVeyraContentId Id = IdOf(TEXT("camera_edge_scroll_speed"));
	return Id;
}

const FVeyraContentId& EdgeZone()
{
	static const FVeyraContentId Id = IdOf(TEXT("camera_edge_zone"));
	return Id;
}

const FVeyraContentId& EdgeDelay()
{
	static const FVeyraContentId Id = IdOf(TEXT("camera_edge_delay"));
	return Id;
}

const FVeyraContentId& DragSensitivity()
{
	static const FVeyraContentId Id = IdOf(TEXT("camera_drag_sensitivity"));
	return Id;
}

const FVeyraContentId& ReturnOnRespawn()
{
	static const FVeyraContentId Id = IdOf(TEXT("camera_return_on_respawn"));
	return Id;
}

const FVeyraContentId& FreeWhileDead()
{
	static const FVeyraContentId Id = IdOf(TEXT("camera_free_while_dead"));
	return Id;
}

const FVeyraContentId& Zoom()
{
	static const FVeyraContentId Id = IdOf(TEXT("camera_zoom"));
	return Id;
}

TOptional<FVeyraZoomScale> ZoomScaleOf(const UVeyraCameraSettings& View, const FVeyraSettingsStore& Store)
{
	const TOptional<FVeyraSettingInfo> Setting = VeyraSettings::Find(Store.GetRegistry(), Zoom());
	if (!Setting.IsSet() || !Setting->Range)
	{
		return {};
	}
	FVeyraZoomScale Scale;
	Scale.Lowest = Setting->Range->Minimum;
	Scale.Default = Setting->Range->Default;
	Scale.Highest = Setting->Range->Maximum;
	Scale.Nearest = View.MinDistance;
	Scale.Standard = View.Distance;
	Scale.Farthest = View.MaxDistance;
	return Scale;
}

double ZoomDistance(const FVeyraZoomScale& Scale, double Level)
{
	if (Level >= Scale.Default)
	{
		const double Span = Scale.Highest - Scale.Default;
		return Span > 0.0 ? FMath::Lerp(Scale.Standard, Scale.Farthest, FMath::Min(Level - Scale.Default, Span) / Span) : Scale.Standard;
	}
	const double Span = Scale.Default - Scale.Lowest;
	return Span > 0.0 ? FMath::Lerp(Scale.Standard, Scale.Nearest, FMath::Min(Scale.Default - Level, Span) / Span) : Scale.Standard;
}

double ZoomLevel(const FVeyraZoomScale& Scale, double Distance)
{
	if (Distance >= Scale.Standard)
	{
		const double Span = Scale.Farthest - Scale.Standard;
		return Span > 0.0 ? FMath::Lerp(Scale.Default, Scale.Highest, FMath::Min(Distance - Scale.Standard, Span) / Span) : Scale.Default;
	}
	const double Span = Scale.Standard - Scale.Nearest;
	return Span > 0.0 ? FMath::Lerp(Scale.Default, Scale.Lowest, FMath::Min(Scale.Standard - Distance, Span) / Span) : Scale.Default;
}

double ZoomLevelAfter(const FVeyraZoomScale& Scale, double Level, int32 Notches, double ZoomStep)
{
	return ZoomLevel(Scale, VeyraCamera::Zoom(ZoomDistance(Scale, Level), Notches, ZoomStep, Scale.Nearest, Scale.Farthest));
}

double SpeedScale(double Value, double Minimum, double Default, double Maximum, double Slowest, double Fastest)
{
	// Evenly in ratio: each step up multiplies the speed by the same amount.
	if (Value >= Default)
	{
		const double Span = Maximum - Default;
		return Span > 0.0 ? FMath::Pow(Fastest, FMath::Min(Value - Default, Span) / Span) : 1.0;
	}
	const double Span = Default - Minimum;
	return Span > 0.0 ? FMath::Pow(Slowest, FMath::Min(Default - Value, Span) / Span) : 1.0;
}

FString ModeName(EVeyraCameraMode Mode)
{
	return StaticEnum<EVeyraCameraMode>()->GetNameStringByValue(static_cast<int64>(Mode));
}

TOptional<EVeyraCameraMode> ParseMode(const FString& Name)
{
	const int64 Value = StaticEnum<EVeyraCameraMode>()->GetValueByNameString(Name);
	return Value == INDEX_NONE ? TOptional<EVeyraCameraMode>() : TOptional<EVeyraCameraMode>(static_cast<EVeyraCameraMode>(Value));
}

FVeyraCameraPreferences Resolve(const UVeyraCameraSettings& View, const FVeyraSettingsStore* Store)
{
	FVeyraCameraPreferences Preferences;
	Preferences.DefaultMode = View.DefaultMode;
	Preferences.PanSpeed = View.PanSpeed;
	Preferences.EdgeScrollSpeed = View.EdgeScrollSpeed;
	Preferences.bEdgeScroll = View.bEdgeScroll;
	Preferences.EdgeScrollPixels = View.EdgeScrollPixels;
	Preferences.DragUnitsPerPixel = View.DragUnitsPerPixel;
	Preferences.Distance = View.Distance;
	if (!Store)
	{
		return Preferences;
	}
	if (const TOptional<FVeyraZoomScale> Scale = ZoomScaleOf(View, *Store))
	{
		Preferences.Distance = ZoomDistance(Scale.GetValue(), Store->GetNumber(Zoom()));
	}
	if (const TOptional<EVeyraCameraMode> Mode = ParseMode(Store->Get(DefaultMode())))
	{
		Preferences.DefaultMode = Mode.GetValue();
	}
	Preferences.PanSpeed = View.PanSpeed * SpeedOf(*Store, MoveSpeed(), View);
	Preferences.EdgeScrollSpeed = View.EdgeScrollSpeed * SpeedOf(*Store, EdgeScrollSpeed(), View);
	Preferences.DragUnitsPerPixel = View.DragUnitsPerPixel * SpeedOf(*Store, DragSensitivity(), View);
	Preferences.bEdgeScroll = Store->IsOn(EdgeScroll());
	if (const float* Pixels = View.EdgeZonePixels.Find(Store->Get(EdgeZone())))
	{
		Preferences.EdgeScrollPixels = *Pixels;
	}
	if (const float* Delay = View.EdgeDelaySeconds.Find(Store->Get(EdgeDelay())))
	{
		Preferences.EdgeDelaySeconds = *Delay;
	}
	Preferences.bReturnOnRespawn = Store->IsOn(ReturnOnRespawn());
	Preferences.bFreeWhileDead = Store->IsOn(FreeWhileDead());
	return Preferences;
}
}
