// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shop/VeyraShopScreen.h"
#include "Shell/VeyraShellLook.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"
#include "Shell/VeyraShellArt.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellStyle.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "Styling/SlateTypes.h"
#include "Text/VeyraContentText.h"
#include "Tuning/VeyraItemsTuning.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "VeyraPlayerController.h"

#define LOCTEXT_NAMESPACE "VeyraShopScreen"

namespace
{
	using VeyraShellStyle::EVeyraShellSurface;
	using VeyraShellStyle::EVeyraShellText;

	const UVeyraShellStyleSettings& ShopStyle()
	{
		return *GetDefault<UVeyraShellStyleSettings>();
	}

	/** Gold as the shop shows it: whole, rounded down, as everywhere in the UI (Economy & Progression Bible §1). */
	FText PriceText(double Gold)
	{
		return FText::AsNumber(FMath::FloorToInt64(Gold));
	}

	/** A tier's heading (Item Bible §2). */
	FText TierHeading(int32 Tier)
	{
		switch (Tier)
		{
		case 1:
			return LOCTEXT("Components", "Components");
		case 2:
			return LOCTEXT("Assemblies", "Assemblies");
		case 3:
			return LOCTEXT("Masterworks", "Masterworks");
		case 4:
			return LOCTEXT("Mythicals", "Mythicals");
		default:
			return FText::Format(LOCTEXT("Tier", "Tier {0}"), FText::AsNumber(Tier));
		}
	}

	/** Two letters that stand for a name until it has an icon: its words' initials. */
	FText Initials(const FText& Name)
	{
		TArray<FString> Words;
		Name.ToString().ParseIntoArrayWS(Words);
		FString Letters;
		for (const FString& Word : Words)
		{
			if (Letters.Len() < 2 && !Word.IsEmpty() && FChar::IsAlpha(Word[0]))
			{
				Letters.AppendChar(FChar::ToUpper(Word[0]));
			}
		}
		return FText::FromString(Letters);
	}

	/**
	 * Text on one line, or wrapped at Width: never at the width the last layout left it, which a rebuilt
	 * line would measure a frame late, overlapping what follows.
	 */
	UTextBlock* MakeLine(UWidgetTree& Tree, const FText& Text, EVeyraShellText Role, float Width = 0.0f)
	{
		UTextBlock* Line = VeyraShellStyle::MakeText(Tree, Text, Role);
		Line->SetAutoWrapText(false);
		if (Width > 0.0f)
		{
			Line->SetWrapTextAt(Width);
		}
		return Line;
	}

	/** A grid of tiles that wraps at Width, known before any layout. */
	UWrapBox* MakeGrid(UWidgetTree& Tree, float Width)
	{
		UWrapBox* Grid = Tree.ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
		Grid->SetExplicitWrapSize(true);
		Grid->SetWrapSize(Width);
		return Grid;
	}
}

bool UVeyraShopScreen::Initialize()
{
	const bool bFirst = Super::Initialize();
	if (bFirst && WidgetTree && !WidgetTree->RootWidget)
	{
		// The shop floats over the match, which stays in view around it: the heading, then the quick-buy
		// panels, the catalog and the selected item side by side, then the foot. Only their contents are
		// rebuilt, so the catalog keeps its scroll.
		const UVeyraShellStyleSettings& Settings = ShopStyle();
		const float Gap = Settings.Spacing * 2.0f;
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(Settings.ShopWidth);
		Size->SetHeightOverride(Settings.ShopHeight);
		UBorder* Panel = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Raised, FMargin(Gap));
		UVerticalBox* Frame = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Frame);
		Size->AddChild(Panel);

		Heading = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		VeyraShellStyle::AddSpaced(*Frame, *Heading);
		VeyraShellStyle::AddSpaced(*Frame, *VeyraShellStyle::MakeRule(*WidgetTree));

		UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UVerticalBoxSlot* BodySlot = Frame->AddChildToVerticalBox(Body);
		BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		BodySlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, Settings.Spacing));

		USizeBox* QuickWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		QuickWidth->SetWidthOverride(Settings.ShopQuickWidth);
		UScrollBox* QuickScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		QuickScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
		QuickBuy = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		QuickScroll->AddChild(QuickBuy);
		QuickWidth->AddChild(QuickScroll);
		Body->AddChildToHorizontalBox(QuickWidth)->SetPadding(FMargin(0.0f, 0.0f, Gap, 0.0f));

		// The catalog under its search (SET-58; ADR-058 §1), which is built once and so keeps the keyboard.
		UVerticalBox* CatalogColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		SearchBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
		VeyraShellStyle::StyleTextField(*SearchBox, Settings.ButtonPadding);
		SearchBox->SetHintText(LOCTEXT("SearchHint", "Search items or stats"));
		SearchBox->OnTextChanged.AddUniqueDynamic(this, &UVeyraShopScreen::HandleSearchChanged);
		VeyraShellStyle::AddSpaced(*CatalogColumn, *SearchBox);
		UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		Catalog = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Scroll->AddChild(Catalog);
		CatalogColumn->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Body->AddChildToHorizontalBox(CatalogColumn)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		USizeBox* DetailsWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		DetailsWidth->SetWidthOverride(Settings.ShopDetailsWidth);
		UBorder* DetailsPanel = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Panel, FMargin(Settings.Spacing));
		Details = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		DetailsPanel->SetContent(Details);
		DetailsWidth->AddChild(DetailsPanel);
		Body->AddChildToHorizontalBox(DetailsWidth)->SetPadding(FMargin(Gap, 0.0f, 0.0f, 0.0f));

		VeyraShellStyle::AddSpaced(*Frame, *VeyraShellStyle::MakeRule(*WidgetTree));
		Foot = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Frame->AddChildToVerticalBox(Foot);
		WidgetTree->RootWidget = Size;
	}
	return bFirst;
}

