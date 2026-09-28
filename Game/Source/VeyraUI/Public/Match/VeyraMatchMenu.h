// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Templates/Function.h"
#include "VeyraMatchTypes.h"

#include "VeyraMatchMenu.generated.h"

class APlayerState;
class AVeyraPlayerController;
class UVerticalBox;
class UVeyraShellButton;

namespace VeyraMatchMenuModel
{
	/**
	 * Whether the menu offers End Custom Match: only in a practice match, and only to its host
	 * (ADR-010 §4, §7). The server checks the same before it ends anything.
	 */
	VEYRAUI_API bool CanEndCustomMatch(EVeyraMatchRules Rules, const APlayerState* Host, const APlayerState* Self);

	/**
	 * Whether the menu offers End Match (Developer): a standard match has no victory condition yet,
	 * so outside Shipping a developer may end it to reach its result (ADR-010). Shipping servers refuse
	 * the request anyway.
	 */
	VEYRAUI_API bool OffersDeveloperEnd(EVeyraMatchRules Rules);
}

/**
 * The in-match menu (ADR-010 §4), built in C++: Resume; for a practice match's host, End Custom Match
 * behind a confirmation; and outside Shipping, End Match (Developer) for a standard match, behind the
 * same confirmation. It asks the server through the player's controller and decides nothing.
 */
UCLASS()
class VEYRAUI_API UVeyraMatchMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Builds the menu's frame, before anything shows it. */
	virtual bool Initialize() override;

	/** Shows the menu for Controller's match. Close runs when the menu should close. */
	void Show(AVeyraPlayerController& InController, TFunction<void()> InClose);

	/** Every button on the menu, in the order built. For tests and scripts. */
	TArray<UVeyraShellButton*> GetButtons() const;

	/** The button labelled Label, or null. */
	UVeyraShellButton* FindButton(const FText& Label) const;

private:
	void Rebuild();
	UVeyraShellButton* AddButton(const FText& Label, TFunction<void()> Action);

	/** What waits for confirmation, if anything. */
	enum class EConfirming : uint8
	{
		Nothing,
		EndCustomMatch,
		DeveloperEnd,
	};

	TWeakObjectPtr<AVeyraPlayerController> Controller;
	TFunction<void()> Close;
	EConfirming Confirming = EConfirming::Nothing;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Content;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UVeyraShellButton>> Buttons;
};
