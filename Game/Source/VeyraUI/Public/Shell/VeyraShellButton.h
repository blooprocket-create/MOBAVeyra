// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/Button.h"
#include "Templates/Function.h"

#include "VeyraShellButton.generated.h"

class UWidgetTree;

/** What a shell button is for, which sets how loudly it speaks (VeyraShellStyle). */
enum class EVeyraShellButtonKind : uint8
{
	/** An ordinary action: a smoked surface with an outline. */
	Secondary,
	/** The one action a screen leads to, such as Play, Accept or Lock In. */
	Primary,
	/** A page or view to move to: quiet, lit when it is the one shown. */
	Tab,
	/** A minor action, such as Quit or Decline: text until hovered. */
	Quiet,
};

/**
 * A shell button built in C++ (ADR-010 §4): a label, the shell's style, and a native action run when
 * it is clicked, so no widget Blueprint or per-button UFUNCTION is needed.
 */
UCLASS()
class VEYRAUI_API UVeyraShellButton : public UButton
{
	GENERATED_BODY()

public:
	/** A button in Tree labelled Label, which runs Action when clicked while enabled. */
	static UVeyraShellButton* Make(UWidgetTree& Tree, const FText& Label, TFunction<void()> Action, bool bEnabled = true, bool bSelected = false);

	/** A button of Kind in Tree labelled Label; as Make otherwise. */
	static UVeyraShellButton* MakeKind(UWidgetTree& Tree, EVeyraShellButtonKind Kind, const FText& Label, TFunction<void()> Action, bool bEnabled = true,
		bool bSelected = false);

	/**
	 * A button of Kind that shows Shown, a short text, while Label names it for FindButton, scripts,
	 * tests and its tooltip: as when several buttons read "Remove" and each names what it removes.
	 */
	static UVeyraShellButton* MakeKindNamed(UWidgetTree& Tree, EVeyraShellButtonKind Kind, const FText& Label, const FText& Shown, TFunction<void()> Action,
		bool bEnabled = true, bool bSelected = false);

	/**
	 * A button in Tree showing Content, such as a portrait, instead of its label; Label still names it
	 * for FindButton, scripts and tests.
	 */
	static UVeyraShellButton* MakeWithContent(UWidgetTree& Tree, const FText& Label, UWidget& Content, TFunction<void()> Action, bool bEnabled = true,
		bool bSelected = false);

	const FText& GetLabel() const { return Label; }

	/** Clicks it as the player would: runs its action only if it is enabled. For tests and scripts. */
	void Press();

	/**
	 * Keeps a text button's label on one line. Shell text wraps, and UMG wraps text in an auto-sized
	 * slot at its narrowest, so a short label would break at every space. Returns the button.
	 */
	UVeyraShellButton& KeepLabelOnOneLine();

private:
	/** A button labelled Label that runs Action, before its style and content are set. */
	static UVeyraShellButton* Create(UWidgetTree& Tree, const FText& Label, TFunction<void()> Action, bool bEnabled);

	UFUNCTION()
	void HandleClicked();

	TFunction<void()> Action;
	FText Label;
};
