// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Shop/VeyraShopModel.h"
#include "Templates/Function.h"

#include "VeyraShopScreen.generated.h"

class AVeyraPlayerController;
class UHorizontalBox;
class UPanelWidget;
class UTextBlock;
class UVerticalBox;
class UVeyraShellButton;
class UWidget;
enum class EVeyraShellButtonKind : uint8;

/**
 * The shop (Economy & Progression Bible §10–§12; ADR-012 §11), laid out as League's is:
 * - on the left, the quick-buy panels: consumables and the vision tools, boots, and the inventory;
 * - in the middle, every item as a tile with its price now, by tier; a tab holds the Flux Spell swaps;
 * - on the right, the selected item: what it builds into, its recipe, the one purchase button, and
 *   what it gives;
 * - along the foot, sale and undo, the purchases waiting for the fountain with their cancels, and Gold.
 * A tile selects its item, and the purchase button buys it; away from the fountain the purchase waits
 * there. While the Vanguard is dead, its buyback leads the heading. The shop asks the server through
 * the player's controller and decides nothing; it shows the server's refusals as they arrive.
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

	/**
	 * Reads the participant's state, and rebuilds the shop when what it shows has changed. The shop does
	 * so each frame it is painted; a script that reads it where nothing paints (a -nullrhi client) calls
	 * this first.
	 */
	void Refresh();

	/** What the shop shows now. */
	const FVeyraShopView& GetView() const { return View; }

	/** The item the right-hand pane shows; none until a tile is chosen. It stays chosen while the shop is shut. */
	const FVeyraContentId& GetSelectedItem() const { return SelectedItem; }

	/** Every button on the shop, in the order built. For tests and scripts. */
	TArray<UVeyraShellButton*> GetButtons() const;

	/** The button labelled Label, or null. */
	UVeyraShellButton* FindButton(const FText& Label) const;

	/** The label of a tile that selects Item: its name. */
	static FText TileLabel(const FVeyraContentId& Item);

	/** The label of the tile that selects inventory slot Index, from 0. */
	static FText SlotLabel(int32 Index);

	/** The tabs' labels: every item, and the Flux Spell swaps. */
	static FText ItemsTabLabel();
	static FText SpellsTabLabel();

	/** The label of the purchase button while Item, priced at Price, is selected. */
	static FText BuyLabel(const FVeyraContentId& Item, double Price);

	/** The label of the button that sells from the selected slot at Index, from 0, for Value. */
	static FText SellLabel(int32 Index, double Value);

	/** The label of the button that cancels the pending purchase at Index, from 0. */
	static FText CancelLabel(int32 Index);

	/** The label of the button that swaps Spell into Flux Spell slot Slot, from 0. */
	static FText SwapLabel(int32 Slot, const FVeyraContentId& Spell);

	/** The label of the button that buys the dead Vanguard back for Cost (§15). */
	static FText BuybackLabel(double Cost);

	/** A vision tool's name, as the shop and HUD show it, and the label of the tile that swaps to it. */
	static FText VisionToolName(EVeyraVisionTool Tool);

	/** The line that says why the server refused the last request; empty when none has been. */
	FText GetMessage() const;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	enum class ETab : uint8
	{
		Items,
		Spells,
	};

	void Rebuild();
	void BuildHeading();
	void BuildQuickBuy();
	void BuildCatalog();
	void BuildSpells();
	void BuildDetails();
	void BuildFoot();

	/** Selects Item for the right-hand pane; FromSlot is the inventory slot it was chosen from, if any. */
	void Select(const FVeyraContentId& Item, int32 FromSlot);

	/**
	 * A square tile of Size named Label, standing for Name until items have icons, with the line Under
	 * beneath it; lit when it can be had, outlined when selected. Pressing it runs Action.
	 */
	UVeyraShellButton* AddTile(UPanelWidget& Parent, const FText& Label, const FText& Name, const FText& Under, float Size, bool bLit, bool bSelected,
		TFunction<void()> Action, class UTexture2D* Icon = nullptr);

	/** A tile for Item from the catalog, priced as the view prices it; pressing it selects the item. */
	UVeyraShellButton* AddItemTile(UPanelWidget& Parent, const FVeyraContentId& Item, float Size);

	/**
	 * A square of Size that stands for Name, outlined in Edge: its Icon, dimmed while it cannot be had,
	 * or without one its words' initials on a raised surface.
	 */
	UWidget& MakeMark(const FText& Name, float Size, bool bLit, const FLinearColor& Edge, class UTexture2D* Icon = nullptr);

	/** A button of Kind showing Label, on one line. */
	UVeyraShellButton* AddKindButton(UPanelWidget& Parent, EVeyraShellButtonKind Kind, const FText& Label, TFunction<void()> Action, bool bEnabled);

	/** A button of Kind named Label for scripts and tests, showing Shown, as the purchase button shows its state. */
	UVeyraShellButton* AddNamedButton(UPanelWidget& Parent, EVeyraShellButtonKind Kind, const FText& Label, const FText& Shown, TFunction<void()> Action,
		bool bEnabled);

	UTextBlock& AddEyebrow(UPanelWidget& Parent, const FText& Text);

	TWeakObjectPtr<AVeyraPlayerController> Controller;
	TFunction<void()> Close;
	FVeyraShopView View;
	bool bBuilt = false;
	ETab Tab = ETab::Items;
	FVeyraContentId SelectedItem;
	int32 SelectedSlot = INDEX_NONE;

	/** The controller's shop and buyback refusal counts when the shop last showed one. */
	int32 SeenRefusals = 0;
	int32 SeenBuybackRefusals = 0;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> Heading;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> QuickBuy;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Catalog;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Details;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> Foot;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Message;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UVeyraShellButton>> Buttons;
};
