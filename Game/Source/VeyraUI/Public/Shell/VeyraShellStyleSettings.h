// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Engine/DeveloperSettings.h"

#include "VeyraShellStyleSettings.generated.h"

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
};
