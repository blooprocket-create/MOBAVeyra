// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/Button.h"
#include "Templates/Function.h"

#include "VeyraShellButton.generated.h"

class UWidgetTree;

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

	/**
	 * A button in Tree showing Content, such as a portrait, instead of its label; Label still names it
	 * for FindButton, scripts and tests.
	 */
	static UVeyraShellButton* MakeWithContent(UWidgetTree& Tree, const FText& Label, UWidget& Content, TFunction<void()> Action, bool bEnabled = true,
		bool bSelected = false);

	const FText& GetLabel() const { return Label; }

	/** Clicks it as the player would: runs its action only if it is enabled. For tests and scripts. */
	void Press();

private:
	/** A button labelled Label that runs Action, before its style and content are set. */
	static UVeyraShellButton* Create(UWidgetTree& Tree, const FText& Label, TFunction<void()> Action, bool bEnabled);

	UFUNCTION()
	void HandleClicked();

	TFunction<void()> Action;
	FText Label;
};
