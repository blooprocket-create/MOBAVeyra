// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Engine/DeveloperSettings.h"

#include "VeyraShellStyleSettings.generated.h"

/** Where a Vanguard's face is in its hero illustration, for the portraits cropped from it. */
USTRUCT()
struct FVeyraVanguardPortrait
{
	GENERATED_BODY()

	/** The Vanguard's content ID, such as "cairn"; empty for the default. */
	UPROPERTY(Config, EditAnywhere, Category = "Art")
	FString Vanguard;

	/** The face's centre, from 0 to 1 across and down the illustration. */
	UPROPERTY(Config, EditAnywhere, Category = "Art")
	FVector2D Focus = FVector2D::ZeroVector;

	/** How much of the illustration's height a portrait shows, above 0 and at most 1. */
	UPROPERTY(Config, EditAnywhere, Category = "Art")
	float CropHeight = 0.0f;
};

/**
 * How the shell's screens and the in-match menu look (ADR-010 §4): grey-box colours, text sizes and
 * spacing. Presentation, not tuning, stored in Config/DefaultGame.ini. Every value is required: the
 * shell logs the problems Validate finds and shows no screen.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Veyra Shell Style"))
class VEYRAUI_API UVeyraShellStyleSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Every problem with these settings, as "Field: message"; empty when the shell can use them. */
	TArray<FString> Validate() const;

	/** Behind every screen. */
	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor BackgroundColor = FLinearColor::Transparent;

	/** Behind cards, the team overview and the in-match menu. */
	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor PanelColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor TextColor = FLinearColor::Transparent;

	/** Secondary text: explanations, labels. */
	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor MutedTextColor = FLinearColor::Transparent;

	/** Titles and the countdown. */
	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor AccentColor = FLinearColor::Transparent;

	/** Behind a problem and its Retry. */
	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor ProblemColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor ButtonColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor ButtonHoveredColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor ButtonPressedColor = FLinearColor::Transparent;

	/** A button that cannot be used now. */
	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor ButtonDisabledColor = FLinearColor::Transparent;

	/** The hovered or chosen card. */
	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor SelectedColor = FLinearColor::Transparent;

	/** Over the match while its menu is open; translucent, so the match shows through. */
	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor MenuScrimColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Text", meta = (ClampMin = "1"))
	int32 TitleFontSize = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Text", meta = (ClampMin = "1"))
	int32 HeadingFontSize = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Text", meta = (ClampMin = "1"))
	int32 BodyFontSize = 0;

	/** The champion-select countdown. */
	UPROPERTY(Config, EditAnywhere, Category = "Text", meta = (ClampMin = "1"))
	int32 CountdownFontSize = 0;

	/** Between the screen's edge and its content, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "0"))
	float ScreenPadding = 0.0f;

	/** Between one element and the next, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "0"))
	float Spacing = 0.0f;

	/** Inside a button, around its label, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "0"))
	float ButtonPadding = 0.0f;

	/** A Vanguard or mode card, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float CardWidth = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float CardHeight = 0.0f;

	/** The in-match menu's width, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float MenuWidth = 0.0f;

	/** The shop's size, in slate units; it scrolls when its lists are longer. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float ShopWidth = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float ShopHeight = 0.0f;

	/** Champion select's names under portraits and statuses. */
	UPROPERTY(Config, EditAnywhere, Category = "Text", meta = (ClampMin = "1"))
	int32 SmallFontSize = 0;

	/** Multiplies the shown Vanguard's art behind champion select, darkening it so the text reads. */
	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor BackdropTint = FLinearColor::Transparent;

	/** The frame around the shown Vanguard's art, the player's own pick and the hovered portrait. */
	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor FrameColor = FLinearColor::Transparent;

	/** Rings the player's team's portraits. */
	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor AllyColor = FLinearColor::Transparent;

	/** Rings the enemy team's portraits. */
	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor EnemyColor = FLinearColor::Transparent;

	/** Inside a portrait or tile button, around its content, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "0"))
	float TilePadding = 0.0f;

	/** A roster tile's rounded corners, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "0"))
	float TileCornerRadius = 0.0f;

	/** The outline around portraits and the shown Vanguard's art, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "0"))
	float FrameWidth = 0.0f;

	/** A roster portrait across champion select's top, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float RosterTileSize = 0.0f;

	/** Each of the countdown's two draining bars, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float PickBarWidth = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float PickBarHeight = 0.0f;

	/** Each team's column of seats, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float SeatColumnWidth = 0.0f;

	/** A seat's round portrait, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float PortraitSize = 0.0f;

	/** Each of a seat's starting spells beside its portrait, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float SeatSpellSize = 0.0f;

	/** The shown Vanguard's framed art in the middle, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float SplashWidth = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float SplashHeight = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "0"))
	float SplashCornerRadius = 0.0f;

	/** A Flux Spell slot's tile along the bottom, and Lock In's height, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float SpellTileSize = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float LockInWidth = 0.0f;

	/** The Flux Spell picker, and each spell's tile in it, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float PickerWidth = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float PickerTileWidth = 0.0f;

	/** Where the Vanguards' hero textures are imported, such as "/Game/Veyra/UI/Vanguards" (VeyraShellArt). */
	UPROPERTY(Config, EditAnywhere, Category = "Art")
	FString VanguardArtFolder;

	/** Where a Vanguard's face is when VanguardPortraits does not say. */
	UPROPERTY(Config, EditAnywhere, Category = "Art")
	FVeyraVanguardPortrait DefaultPortrait;

	/** Where each Vanguard's face is in its hero illustration; one entry per Vanguard at most. */
	UPROPERTY(Config, EditAnywhere, Category = "Art")
	TArray<FVeyraVanguardPortrait> VanguardPortraits;
};