void UVeyraShopScreen::Show(AVeyraPlayerController& InController, TFunction<void()> InClose)
{
	Controller = &InController;
	Close = MoveTemp(InClose);
	// Refusals from before the shop opened are not news.
	SeenRefusals = InController.GetShopRefusalCount();
	SeenBuybackRefusals = InController.GetBuybackRefusalCount();
	bBuilt = false;
	Refresh();
}

void UVeyraShopScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Refresh();
}

void UVeyraShopScreen::Refresh()
{
	const AVeyraPlayerController* Owner = Controller.Get();
	const APlayerState* Participant = Owner ? Owner->PlayerState.Get() : nullptr;
	if (!Participant)
	{
		return;
	}
	FVeyraShopView Latest = VeyraShopModel::Describe(*Participant);
	if (bBuilt && Latest == View)
	{
		if (Message)
		{
			Message->SetText(GetMessage());
		}
		return;
	}
	View = MoveTemp(Latest);
	Rebuild();
}

FText UVeyraShopScreen::GetMessage() const
{
	const AVeyraPlayerController* Owner = Controller.Get();
	if (Owner && Owner->GetBuybackRefusalCount() > SeenBuybackRefusals)
	{
		return VeyraShopModel::DescribeBuybackRefusal(Owner->GetLastBuybackRefusal());
	}
	return Owner && Owner->GetShopRefusalCount() > SeenRefusals ? VeyraShopModel::DescribeRefusal(Owner->GetLastShopRefusal()) : FText::GetEmpty();
}

FText UVeyraShopScreen::TileLabel(const FVeyraContentId& Item)
{
	return VeyraContentText::ItemName(Item);
}

FText UVeyraShopScreen::SlotLabel(int32 Index)
{
	return FText::Format(LOCTEXT("SlotTile", "Inventory {0}"), FText::AsNumber(Index + 1));
}

FText UVeyraShopScreen::ItemsTabLabel()
{
	return LOCTEXT("ItemsTab", "All Items");
}

FText UVeyraShopScreen::SpellsTabLabel()
{
	return LOCTEXT("SpellsTab", "Flux Spells");
}

FText UVeyraShopScreen::BuyLabel(const FVeyraContentId& Item, double Price)
{
	return FText::Format(LOCTEXT("Buy", "{0}   {1}"), VeyraContentText::ItemName(Item), PriceText(Price));
}

FText UVeyraShopScreen::SellLabel(int32 Index, double Value)
{
	return FText::Format(LOCTEXT("Sell", "Sell {0} for {1}"), FText::AsNumber(Index + 1), PriceText(Value));
}

FText UVeyraShopScreen::CancelLabel(int32 Index)
{
	return FText::Format(LOCTEXT("Cancel", "Cancel {0}"), FText::AsNumber(Index + 1));
}

FText UVeyraShopScreen::BuybackLabel(double Cost)
{
	return FText::Format(LOCTEXT("Buyback", "Buy Back   {0}"), PriceText(Cost));
}

FText UVeyraShopScreen::VisionToolName(EVeyraVisionTool Tool)
{
	switch (Tool)
	{
	case EVeyraVisionTool::PersistentWard:
		return LOCTEXT("PersistentWard", "Persistent Ward");
	case EVeyraVisionTool::Sweeper:
		return LOCTEXT("Sweeper", "Sweeper");
	case EVeyraVisionTool::QuickSight:
		return LOCTEXT("QuickSight", "Quick Sight");
	}
	return FText::GetEmpty();
}

FText UVeyraShopScreen::SwapLabel(int32 Slot, const FVeyraContentId& Spell)
{
	return FText::Format(LOCTEXT("Swap", "Slot {0}: {1}"), FText::AsNumber(Slot + 1), VeyraContentText::AbilityName(Spell));
}

