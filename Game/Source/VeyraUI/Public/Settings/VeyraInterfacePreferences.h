// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Hud/VeyraCombatTextModel.h"
#include "Loading/VeyraLoadingModel.h"
#include "Misc/Optional.h"
#include "Units/VeyraUnit.h"

class FVeyraSettingsStore;
class UObject;
class UVeyraGreyboxSettings;

/** When a unit's overhead bar shows (Settings Bible §3.4; ADR-052 §2). It only hides bars: an unseen unit stays unseen. */
enum class EVeyraBarVisibility : uint8
{
	Always,
	/** Below full Health. */
	WhenDamaged,
	/** The player's attack target, or the unit under the cursor. */
	WhenTargeted,
	/** Below full Health, or fighting someone. */
	WhenEngaged,
};

/** What decides whether a unit's bar shows to the player. */
struct FVeyraBarFacts
{
	EVeyraUnitKind Kind = EVeyraUnitKind::Vanguard;
	/** On the player's side. */
	bool bAllied = false;
	bool bDamaged = false;
	bool bTargeted = false;
	bool bFighting = false;
};

/** The colours of each side as the player sees them (Settings Bible §4.1; ADR-055 §1): every side-coloured cue reads these. */
struct FVeyraSideColors
{
	/** The player's own Vanguard. */
	FLinearColor Own = FLinearColor::Transparent;
	FLinearColor Ally = FLinearColor::Transparent;
	FLinearColor Enemy = FLinearColor::Transparent;
	FLinearColor Neutral = FLinearColor::Transparent;
};

/** The HUD, the minimap and the in-match controls as the player set them (Settings Bible §3; ADR-024 §6). */
struct FVeyraInterfacePreferences
{
	/** The player's colour vision (SET-8; ADR-055 §1). */
	FVeyraSideColors SideColors;
	/** Interface Text Size's option (SET-62): the shell's text scales by its factor. */
	FString TextSize;
	/** Enhanced keyboard focus (SET-74), opaque panels (SET-75), still decorative motion (SET-65) and no flashing (SET-18). */
	bool bEnhancedFocus = false;
	bool bReduceTransparency = false;
	bool bReduceUiAnimation = false;
	bool bReduceFlashing = false;
	/** The connection and low-performance warnings (SET-21, SET-110; ADR-055 §5). */
	bool bConnectionWarning = true;
	bool bPerformanceWarning = true;
	/** The HUD deck's size, as a multiple of its designed size. */
	float HudScale = 1.0f;
	/** The minimap's side and its icons' sides, in pixels. */
	float MinimapSize = 0.0f;
	float MinimapVanguardIcon = 0.0f;
	float MinimapStructureIcon = 0.0f;
	float MinimapUnitIcon = 0.0f;
	/** Clicking the minimap moves the camera; right-clicking it sends the Vanguard (§3.2). */
	bool bMinimapClickMovesCamera = true;
	bool bMinimapRightClickMoves = true;
	/** How long a ping stays on the battleground and the minimap, in seconds. */
	float PingSeconds = 0.0f;
	/** The frame rate and ping readouts (§3.6). */
	bool bShowFps = false;
	bool bShowPing = false;
	/** The scoreboard's key switches it with each press instead of showing it while held (SET-56). */
	bool bScoreboardToggles = false;
	/** The cursor stays inside the game's window during a match (SET-83). */
	bool bConfineCursor = true;
	/** Leave Match asks Stay in Match / Leave Match first (SET-76; ADR-053 §1). */
	bool bConfirmLeaveMatch = true;
	/** A match found draws attention to a client in the background, and plays the match-ready sound (SET-50, SET-71; ADR-053 §2). */
	bool bBackgroundMatchNotification = true;
	bool bMatchReadySound = true;
	/** Which tips and lore the loading screen shows (SET-118; ADR-053 §3). */
	EVeyraLoadingContent LoadingContent = EVeyraLoadingContent::Both;
	/** After how long a play streak the break reminder shows, as its option; Off shows none (ADR-053 §4). */
	FString PlayReminder;
	/** The local player's indicator outline, in units: Standard or Thick (Settings Bible §3.3; ADR-041 §2). */
	float IndicatorThickness = 0.0f;
	/** The chat's type size (SET-66), its backdrop, clear for Transparent (SET-67), how long a line stays whole, and its timestamps (Settings Bible §5.2). */
	int32 ChatFontSize = 0;
	FLinearColor ChatBackdrop = FLinearColor::Transparent;
	float ChatFadeSeconds = 0.0f;
	bool bChatTimestamps = false;
	/** When Fluxborn bars show, on the player's side and the other, and when jungle creatures' do (Settings Bible §3.4; ADR-052 §2). */
	EVeyraBarVisibility AlliedFluxbornBars = EVeyraBarVisibility::Always;
	EVeyraBarVisibility EnemyFluxbornBars = EVeyraBarVisibility::Always;
	EVeyraBarVisibility JungleBars = EVeyraBarVisibility::Always;
	/** Which combat text numbers show, and how (Settings Bible §3.4; ADR-052 §1), with the HUD's timing. */
	FVeyraCombatTextOptions CombatText;
	/** Damage numbers in one colour rather than coded by type (Proposal 52). */
	bool bUniformDamageColors = false;
};

