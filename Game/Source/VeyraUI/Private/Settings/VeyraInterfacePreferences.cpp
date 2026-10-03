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

	/** The indicator boundary's option that draws it thick (Settings Bible §3.3). */
	const TCHAR* const Thick = TEXT("Thick");

	/** Combat text's density and damage colours that differ from the default (ADR-052 §1). */
	const TCHAR* const Reduced = TEXT("Reduced");
	const TCHAR* const Uniform = TEXT("Uniform");

	/** Cooldown Precision's option that shows whole seconds (Proposal 44). */
	const TCHAR* const Whole = TEXT("Whole");

	/** Status Sorting's options other than By Category (Proposal 53). */
	const TCHAR* const ByRemainingDuration = TEXT("ByRemainingDuration");
	const TCHAR* const ByApplicationOrder = TEXT("ByApplicationOrder");

	/** Whether a bar shows under Visibility. */
	bool Shows(EVeyraBarVisibility Visibility, const FVeyraBarFacts& Facts)
	{
		switch (Visibility)
		{
		case EVeyraBarVisibility::Always:
			return true;
		case EVeyraBarVisibility::WhenDamaged:
			return Facts.bDamaged;
		case EVeyraBarVisibility::WhenTargeted:
			return Facts.bTargeted;
		case EVeyraBarVisibility::WhenEngaged:
			return Facts.bDamaged || Facts.bFighting;
		}
		return true;
	}
}

const FVeyraContentId& HudScale()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_hud_scale"));
	return Id;
}

const FVeyraContentId& AbilityBarScale()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_ability_bar_scale"));
	return Id;
}

const FVeyraContentId& VitalsScale()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_vitals_scale"));
	return Id;
}

const FVeyraContentId& ItemScale()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_item_scale"));
	return Id;
}

const FVeyraContentId& SpellScale()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_spell_scale"));
	return Id;
}

const FVeyraContentId& TeamPanelScale()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_team_panel_scale"));
	return Id;
}

const FVeyraContentId& ChatScale()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_chat_scale"));
	return Id;
}

const FVeyraContentId& CombatTextScale()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_combat_text_scale"));
	return Id;
}

const FVeyraContentId& OverheadBarScale()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_overhead_bar_scale"));
	return Id;
}

const FVeyraContentId& SafeAreaHorizontal()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_safe_area_horizontal"));
	return Id;
}

const FVeyraContentId& SafeAreaVertical()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_safe_area_vertical"));
	return Id;
}

const FVeyraContentId& CooldownNumbers()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_cooldown_numbers"));
	return Id;
}

const FVeyraContentId& CooldownSweep()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_cooldown_sweep"));
	return Id;
}

const FVeyraContentId& CooldownPrecision()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_cooldown_precision"));
	return Id;
}

const FVeyraContentId& StatusSort()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_status_sort"));
	return Id;
}

const FVeyraContentId& StatusHighContrast()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_status_high_contrast"));
	return Id;
}

const FVeyraContentId& StatusDurations()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_status_durations"));
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

const FVeyraContentId& ConfirmLeaveMatch()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_confirm_leave_match"));
	return Id;
}

const FVeyraContentId& BackgroundMatchNotification()
{
	static const FVeyraContentId Id = IdOf(TEXT("audio_background_match_notification"));
	return Id;
}

const FVeyraContentId& MatchReadySound()
{
	static const FVeyraContentId Id = IdOf(TEXT("audio_match_ready_sound"));
	return Id;
}

const FVeyraContentId& PlayReminder()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_play_reminder"));
	return Id;
}

const FVeyraContentId& LoadingContent()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_loading_tips"));
	return Id;
}

const FVeyraContentId& IndicatorBoundary()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_indicator_boundary"));
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

const FVeyraContentId& AlliedFluxbornBars()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_fluxborn_bars_allied"));
	return Id;
}

const FVeyraContentId& EnemyFluxbornBars()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_fluxborn_bars_enemy"));
	return Id;
}

const FVeyraContentId& JungleBars()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_jungle_bars"));
	return Id;
}

const FVeyraContentId& CombatTextDamageDealt()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_combat_text_damage_dealt"));
	return Id;
}

const FVeyraContentId& CombatTextDamageReceived()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_combat_text_damage_received"));
	return Id;
}

const FVeyraContentId& CombatTextHealing()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_combat_text_healing"));
	return Id;
}

const FVeyraContentId& CombatTextShielding()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_combat_text_shielding"));
	return Id;
}

const FVeyraContentId& CombatTextCrits()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_combat_text_crits"));
	return Id;
}

const FVeyraContentId& CombatTextDensity()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_combat_text_density"));
	return Id;
}

const FVeyraContentId& DamageNumberColors()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_damage_number_colors"));
	return Id;
}

const FVeyraContentId& ConnectionWarning()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_connection_warning"));
	return Id;
}

const FVeyraContentId& PerformanceWarning()
{
	static const FVeyraContentId Id = IdOf(TEXT("interface_performance_warning"));
	return Id;
}

