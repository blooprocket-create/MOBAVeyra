// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraShellLook.h"

#include "Greybox/VeyraGreyboxSettings.h"
#include "Settings/VeyraInterfacePreferences.h"
#include "Shell/VeyraShellStyleSettings.h"

namespace VeyraShellLook
{
namespace
{
	/** One client, one player: the menus share one look, as they share one style. */
	FVeyraShellLook& Shared()
	{
		static FVeyraShellLook Look;
		return Look;
	}
}

FVeyraShellLook For(const FVeyraInterfacePreferences& Preferences)
{
	FVeyraShellLook Look;
	const float* Scale = GetDefault<UVeyraShellStyleSettings>()->TextSizeScales.Find(Preferences.TextSize);
	Look.TextScale = Scale ? *Scale : 1.0f;
	Look.bOpaquePanels = Preferences.bReduceTransparency;
	Look.bStillAnimation = Preferences.bReduceUiAnimation;
	Look.bEnhancedFocus = Preferences.bEnhancedFocus;
	return Look;
}

const FVeyraShellLook& Current()
{
	return Shared();
}

bool Use(const FVeyraShellLook& Look)
{
	if (Shared() == Look)
	{
		return false;
	}
	Shared() = Look;
	return true;
}

void FollowPlayer(const UObject* WorldContext)
{
	Use(For(VeyraInterfacePreferences::Resolve(*GetDefault<UVeyraGreyboxSettings>(), VeyraInterfacePreferences::StoreOf(WorldContext))));
}

int32 ScaledFontSize(int32 Size)
{
	return FMath::Max(1, FMath::RoundToInt32(Size * Shared().TextScale));
}

FLinearColor Panel(const FLinearColor& Fill)
{
	return Shared().bOpaquePanels ? Fill.CopyWithNewOpacity(1.0f) : Fill;
}

FVeyraFocusEdge FieldFocusEdge()
{
	const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
	return Shared().bEnhancedFocus ? FVeyraFocusEdge{ Style.FocusOutlineColor, Style.FocusOutlineWidth } : FVeyraFocusEdge{ Style.AccentColor, 1.0f };
}
}

FVeyraScopedShellLook::FVeyraScopedShellLook(const FVeyraShellLook& Look)
	: Previous(VeyraShellLook::Current())
{
	VeyraShellLook::Use(Look);
}

FVeyraScopedShellLook::~FVeyraScopedShellLook()
{
	VeyraShellLook::Use(Previous);
}