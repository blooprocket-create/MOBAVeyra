// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Settings/VeyraInterfacePreferences.h"

#include "Greybox/VeyraGreyboxSettings.h"
#include "VeyraSettingsStore.h"
#include "VeyraSettingsSubsystem.h"

namespace VeyraInterfacePreferences
{
namespace
{
	FVeyraContentId IdOf(const TCHAR* Text)
	{
		// The registry's validation and the tests keep these real.
		return FVeyraContentId::FromText(Text).GetValue();
	}

	/** A percentage setting as a multiple. */
	float Share(const FVeyraSettingsStore& Store, const FVeyraContentId& Id)
	{
		constexpr float PerCent = 100.0f;
		return static_cast<float>(Store.GetNumber(Id)) / PerCent;
	}

	/** The scoreboard mode's option that switches it with each press (SET-56). */
	const TCHAR* const Toggle = TEXT("Toggle");

	/** The chat's text sizes and backdrops, as the registry names their options (SET-66, SET-67). */
	const TCHAR* const Large = TEXT("Large");
	const TCHAR* const ExtraLarge = TEXT("ExtraLarge");
	const TCHAR* const Transparent = TEXT("Transparent");
	const TCHAR* const HighContrast = TEXT("HighContrast");
}

const FVeyraContentId& HudScale()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_hud_scale"));
	return Id;
}

const FVeyraContentId& MinimapScale()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_minimap_scale"));
	return Id;
}

const FVeyraContentId& MinimapIconScale()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_minimap_icon_scale"));
	return Id;
}

const FVeyraContentId& MinimapClickMovesCamera()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_minimap_click_moves_camera"));
	return Id;
}

const FVeyraContentId& MinimapRightClickMoves()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_minimap_right_click_moves"));
	return Id;
}

const FVeyraContentId& PingSeconds()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_ping_seconds"));
	return Id;
}

const FVeyraContentId& ShowFps()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_show_fps"));
	return Id;
}

const FVeyraContentId& ShowPing()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_show_ping"));
	return Id;
}

const FVeyraContentId& ScoreboardMode()
{
	static const FVeyraContentId Id = IdOf(TEXT("controls_scoreboard_mode"));
	return Id;
}

const FVeyraContentId& ConfineCursor()
{
	static const FVeyraContentId Id = IdOf(TEXT("controls_confine_cursor"));
	return Id;
}

const FVeyraContentId& ChatTextSize()
{
	static const FVeyraContentId Id = IdOf(TEXT("communication_chat_text_size"));
	return Id;
}

const FVeyraContentId& ChatBackdrop()
{
	static const FVeyraContentId Id = IdOf(TEXT("communication_chat_backdrop"));
	return Id;
}

const FVeyraContentId& ChatFadeSeconds()
{
	static const FVeyraContentId Id = IdOf(TEXT("communication_chat_fade_seconds"));
	return Id;
}

const FVeyraContentId& ChatTimestamps()
{
	static const FVeyraContentId Id = IdOf(TEXT("communication_chat_timestamps"));
	return Id;
}

FVeyraInterfacePreferences Resolve(const UVeyraGreyboxSettings& Hud, const FVeyraSettingsStore* Store)
{
	FVeyraInterfacePreferences Preferences;
	Preferences.MinimapSize = Hud.MinimapSize;
	Preferences.MinimapVanguardIcon = Hud.MinimapVanguardIcon;
	Preferences.MinimapStructureIcon = Hud.MinimapStructureIcon;
	Preferences.MinimapUnitIcon = Hud.MinimapUnitIcon;
	Preferences.bMinimapClickMovesCamera = Hud.bMinimapClickMovesCamera;
	Preferences.bMinimapRightClickMoves = Hud.bMinimapRightClickMoves;
	Preferences.PingSeconds = Hud.PingSeconds;
	Preferences.ChatFontSize = Hud.ChatFontSize;
	Preferences.ChatBackdrop = Hud.ChatBackdropColor;
	Preferences.ChatFadeSeconds = Hud.ChatFadeSeconds;
	if (!Store)
	{
		return Preferences;
	}
	Preferences.HudScale = Share(*Store, HudScale());
	const float MinimapShare = Share(*Store, MinimapScale());
	const float IconShare = Share(*Store, MinimapIconScale());
	Preferences.MinimapSize = Hud.MinimapSize * MinimapShare;
	Preferences.MinimapVanguardIcon = Hud.MinimapVanguardIcon * IconShare;
	Preferences.MinimapStructureIcon = Hud.MinimapStructureIcon * IconShare;
	Preferences.MinimapUnitIcon = Hud.MinimapUnitIcon * IconShare;
	Preferences.bMinimapClickMovesCamera = Store->IsOn(MinimapClickMovesCamera());
	Preferences.bMinimapRightClickMoves = Store->IsOn(MinimapRightClickMoves());
	Preferences.PingSeconds = static_cast<float>(Store->GetNumber(PingSeconds()));
	Preferences.bShowFps = Store->IsOn(ShowFps());
	Preferences.bShowPing = Store->IsOn(ShowPing());
	Preferences.bScoreboardToggles = Store->Get(ScoreboardMode()) == Toggle;
	Preferences.bConfineCursor = Store->IsOn(ConfineCursor());
	const FString Size = Store->Get(ChatTextSize());
	Preferences.ChatFontSize = Size == Large ? Hud.ChatLargeFontSize : Size == ExtraLarge ? Hud.ChatExtraLargeFontSize : Hud.ChatFontSize;
	const FString Backdrop = Store->Get(ChatBackdrop());
	Preferences.ChatBackdrop = Backdrop == Transparent ? FLinearColor::Transparent : Backdrop == HighContrast ? Hud.ChatHighContrastBackdropColor : Hud.ChatBackdropColor;
	Preferences.ChatFadeSeconds = static_cast<float>(Store->GetNumber(ChatFadeSeconds()));
	Preferences.bChatTimestamps = Store->IsOn(ChatTimestamps());
	return Preferences;
}

const FVeyraSettingsStore* StoreOf(const UObject* WorldContext)
{
	const UVeyraSettingsSubsystem* Settings = UVeyraSettingsSubsystem::Get(WorldContext);
	return Settings ? &Settings->GetStore() : nullptr;
}

FString DescribeReadouts(const FVeyraInterfacePreferences& Preferences, float FramesPerSecond, TOptional<float> PingMilliseconds)
{
	TArray<FString> Parts;
	if (Preferences.bShowFps)
	{
		Parts.Add(FString::Printf(TEXT("%d FPS"), FMath::RoundToInt(FramesPerSecond)));
	}
	if (Preferences.bShowPing && PingMilliseconds.IsSet())
	{
		Parts.Add(FString::Printf(TEXT("%d ms"), FMath::RoundToInt(PingMilliseconds.GetValue())));
	}
	return FString::Join(Parts, TEXT("   "));
}
}