const FVeyraContentId& ColorVision()
{
	static const FVeyraContentId Id = IdOf(TEXT("accessibility_color_vision"));
	return Id;
}

const FVeyraContentId& AllyColor()
{
	static const FVeyraContentId Id = IdOf(TEXT("accessibility_ally_color"));
	return Id;
}

const FVeyraContentId& EnemyColor()
{
	static const FVeyraContentId Id = IdOf(TEXT("accessibility_enemy_color"));
	return Id;
}

const FVeyraContentId& NeutralColor()
{
	static const FVeyraContentId Id = IdOf(TEXT("accessibility_neutral_color"));
	return Id;
}

const FVeyraContentId& TextSize()
{
	static const FVeyraContentId Id = IdOf(TEXT("accessibility_text_size"));
	return Id;
}

const FVeyraContentId& FocusIndicator()
{
	static const FVeyraContentId Id = IdOf(TEXT("accessibility_focus_indicator"));
	return Id;
}

const FVeyraContentId& ReduceTransparency()
{
	static const FVeyraContentId Id = IdOf(TEXT("accessibility_reduce_transparency"));
	return Id;
}

const FVeyraContentId& ReduceUiAnimation()
{
	static const FVeyraContentId Id = IdOf(TEXT("accessibility_reduce_ui_animation"));
	return Id;
}

const FVeyraContentId& ReduceFlashing()
{
	static const FVeyraContentId Id = IdOf(TEXT("accessibility_reduce_flashing"));
	return Id;
}

FVeyraSideColors SideColorsFor(const UVeyraGreyboxSettings& Hud, const FString& Vision, const FString& Ally, const FString& Enemy, const FString& Neutral)
{
	FVeyraSideColors Colors{ Hud.OwnColor, Hud.AllyColor, Hud.EnemyColor, Hud.NeutralColor };
	if (const FVeyraSideColorSet* Preset = Hud.ColorVisionPresets.Find(Vision))
	{
		return FVeyraSideColors{ Preset->Own, Preset->Ally, Preset->Enemy, Preset->Neutral };
	}
	if (Vision == TEXT("Custom"))
	{
		const FLinearColor* Allied = Hud.SideColorPalette.Find(Ally);
		const FLinearColor* Hostile = Hud.SideColorPalette.Find(Enemy);
		const FLinearColor* Neither = Hud.SideColorPalette.Find(Neutral);
		Colors.Ally = Allied ? *Allied : Colors.Ally;
		Colors.Enemy = Hostile ? *Hostile : Colors.Enemy;
		Colors.Neutral = Neither ? *Neither : Colors.Neutral;
		// The player stands apart from their allies in the same family of colour.
		Colors.Own = FMath::Lerp(Colors.Ally, FLinearColor::White, Hud.CustomOwnLightening);
		Colors.Own.A = Colors.Ally.A;
	}
	return Colors;
}

TArray<FLinearColor> SwatchesFor(const UVeyraGreyboxSettings& Hud, const FVeyraSettingsStore& Store, const FVeyraContentId& Id)
{
	if (Id == ColorVision())
	{
		const FVeyraSideColors Sides = SideColorsFor(Hud, Store.Get(ColorVision()), Store.Get(AllyColor()), Store.Get(EnemyColor()), Store.Get(NeutralColor()));
		return { Sides.Own, Sides.Ally, Sides.Enemy, Sides.Neutral };
	}
	if (Id == AllyColor() || Id == EnemyColor() || Id == NeutralColor())
	{
		if (const FLinearColor* Held = Hud.SideColorPalette.Find(Store.Get(Id)))
		{
			return { *Held };
		}
	}
	return {};
}

EVeyraBarVisibility ParseBars(const FString& Option)
{
	return Option == TEXT("WhenDamaged") ? EVeyraBarVisibility::WhenDamaged
		: Option == TEXT("WhenTargeted") ? EVeyraBarVisibility::WhenTargeted
		: Option == TEXT("WhenEngaged")	 ? EVeyraBarVisibility::WhenEngaged
										 : EVeyraBarVisibility::Always;
}

bool ShowsBar(const FVeyraInterfacePreferences& Preferences, const FVeyraBarFacts& Facts)
{
	switch (Facts.Kind)
	{
	case EVeyraUnitKind::Fluxborn:
		return Shows(Facts.bAllied ? Preferences.AlliedFluxbornBars : Preferences.EnemyFluxbornBars, Facts);
	case EVeyraUnitKind::Wildlife:
		return Shows(Preferences.JungleBars, Facts);
	default:
		return true;
	}
}