void UVeyraShopScreen::Select(const FVeyraContentId& Item, int32 FromSlot)
{
	SelectedItem = Item;
	SelectedSlot = FromSlot;
	Rebuild();
}

void UVeyraShopScreen::Rebuild()
{
	if (!Heading)
	{
		return;
	}
	bBuilt = true;
	Buttons.Reset();
	// A chosen slot shows what it holds now.
	if (View.Slots.IsValidIndex(SelectedSlot) && View.Slots[SelectedSlot].Item.IsValid())
	{
		SelectedItem = View.Slots[SelectedSlot].Item;
	}
	BuildHeading();
	BuildQuickBuy();
	if (Tab == ETab::Items)
	{
		BuildCatalog();
	}
	else
	{
		BuildSpells();
	}
	BuildDetails();
	BuildFoot();
}

void UVeyraShopScreen::BuildHeading()
{
	Heading->ClearChildren();
	for (const ETab Each : { ETab::Items, ETab::Spells })
	{
		const FText Label = Each == ETab::Items ? ItemsTabLabel() : SpellsTabLabel();
		UVeyraShellButton* TabButton = UVeyraShellButton::MakeKind(*WidgetTree, EVeyraShellButtonKind::Tab, Label, [this, Each] {
			Tab = Each;
			Rebuild();
		}, true, Tab == Each);
		TabButton->KeepLabelOnOneLine();
		Buttons.Add(TabButton);
		VeyraShellStyle::AddSpaced(*Heading, *TabButton);
	}
	const FText Where = View.bAtShop ? LOCTEXT("AtShop", "Purchases arrive now.") : LOCTEXT("AwayFromShop", "Purchases wait for your fountain.");
	VeyraShellStyle::AddSpaced(*Heading, *MakeLine(*WidgetTree, Where, EVeyraShellText::Muted));
	Heading->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	// While dead, the way back into the fight leads the heading: its price now, or why not yet (§15).
	if (View.bBuybackShown)
	{
		const FText Why = VeyraShopModel::DescribeBuybackRefusal(View.Buyback.Refusal);
		if (!Why.IsEmpty())
		{
			VeyraShellStyle::AddSpaced(*Heading, *MakeLine(*WidgetTree, Why, EVeyraShellText::Muted));
		}
		AddKindButton(*Heading, EVeyraShellButtonKind::Primary, BuybackLabel(View.Buyback.Cost), [this] {
			if (AVeyraPlayerController* Player = Controller.Get())
			{
				Player->RequestBuyback();
			}
		}, View.Buyback.Refusal == EVeyraBuybackRefusal::None);
	}
	AddKindButton(*Heading, EVeyraShellButtonKind::Quiet, LOCTEXT("Close", "Close"), [this] {
		if (Close)
		{
			Close();
		}
	}, true);
}

void UVeyraShopScreen::BuildQuickBuy()
{
	// The quick-buy panels: what is bought again and again, and the inventory, always to hand.
	QuickBuy->ClearChildren();
	const UVeyraShellStyleSettings& Settings = ShopStyle();
	const float Tile = Settings.ShopMarkSize;
	const auto AddCategory = [this, Tile](UWrapBox& Grid, EVeyraItemCategory Category) {
		for (const FVeyraShopOffer& Offer : View.Offers)
		{
			if (Offer.Category == Category)
			{
				AddItemTile(Grid, Offer.Item, Tile);
			}
		}
	};

	// Consumables, with the vision tools beside them (Vision Bible §3).
	AddEyebrow(*QuickBuy, LOCTEXT("Consumables", "Consumables"));
	UWrapBox* Consumables = MakeGrid(*WidgetTree, Settings.ShopQuickWidth);
	AddCategory(*Consumables, EVeyraItemCategory::Consumable);
	if (View.bHasVisionTool)
	{
		for (const FVeyraShopVisionToolOffer& Offer : View.VisionToolOffers)
		{
			// The tool in the slot is not for sale again.
			if (Offer.Tool == View.VisionTool)
			{
				continue;
			}
			// A tool's tile swaps to it at once, as buying a trinket does; it waits for the fountain.
			const bool bAllowed = Offer.Refusal == EVeyraShopRefusal::None;
			AddTile(*Consumables, VisionToolName(Offer.Tool), VisionToolName(Offer.Tool), PriceText(View.VisionToolSwapCost), Tile, bAllowed, false,
				[this, Tool = Offer.Tool] {
					if (AVeyraPlayerController* Player = Controller.Get())
					{
						Player->RequestSwapVisionTool(Tool);
					}
				})->SetIsEnabled(bAllowed);
		}
	}
	VeyraShellStyle::AddSpaced(*QuickBuy, *Consumables);
	if (View.bHasVisionTool)
	{
		UTextBlock* Held = MakeLine(*WidgetTree, FText::Format(LOCTEXT("VisionToolHeld", "In the slot: {0}"), VisionToolName(View.VisionTool)), EVeyraShellText::Small,
			Settings.ShopQuickWidth);
		Held->SetColorAndOpacity(FSlateColor(Settings.MutedTextColor));
		VeyraShellStyle::AddSpaced(*QuickBuy, *Held);
	}

	AddEyebrow(*QuickBuy, LOCTEXT("Boots", "Boots"));
	UWrapBox* Boots = MakeGrid(*WidgetTree, Settings.ShopQuickWidth);
	AddCategory(*Boots, EVeyraItemCategory::Boots);
	VeyraShellStyle::AddSpaced(*QuickBuy, *Boots);

	// The inventory: choosing a slot shows its item, and the foot offers its sale.
	AddEyebrow(*QuickBuy, LOCTEXT("Inventory", "Inventory"));
	UWrapBox* Slots = MakeGrid(*WidgetTree, Settings.ShopQuickWidth);
	for (int32 Index = 0; Index < View.Slots.Num(); ++Index)
	{
		const FVeyraShopSlot& Shown = View.Slots[Index];
		const bool bHeld = Shown.Item.IsValid();
		const FText Under = bHeld && Shown.Count > 1 ? FText::Format(LOCTEXT("Stack", "x{0}"), FText::AsNumber(Shown.Count)) : FText::GetEmpty();
		AddTile(*Slots, SlotLabel(Index), bHeld ? VeyraContentText::ItemName(Shown.Item) : FText::GetEmpty(), Under, Tile, bHeld, SelectedSlot == Index,
			[this, Index, Item = Shown.Item] { Select(Item.IsValid() ? Item : SelectedItem, Index); },
			bHeld ? VeyraShellArt::ItemIconOf(Shown.Item.ToString()) : nullptr);
	}
	VeyraShellStyle::AddSpaced(*QuickBuy, *Slots);
}