/** The player's interface settings over the developer's (UVeyraGreyboxSettings), apart from the engine. */
namespace VeyraInterfacePreferences
{
	/** The settings the interface reads, as the registry names them. */
	VEYRAUI_API const FVeyraContentId& HudScale();
	VEYRAUI_API const FVeyraContentId& MinimapScale();
	VEYRAUI_API const FVeyraContentId& MinimapIconScale();
	VEYRAUI_API const FVeyraContentId& MinimapClickMovesCamera();
	VEYRAUI_API const FVeyraContentId& MinimapRightClickMoves();
	VEYRAUI_API const FVeyraContentId& PingSeconds();
	VEYRAUI_API const FVeyraContentId& ShowFps();
	VEYRAUI_API const FVeyraContentId& ShowPing();
	VEYRAUI_API const FVeyraContentId& ScoreboardMode();
	VEYRAUI_API const FVeyraContentId& ConfineCursor();
	VEYRAUI_API const FVeyraContentId& ConfirmLeaveMatch();
	VEYRAUI_API const FVeyraContentId& BackgroundMatchNotification();
	VEYRAUI_API const FVeyraContentId& MatchReadySound();
	VEYRAUI_API const FVeyraContentId& LoadingContent();
	VEYRAUI_API const FVeyraContentId& PlayReminder();
	VEYRAUI_API const FVeyraContentId& IndicatorBoundary();
	VEYRAUI_API const FVeyraContentId& ChatTextSize();
	VEYRAUI_API const FVeyraContentId& ChatBackdrop();
	VEYRAUI_API const FVeyraContentId& ChatFadeSeconds();
	VEYRAUI_API const FVeyraContentId& ChatTimestamps();
	VEYRAUI_API const FVeyraContentId& AlliedFluxbornBars();
	VEYRAUI_API const FVeyraContentId& EnemyFluxbornBars();
	VEYRAUI_API const FVeyraContentId& JungleBars();
	VEYRAUI_API const FVeyraContentId& CombatTextDamageDealt();
	VEYRAUI_API const FVeyraContentId& CombatTextDamageReceived();
	VEYRAUI_API const FVeyraContentId& CombatTextHealing();
	VEYRAUI_API const FVeyraContentId& CombatTextShielding();
	VEYRAUI_API const FVeyraContentId& CombatTextCrits();
	VEYRAUI_API const FVeyraContentId& CombatTextDensity();
	VEYRAUI_API const FVeyraContentId& DamageNumberColors();
	VEYRAUI_API const FVeyraContentId& ConnectionWarning();
	VEYRAUI_API const FVeyraContentId& PerformanceWarning();
	VEYRAUI_API const FVeyraContentId& ColorVision();
	VEYRAUI_API const FVeyraContentId& AllyColor();
	VEYRAUI_API const FVeyraContentId& EnemyColor();
	VEYRAUI_API const FVeyraContentId& NeutralColor();
	VEYRAUI_API const FVeyraContentId& TextSize();
	VEYRAUI_API const FVeyraContentId& FocusIndicator();
	VEYRAUI_API const FVeyraContentId& ReduceTransparency();
	VEYRAUI_API const FVeyraContentId& ReduceUiAnimation();
	VEYRAUI_API const FVeyraContentId& ReduceFlashing();

	/**
	 * The side colours for Color Vision's option Vision (ADR-055 §1): Standard's, a preset's, or under Custom the named
	 * palette colours Ally, Enemy and Neutral, the player's own lightening their ally colour. Anything unknown is Standard.
	 */
	VEYRAUI_API FVeyraSideColors SideColorsFor(const UVeyraGreyboxSettings& Hud, const FString& Vision, const FString& Ally, const FString& Enemy,
		const FString& Neutral);

	/**
	 * The colours setting Id shows beside it in Settings (SET-8; ADR-055 §1): for Color Vision, the player's own, ally, enemy
	 * and neutral colours as Store now resolves them; for a custom side colour, the palette colour it holds; none otherwise.
	 */
	VEYRAUI_API TArray<FLinearColor> SwatchesFor(const UVeyraGreyboxSettings& Hud, const FVeyraSettingsStore& Store, const FVeyraContentId& Id);

	/** A bar setting's option as the HUD reads it; Always for anything else. */
	VEYRAUI_API EVeyraBarVisibility ParseBars(const FString& Option);

	/**
	 * Whether a unit's bar shows (ADR-052 §2): Fluxborn bars by side and jungle creatures' as the player set
	 * them; every other unit's, such as a Vanguard's or a structure's, whenever the unit is seen.
	 */
	VEYRAUI_API bool ShowsBar(const FVeyraInterfacePreferences& Preferences, const FVeyraBarFacts& Facts);

	/** The interface: Hud's, with the player's settings in Store in their place; Hud's alone without a store. */
	VEYRAUI_API FVeyraInterfacePreferences Resolve(const UVeyraGreyboxSettings& Hud, const FVeyraSettingsStore* Store);

	/** The player's settings in WorldContext's game instance; null where there are none. */
	VEYRAUI_API const FVeyraSettingsStore* StoreOf(const UObject* WorldContext);

	/** The readouts the player asked for, such as "60 FPS" and "35 ms"; empty for none. The ping shows only once known. */
	VEYRAUI_API FString DescribeReadouts(const FVeyraInterfacePreferences& Preferences, float FramesPerSecond, TOptional<float> PingMilliseconds);
}