FVeyraInterfacePreferences Resolve(const UVeyraGreyboxSettings& Hud, const FVeyraSettingsStore* Store)
{
	FVeyraInterfacePreferences Preferences;
	Preferences.SideColors = FVeyraSideColors{ Hud.OwnColor, Hud.AllyColor, Hud.EnemyColor, Hud.NeutralColor };
	Preferences.TextSize = TEXT("Standard");
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
	Preferences.IndicatorThickness = Hud.IndicatorThickness;
	Preferences.CombatText.MergeSeconds = Hud.CombatTextMergeSeconds;
	Preferences.CombatText.ShowSeconds = Hud.CombatTextShowSeconds;
	if (!Store)
	{
		return Preferences;
	}
	Preferences.HudScale = Share(*Store, HudScale());
	Preferences.HudScales.AbilityBar = Share(*Store, AbilityBarScale());
	Preferences.HudScales.Vitals = Share(*Store, VitalsScale());
	Preferences.HudScales.Items = Share(*Store, ItemScale());
	Preferences.HudScales.Spells = Share(*Store, SpellScale());
	Preferences.HudScales.TeamPanels = Share(*Store, TeamPanelScale());
	Preferences.HudScales.Chat = Share(*Store, ChatScale());
	Preferences.HudScales.CombatText = Share(*Store, CombatTextScale());
	Preferences.HudScales.OverheadBars = Share(*Store, OverheadBarScale());
	Preferences.SafeArea = FVector2D(Share(*Store, SafeAreaHorizontal()), Share(*Store, SafeAreaVertical()));
	Preferences.Cooldowns.bNumbers = Store->IsOn(CooldownNumbers());
	Preferences.Cooldowns.bSweep = Store->IsOn(CooldownSweep());
	Preferences.Cooldowns.bTenths = Store->Get(CooldownPrecision()) != Whole;
	const FString Sort = Store->Get(StatusSort());
	Preferences.Statuses.Sort = Sort == ByRemainingDuration ? EVeyraStatusSort::ByRemainingDuration
		: Sort == ByApplicationOrder						? EVeyraStatusSort::ByApplicationOrder
															: EVeyraStatusSort::ByCategory;
	Preferences.Statuses.bHighContrast = Store->IsOn(StatusHighContrast());
	Preferences.Statuses.bDurations = Store->IsOn(StatusDurations());
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
	Preferences.bConfirmLeaveMatch = Store->IsOn(ConfirmLeaveMatch());
	Preferences.bBackgroundMatchNotification = Store->IsOn(BackgroundMatchNotification());
	Preferences.bMatchReadySound = Store->IsOn(MatchReadySound());
	Preferences.LoadingContent = VeyraLoadingModel::ParseContent(Store->Get(LoadingContent()));
	Preferences.PlayReminder = Store->Get(PlayReminder());
	Preferences.IndicatorThickness = Store->Get(IndicatorBoundary()) == Thick ? Hud.IndicatorThickThickness : Hud.IndicatorThickness;
	const FString Size = Store->Get(ChatTextSize());
	Preferences.ChatFontSize = Size == Large ? Hud.ChatLargeFontSize : Size == ExtraLarge ? Hud.ChatExtraLargeFontSize : Hud.ChatFontSize;
	const FString Backdrop = Store->Get(ChatBackdrop());
	Preferences.ChatBackdrop = Backdrop == Transparent ? FLinearColor::Transparent : Backdrop == HighContrast ? Hud.ChatHighContrastBackdropColor : Hud.ChatBackdropColor;
	Preferences.ChatFadeSeconds = static_cast<float>(Store->GetNumber(ChatFadeSeconds()));
	Preferences.bChatTimestamps = Store->IsOn(ChatTimestamps());
	Preferences.AlliedFluxbornBars = ParseBars(Store->Get(AlliedFluxbornBars()));
	Preferences.EnemyFluxbornBars = ParseBars(Store->Get(EnemyFluxbornBars()));
	Preferences.JungleBars = ParseBars(Store->Get(JungleBars()));
	Preferences.CombatText.bDamageDealt = Store->IsOn(CombatTextDamageDealt());
	Preferences.CombatText.bDamageReceived = Store->IsOn(CombatTextDamageReceived());
	Preferences.CombatText.bHealing = Store->IsOn(CombatTextHealing());
	Preferences.CombatText.bShielding = Store->IsOn(CombatTextShielding());
	Preferences.CombatText.bCritEmphasis = Store->IsOn(CombatTextCrits());
	Preferences.CombatText.bReduced = Store->Get(CombatTextDensity()) == Reduced;
	Preferences.bUniformDamageColors = Store->Get(DamageNumberColors()) == Uniform;
	Preferences.SideColors = SideColorsFor(Hud, Store->Get(ColorVision()), Store->Get(AllyColor()), Store->Get(EnemyColor()), Store->Get(NeutralColor()));
	Preferences.TextSize = Store->Get(TextSize());
	Preferences.bEnhancedFocus = Store->Get(FocusIndicator()) == TEXT("Enhanced");
	Preferences.bReduceTransparency = Store->IsOn(ReduceTransparency());
	Preferences.bReduceUiAnimation = Store->IsOn(ReduceUiAnimation());
	Preferences.bReduceFlashing = Store->IsOn(ReduceFlashing());
	Preferences.bConnectionWarning = Store->IsOn(ConnectionWarning());
	Preferences.bPerformanceWarning = Store->IsOn(PerformanceWarning());
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