void UVeyraShopScreen::BuildCatalog()
{
	// Every item as a tile, a group per tier, each with its price now; the rules are the server's.
	Catalog->ClearChildren();
	const UVeyraShellStyleSettings& Settings = ShopStyle();
	// What the panel's padding, the two gaps and the scroll bar leave between the side columns.
	const float Width = Settings.ShopWidth - Settings.ShopQuickWidth - Settings.ShopDetailsWidth - Settings.Spacing * 9.0f;
	UWrapBox* Grid = nullptr;
	int32 GridTier = INDEX_NONE;
	const FVeyraItemsTuning& Items = UVeyraItemsTuningSubsystem::Get();
	for (const FVeyraShopOffer& Offer : View.Offers)
	{
		if (!VeyraShopModel::MatchesSearch(Items, Offer.Item, Search))
		{
			continue;
		}
		if (!Grid || Offer.Tier != GridTier)
		{
			GridTier = Offer.Tier;
			AddEyebrow(*Catalog, TierHeading(Offer.Tier));
			Grid = MakeGrid(*WidgetTree, Width);
			VeyraShellStyle::AddSpaced(*Catalog, *Grid);
		}
		AddItemTile(*Grid, Offer.Item, Settings.ShopTileSize);
	}
	if (!Grid)
	{
		AddEyebrow(*Catalog, FText::Format(LOCTEXT("NoMatch", "No item matches \"{0}\"."), FText::FromString(Search.TrimStartAndEnd())));
	}
}

void UVeyraShopScreen::HandleSearchChanged(const FText& Text)
{
	Search = Text.ToString();
	if (Tab == ETab::Items)
	{
		Rebuild();
	}
}

void UVeyraShopScreen::SetSearch(const FString& InSearch)
{
	if (SearchBox)
	{
		// Through the field, as typing is.
		SearchBox->SetText(FText::FromString(InSearch));
	}
	HandleSearchChanged(FText::FromString(InSearch));
}

void UVeyraShopScreen::FocusSearch()
{
	if (Tab != ETab::Items)
	{
		Tab = ETab::Items;
		Rebuild();
	}
	if (SearchBox)
	{
		SearchBox->SetKeyboardFocus();
	}
}

bool UVeyraShopScreen::IsSearching() const
{
	return SearchBox && SearchBox->HasKeyboardFocus();
}

bool UVeyraShopScreen::LeaveSearch()
{
	if (!IsSearching())
	{
		return false;
	}
	// The keys are the match's again; the shop stays open until the next Escape.
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
	return true;
}

