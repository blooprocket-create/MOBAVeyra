// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Shop/VeyraShopModel.h"
#include "Templates/Function.h"

#include "VeyraShopScreen.generated.h"

class AVeyraPlayerController;
class UTextBlock;
class UVerticalBox;
class UVeyraShellButton;

/**
 * The shop (Economy & Progression Bible §10–§12; ADR-012 §11), built in C++ as League's is laid out:
 * every item in columns by tier with its price now, the player's six slots with what each sells for,
 * undo, and the purchases waiting for the fountain with their cancel buttons. Buying away from the
 * fountain queues the purchase. It asks the server through the player's controller and decides
 * nothing; it shows the server's refusals as they arrive.
 */
UCLASS()
class VEYRAUI_API UVeyraShopScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Builds the shop's frame, before anything shows it. */
	virtual bool Initialize() override;

	/** Shows the shop for Controller's participant. Close runs when the shop should close. */
	void Show(AVeyraPlayerController& InController, TFunction<void()> InClose);

	/** What the shop shows now. */
	const FVeyraShopView& GetView() const { return View; }

	/** Every button on the shop, in the order built. For tests and scripts. */
	TArray<UVeyraShellButton*> GetButtons() const;

	/** The button labelled Label, or null. */
	UVeyraShellButton* FindButton(const FText& Label) const;

	/** The label of the button that buys Item, as the shop shows it with Price. */
	static FText BuyLabel(const FVeyraContentId& Item, double Price);

	/** The label of the button that sells from the slot at Index, from 0, for Value. */
	static FText SellLabel(int32 Index, double Value);

	/** The label of the button that cancels the pending purchase at Index, from 0. */
	static FText CancelLabel(int32 Index);

	/** The line that says why the server refused the last request; empty when none has been. */
	FText GetMessage() const;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** Reads the participant's state, and rebuilds the shop when what it shows has changed. */
	void Refresh();
	void Rebuild();
	UVeyraShellButton* AddButton(UVerticalBox& Parent, const FText& Label, TFunction<void()> Action, bool bEnabled);

	TWeakObjectPtr<AVeyraPlayerController> Controller;
	TFunction<void()> Close;
	FVeyraShopView View;
	bool bBuilt = false;

	/** The controller's shop refusal count when the shop last showed one. */
	int32 SeenRefusals = 0;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Content;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Message;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UVeyraShellButton>> Buttons;
};