FReply UVeyraShopScreen::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Escape leaves the search first, before it reaches the menu's key, which would close the shop (SET-58).
	if (InKeyEvent.GetKey() == EKeys::Escape && LeaveSearch())
	{
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UVeyraShopScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Typed into the search, a key never casts, moves or buys in the match (SET-58).
	if (IsSearching())
	{
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UVeyraShopScreen::BuildSpells()
{
	// The Flux Spell slots: each swap costs Gold, at the fountain only (ADR-015 §6).
	Catalog->ClearChildren();
	AddEyebrow(*Catalog, FText::Format(LOCTEXT("FluxSpells", "Flux Spells (each swap {0})"), PriceText(View.SpellSwapCost)));
	for (int32 Index = 0; Index < View.SpellSlots.Num(); ++Index)
	{
		const FVeyraShopSpellSlot& Shown = View.SpellSlots[Index];
		FText Held = Shown.Spell.IsValid() ? VeyraContentText::AbilityName(Shown.Spell) : LOCTEXT("NoSpell", "Empty");
		if (Shown.bLocked)
		{
			Held = FText::Format(LOCTEXT("LockedSpell", "{0} (locked)"), Held);
		}
		VeyraShellStyle::AddSpaced(*Catalog, *MakeLine(*WidgetTree, FText::Format(LOCTEXT("SpellSlotLine", "Spell {0}: {1}"), FText::AsNumber(Index + 1), Held),
			EVeyraShellText::Heading));
		UWrapBox* Swaps = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
		for (const FVeyraShopSpellOffer& Offer : Shown.Offers)
		{
			if (Offer.Spell == Shown.Spell)
			{
				continue;
			}
			const bool bAllowed = Offer.Refusal == EVeyraShopRefusal::None;
			const TFunction<void()> Swap = [this, Index, Spell = Offer.Spell] {
				if (AVeyraPlayerController* Player = Controller.Get())
				{
					Player->RequestSwapFluxSpell(Index, Spell);
				}
			};
			// A spell with an icon shows it, as a tile named for the swap; one without, its name.
			const FText Name = VeyraContentText::AbilityName(Offer.Spell);
			if (UTexture2D* Icon = VeyraShellArt::AbilityIconOf(Offer.Spell.ToString()))
			{
				AddTile(*Swaps, SwapLabel(Index, Offer.Spell), Name, Name, ShopStyle().ShopTileSize, bAllowed, false, Swap, Icon)->SetIsEnabled(bAllowed);
			}
			else
			{
				AddKindButton(*Swaps, EVeyraShellButtonKind::Secondary, SwapLabel(Index, Offer.Spell), Swap, bAllowed);
			}
		}
		VeyraShellStyle::AddSpaced(*Catalog, *Swaps);
	}
}

void UVeyraShopScreen::BuildDetails()
{
	// The selected item, in the right-hand pane: what it builds into, its recipe, the one
	// purchase button, and what it gives.
	Details->ClearChildren();
	const UVeyraShellStyleSettings& Settings = ShopStyle();
	const float Width = Settings.ShopDetailsWidth - Settings.Spacing * 2.0f;
	const FVeyraItemsTuning& Tuning = UVeyraItemsTuningSubsystem::Get();
	const FVeyraItemDefinition* Definition = SelectedItem.IsValid() ? Tuning.Items.Find(SelectedItem) : nullptr;
	const FVeyraShopOffer* Offer = SelectedItem.IsValid() ? View.Offers.FindByPredicate([this](const FVeyraShopOffer& Each) { return Each.Item == SelectedItem; }) : nullptr;
	if (!Definition || !Offer)
	{
		UTextBlock* Hint = MakeLine(*WidgetTree, LOCTEXT("ChooseAnItem", "Choose an item to see its recipe, what it builds into and what it gives."),
			EVeyraShellText::Body, Width);
		Hint->SetColorAndOpacity(FSlateColor(Settings.MutedTextColor));
		VeyraShellStyle::AddSpaced(*Details, *Hint);
		return;
	}

	const TArray<FVeyraContentId> Upgrades = VeyraShopModel::BuildsInto(Tuning, SelectedItem);
	if (!Upgrades.IsEmpty())
	{
		AddEyebrow(*Details, LOCTEXT("BuildsInto", "Builds into"));
		UWrapBox* Into = MakeGrid(*WidgetTree, Width);
		for (const FVeyraContentId& Upgrade : Upgrades)
		{
			AddItemTile(*Into, Upgrade, Settings.ShopMarkSize);
		}
		VeyraShellStyle::AddSpaced(*Details, *Into);
		VeyraShellStyle::AddSpaced(*Details, *VeyraShellStyle::MakeRule(*WidgetTree));
	}

	// The recipe: the item, and beneath it what it is made from.
	UHorizontalBox* Top = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddItemTile(*Top, SelectedItem, Settings.ShopTileSize);
	Details->AddChildToVerticalBox(Top)->SetHorizontalAlignment(HAlign_Center);
	if (!Definition->Components.IsEmpty())
	{
		UHorizontalBox* From = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		for (const FVeyraContentId& Component : Definition->Components)
		{
			AddItemTile(*From, Component, Settings.ShopMarkSize);
		}
		UVerticalBoxSlot* FromSlot = Details->AddChildToVerticalBox(From);
		FromSlot->SetHorizontalAlignment(HAlign_Center);
		FromSlot->SetPadding(FMargin(0.0f, Settings.Spacing / 2.0f, 0.0f, 0.0f));
	}

	// The one purchase button, named for the item and its price, saying what pressing it would do.
	FText Shown = LOCTEXT("PurchaseItem", "Purchase Item");
	if (Offer->Refusal == EVeyraShopRefusal::NotEnoughGold)
	{
		Shown = LOCTEXT("NotEnoughGold", "Not Enough Gold");
	}
	else if (Offer->Refusal == EVeyraShopRefusal::MythicalTaken)
	{
		Shown = LOCTEXT("MythicalLocked", "Mythical Locked");
	}
	else if (Offer->Refusal != EVeyraShopRefusal::None)
	{
		Shown = LOCTEXT("ItemUnavailable", "Item Unavailable");
	}
	UVeyraShellButton* Purchase = AddNamedButton(*Details, EVeyraShellButtonKind::Primary, BuyLabel(Offer->Item, Offer->Price), Shown, [this, Item = Offer->Item] {
		if (AVeyraPlayerController* Player = Controller.Get())
		{
			Player->RequestBuyItem(Item);
		}
	}, Offer->Refusal == EVeyraShopRefusal::None);
	if (UVerticalBoxSlot* PurchaseSlot = Cast<UVerticalBoxSlot>(Purchase->Slot))
	{
		PurchaseSlot->SetHorizontalAlignment(HAlign_Fill);
		PurchaseSlot->SetPadding(FMargin(0.0f, Settings.Spacing, 0.0f, Settings.Spacing));
	}

	// What it is and gives: its name, its whole cost, its stats and its effect.
	VeyraShellStyle::AddSpaced(*Details, *MakeLine(*WidgetTree, VeyraContentText::ItemName(SelectedItem), EVeyraShellText::Heading, Width));
	UTextBlock* Cost = MakeLine(*WidgetTree, FText::Format(LOCTEXT("TotalCost", "Cost {0}"), PriceText(Offer->TotalCost)), EVeyraShellText::Body);
	Cost->SetColorAndOpacity(FSlateColor(Settings.PrimaryColor));
	VeyraShellStyle::AddSpaced(*Details, *Cost);
	if (const FText Stats = VeyraShopModel::DescribeStats(Definition->Stats); !Stats.IsEmpty())
	{
		VeyraShellStyle::AddSpaced(*Details, *MakeLine(*WidgetTree, Stats, EVeyraShellText::Body, Width));
	}
	if (const FText Effect = VeyraContentText::ItemDescription(SelectedItem); !Effect.IsEmpty())
	{
		UTextBlock* EffectLine = MakeLine(*WidgetTree, Effect, EVeyraShellText::Small, Width);
		EffectLine->SetColorAndOpacity(FSlateColor(Settings.MutedTextColor));
		VeyraShellStyle::AddSpaced(*Details, *EffectLine);
	}
	if (Offer->Refusal != EVeyraShopRefusal::None && Offer->Refusal != EVeyraShopRefusal::NotEnoughGold)
	{
		UTextBlock* Why = MakeLine(*WidgetTree, VeyraShopModel::DescribeRefusal(Offer->Refusal), EVeyraShellText::Small, Width);
		Why->SetColorAndOpacity(FSlateColor(Settings.AccentColor));
		VeyraShellStyle::AddSpaced(*Details, *Why);
	}
}

void UVeyraShopScreen::BuildFoot()
{
	// Sale and undo, what waits for the fountain, why the server refused, and Gold.
	Foot->ClearChildren();
	const UVeyraShellStyleSettings& Settings = ShopStyle();
	const FVeyraShopSlot* Chosen = View.Slots.IsValidIndex(SelectedSlot) && View.Slots[SelectedSlot].Item.IsValid() ? &View.Slots[SelectedSlot] : nullptr;
	if (Chosen)
	{
		AddKindButton(*Foot, EVeyraShellButtonKind::Secondary, SellLabel(SelectedSlot, Chosen->SaleValue), [this, Index = SelectedSlot] {
			if (AVeyraPlayerController* Player = Controller.Get())
			{
				Player->RequestSellItem(Index);
			}
		}, View.bAtShop);
	}
	else
	{
		AddKindButton(*Foot, EVeyraShellButtonKind::Secondary, LOCTEXT("SellNothing", "Sell"), [] {}, false);
	}
	AddKindButton(*Foot, EVeyraShellButtonKind::Secondary, LOCTEXT("Undo", "Undo"), [this] {
		if (AVeyraPlayerController* Player = Controller.Get())
		{
			Player->RequestUndoPurchase();
		}
	}, View.UndoSteps > 0);

	if (!View.Pending.IsEmpty())
	{
		VeyraShellStyle::AddSpaced(*Foot, *MakeLine(*WidgetTree, LOCTEXT("Pending", "Waiting for your fountain"), EVeyraShellText::Eyebrow));
		for (int32 Index = 0; Index < View.Pending.Num(); ++Index)
		{
			const FVeyraShopPending& Entry = View.Pending[Index];
			VeyraShellStyle::AddSpaced(*Foot, MakeMark(VeyraContentText::ItemName(Entry.Item), Settings.ShopMarkSize, true, Settings.HairlineColor,
				VeyraShellArt::ItemIconOf(Entry.Item.ToString())));
			AddKindButton(*Foot, EVeyraShellButtonKind::Quiet, CancelLabel(Index), [this, Index] {
				if (AVeyraPlayerController* Player = Controller.Get())
				{
					Player->RequestCancelPurchase(Index);
				}
			}, true);
		}
	}

	Message = MakeLine(*WidgetTree, GetMessage(), EVeyraShellText::Body);
	Message->SetColorAndOpacity(FSlateColor(Settings.AccentColor));
	UHorizontalBoxSlot* MessageSlot = Foot->AddChildToHorizontalBox(Message);
	MessageSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	MessageSlot->SetVerticalAlignment(VAlign_Center);
	MessageSlot->SetHorizontalAlignment(HAlign_Right);
	MessageSlot->SetPadding(FMargin(Settings.Spacing, 0.0f));

	UTextBlock* Gold = MakeLine(*WidgetTree, FText::Format(LOCTEXT("Gold", "Gold {0}"), PriceText(View.Gold)), EVeyraShellText::Heading);
	Gold->SetColorAndOpacity(FSlateColor(Settings.PrimaryColor));
	Foot->AddChildToHorizontalBox(Gold)->SetVerticalAlignment(VAlign_Center);
}

UVeyraShellButton* UVeyraShopScreen::AddItemTile(UPanelWidget& Parent, const FVeyraContentId& Item, float Size)
{
	const FVeyraShopOffer* Offer = View.Offers.FindByPredicate([&Item](const FVeyraShopOffer& Each) { return Each.Item == Item; });
	const bool bLit = Offer && Offer->Refusal == EVeyraShopRefusal::None;
	// A Mythical other than the participant's shows as locked for the match, not as a price (ADR-025 §2).
	const bool bLocked = Offer && Offer->Refusal == EVeyraShopRefusal::MythicalTaken;
	const FText Price = bLocked ? LOCTEXT("MythicalLockedTile", "Locked") : Offer ? PriceText(Offer->Price) : FText::GetEmpty();
	// Outlined when chosen from the catalog; a slot chosen in the inventory is outlined there instead.
	const bool bSelected = Item == SelectedItem && !View.Slots.IsValidIndex(SelectedSlot);
	const FText Name = VeyraContentText::ItemName(Item);
	UVeyraShellButton* Tile = AddTile(Parent, TileLabel(Item), Name, Price, Size, bLit, bSelected, [this, Item] { Select(Item, INDEX_NONE); },
		VeyraShellArt::ItemIconOf(Item.ToString()));
	// Hovering names the item and what it gives: initials alone can be alike.
	const FVeyraItemDefinition* Definition = UVeyraItemsTuningSubsystem::Get().Items.Find(Item);
	const FText Stats = Definition ? VeyraShopModel::DescribeStats(Definition->Stats) : FText::GetEmpty();
	Tile->SetToolTipText(Stats.IsEmpty() ? Name : FText::Format(LOCTEXT("TileTip", "{0}\n{1}"), Name, Stats));
	return Tile;
}

UVeyraShellButton* UVeyraShopScreen::AddTile(UPanelWidget& Parent, const FText& Label, const FText& Name, const FText& Under, float Size, bool bLit,
	bool bSelected, TFunction<void()> Action, UTexture2D* Icon)
{
	// A tile: the icon framed thinly, the frame lit when chosen, the price beneath in Gold's colour
	// while it can be bought and dimmed while it cannot.
	const UVeyraShellStyleSettings& Settings = ShopStyle();
	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Stack->AddChildToVerticalBox(&MakeMark(Name, Size, bLit, bSelected ? Settings.AccentColor : Settings.HairlineColor, Icon))->SetHorizontalAlignment(HAlign_Center);
	UTextBlock* Line = MakeLine(*WidgetTree, Under, EVeyraShellText::Small);
	Line->SetJustification(ETextJustify::Center);
	Line->SetColorAndOpacity(FSlateColor(bLit ? Settings.PrimaryColor : Settings.MutedTextColor));
	Stack->AddChildToVerticalBox(Line)->SetHorizontalAlignment(HAlign_Center);

	UVeyraShellButton* Button = UVeyraShellButton::MakeWithContent(*WidgetTree, Label, *Stack, MoveTemp(Action));
	// The tile's own padding only: a button's slot pads its content by default, which would cost the grid a column.
	if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Stack->Slot))
	{
		ContentSlot->SetPadding(FMargin(0.0f));
	}
	if (!Name.IsEmpty())
	{
		Button->SetToolTipText(Name);
	}
	FButtonStyle TileStyle = VeyraShellStyle::ButtonStyleFor(EVeyraShellButtonKind::Quiet, false);
	const FMargin Pad(Settings.TilePadding);
	TileStyle.SetNormalPadding(Pad);
	TileStyle.SetPressedPadding(Pad);
	Button->SetStyle(TileStyle);
	Buttons.Add(Button);
	VeyraShellStyle::AddSpaced(Parent, *Button);
	return Button;
}

UWidget& UVeyraShopScreen::MakeMark(const FText& Name, float Size, bool bLit, const FLinearColor& Edge, UTexture2D* Icon)
{
	const UVeyraShellStyleSettings& Settings = ShopStyle();
	if (Icon)
	{
		// The icon whole, its corners rounded like a tile's, and greyed while it cannot be had.
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Image->SetBrush(VeyraShellArt::Brush(Icon, FBox2f(FVector2f::ZeroVector, FVector2f::UnitVector), FVector2D(Size), Settings.ButtonCornerRadius,
			VeyraShellLook::Panel(Settings.SurfaceRaisedColor), Edge, 1.0f));
		if (!bLit)
		{
			Image->SetColorAndOpacity(Settings.ItemDimTint);
		}
		USizeBox* Square = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Square->SetWidthOverride(Size);
		Square->SetHeightOverride(Size);
		Square->AddChild(Image);
		return *Square;
	}
	UBorder* Mark = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Mark->SetBrush(FSlateRoundedBoxBrush(VeyraShellLook::Panel(Settings.SurfaceRaisedColor), Settings.ButtonCornerRadius, Edge, 1.0f));
	Mark->SetPadding(FMargin(0.0f));
	Mark->SetHorizontalAlignment(HAlign_Center);
	Mark->SetVerticalAlignment(VAlign_Center);
	UTextBlock* Letters = MakeLine(*WidgetTree, Initials(Name), EVeyraShellText::Heading);
	Letters->SetColorAndOpacity(FSlateColor(bLit ? Settings.AccentColor : Settings.MutedTextColor));
	Mark->SetContent(Letters);
	USizeBox* Square = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Square->SetWidthOverride(Size);
	Square->SetHeightOverride(Size);
	Square->AddChild(Mark);
	return *Square;
}

UTextBlock& UVeyraShopScreen::AddEyebrow(UPanelWidget& Parent, const FText& Text)
{
	UTextBlock* Line = MakeLine(*WidgetTree, Text, EVeyraShellText::Eyebrow);
	VeyraShellStyle::AddSpaced(Parent, *Line);
	return *Line;
}

UVeyraShellButton* UVeyraShopScreen::AddKindButton(UPanelWidget& Parent, EVeyraShellButtonKind Kind, const FText& Label, TFunction<void()> Action, bool bEnabled)
{
	UVeyraShellButton* Button = UVeyraShellButton::MakeKind(*WidgetTree, Kind, Label, MoveTemp(Action), bEnabled);
	Button->KeepLabelOnOneLine();
	Buttons.Add(Button);
	VeyraShellStyle::AddSpaced(Parent, *Button);
	return Button;
}

UVeyraShellButton* UVeyraShopScreen::AddNamedButton(UPanelWidget& Parent, EVeyraShellButtonKind Kind, const FText& Label, const FText& Shown,
	TFunction<void()> Action, bool bEnabled)
{
	UTextBlock* Text = MakeLine(*WidgetTree, Shown, VeyraShellStyle::LabelRoleFor(Kind));
	Text->SetJustification(ETextJustify::Center);
	if (!bEnabled)
	{
		Text->SetColorAndOpacity(FSlateColor(ShopStyle().MutedTextColor));
	}
	UVeyraShellButton* Button = UVeyraShellButton::MakeWithContent(*WidgetTree, Label, *Text, MoveTemp(Action), bEnabled);
	Button->SetStyle(VeyraShellStyle::ButtonStyleFor(Kind, false));
	Buttons.Add(Button);
	VeyraShellStyle::AddSpaced(Parent, *Button);
	return Button;
}

TArray<UVeyraShellButton*> UVeyraShopScreen::GetButtons() const
{
	TArray<UVeyraShellButton*> Out;
	for (const TObjectPtr<UVeyraShellButton>& Button : Buttons)
	{
		Out.Add(Button.Get());
	}
	return Out;
}

UVeyraShellButton* UVeyraShopScreen::FindButton(const FText& Label) const
{
	for (const TObjectPtr<UVeyraShellButton>& Button : Buttons)
	{
		if (Button && Button->GetLabel().ToString() == Label.ToString())
		{
			return Button.Get();
		}
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
