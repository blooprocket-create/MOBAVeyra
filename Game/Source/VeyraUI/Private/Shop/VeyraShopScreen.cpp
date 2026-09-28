// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shop/VeyraShopScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "GameFramework/PlayerState.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellStyle.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "Text/VeyraContentText.h"
#include "Tuning/VeyraItemsTuning.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "VeyraPlayerController.h"

#define LOCTEXT_NAMESPACE "VeyraShopScreen"

namespace
{
	/** Gold as the shop shows it: whole, rounded down, as everywhere in the UI (Economy & Progression Bible §1). */
	FText GoldText(double Gold)
	{
		return FText::AsNumber(FMath::FloorToInt64(Gold));
	}

	/** A tier's column heading (Item Bible §2). */
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
		default:
			return FText::Format(LOCTEXT("Tier", "Tier {0}"), FText::AsNumber(Tier));
		}
	}
}

bool UVeyraShopScreen::Initialize()
{
	const bool bFirst = Super::Initialize();
	if (bFirst && WidgetTree && !WidgetTree->RootWidget)
	{
		// The shop floats over the match, which stays in view around it.
		const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(Style.ShopWidth);
		Size->SetHeightOverride(Style.ShopHeight);
		UBorder* Panel = VeyraShellStyle::MakeBorder(*WidgetTree, Style.PanelColor, Style.Spacing);
		UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Scroll->AddChild(Content);
		Panel->SetContent(Scroll);
		Size->AddChild(Panel);
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
	return Owner && Owner->GetShopRefusalCount() > SeenRefusals ? VeyraShopModel::DescribeRefusal(Owner->GetLastShopRefusal()) : FText::GetEmpty();
}

FText UVeyraShopScreen::BuyLabel(const FVeyraContentId& Item, double Price)
{
	return FText::Format(LOCTEXT("Buy", "{0}   {1}"), VeyraContentText::ItemName(Item), GoldText(Price));
}

FText UVeyraShopScreen::SellLabel(int32 Index, double Value)
{
	return FText::Format(LOCTEXT("Sell", "Sell {0} for {1}"), FText::AsNumber(Index + 1), GoldText(Value));
}

FText UVeyraShopScreen::CancelLabel(int32 Index)
{
	return FText::Format(LOCTEXT("Cancel", "Cancel {0}"), FText::AsNumber(Index + 1));
}

void UVeyraShopScreen::Rebuild()
{
	if (!Content)
	{
		return;
	}
	bBuilt = true;
	Content->ClearChildren();
	Buttons.Reset();
	const FVeyraItemsTuning& Tuning = UVeyraItemsTuningSubsystem::Get();
	using VeyraShellStyle::EVeyraShellText;

	// The heading: Gold, where purchases arrive, undo and close.
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	VeyraShellStyle::AddSpaced(*Header, *VeyraShellStyle::MakeText(*WidgetTree, LOCTEXT("Title", "Shop"), EVeyraShellText::Title));
	VeyraShellStyle::AddSpaced(*Header, *VeyraShellStyle::MakeText(*WidgetTree, FText::Format(LOCTEXT("Gold", "Gold {0}"), GoldText(View.Gold)), EVeyraShellText::Heading));
	const FText Where = View.bAtShop ? LOCTEXT("AtShop", "Purchases arrive now.") : LOCTEXT("AwayFromShop", "Purchases wait for your fountain.");
	VeyraShellStyle::AddSpaced(*Header, *VeyraShellStyle::MakeText(*WidgetTree, Where, EVeyraShellText::Muted));
	UVerticalBox* HeaderButtons = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddButton(*HeaderButtons, LOCTEXT("Undo", "Undo"), [this] {
		if (AVeyraPlayerController* Player = Controller.Get())
		{
			Player->RequestUndoPurchase();
		}
	}, View.UndoSteps > 0);
	AddButton(*HeaderButtons, LOCTEXT("Close", "Close"), [this] {
		if (Close)
		{
			Close();
		}
	}, true);
	VeyraShellStyle::AddSpaced(*Header, *HeaderButtons);
	VeyraShellStyle::AddSpaced(*Content, *Header);

	Message = VeyraShellStyle::MakeText(*WidgetTree, GetMessage(), EVeyraShellText::Body);
	Message->SetColorAndOpacity(FSlateColor(GetDefault<UVeyraShellStyleSettings>()->AccentColor));
	VeyraShellStyle::AddSpaced(*Content, *Message);

	// The player's slots, each with its sale, and what waits for the fountain.
	VeyraShellStyle::AddSpaced(*Content, *VeyraShellStyle::MakeText(*WidgetTree, LOCTEXT("Inventory", "Inventory"), EVeyraShellText::Heading));
	UHorizontalBox* SlotRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (int32 Index = 0; Index < View.Slots.Num(); ++Index)
	{
		const FVeyraShopSlot& Shown = View.Slots[Index];
		UVerticalBox* Cell = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		FText Held = LOCTEXT("EmptySlot", "Empty");
		if (Shown.Item.IsValid())
		{
			Held = Shown.Count > 1 ? FText::Format(LOCTEXT("Stack", "{0} x{1}"), VeyraContentText::ItemName(Shown.Item), FText::AsNumber(Shown.Count))
								   : VeyraContentText::ItemName(Shown.Item);
		}
		VeyraShellStyle::AddSpaced(*Cell, *VeyraShellStyle::MakeText(*WidgetTree, FText::Format(LOCTEXT("SlotLine", "{0}: {1}"), FText::AsNumber(Index + 1), Held),
			Shown.Item.IsValid() ? EVeyraShellText::Body : EVeyraShellText::Muted));
		if (Shown.Item.IsValid())
		{
			AddButton(*Cell, SellLabel(Index, Shown.SaleValue), [this, Index] {
				if (AVeyraPlayerController* Player = Controller.Get())
				{
					Player->RequestSellItem(Index);
				}
			}, View.bAtShop);
		}
		SlotRow->AddChildToHorizontalBox(Cell)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	VeyraShellStyle::AddSpaced(*Content, *SlotRow);
	if (!View.Pending.IsEmpty())
	{
		VeyraShellStyle::AddSpaced(*Content, *VeyraShellStyle::MakeText(*WidgetTree, LOCTEXT("Pending", "Waiting for your fountain"), EVeyraShellText::Heading));
		UHorizontalBox* PendingRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		for (int32 Index = 0; Index < View.Pending.Num(); ++Index)
		{
			const FVeyraShopPending& Entry = View.Pending[Index];
			UVerticalBox* Cell = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			VeyraShellStyle::AddSpaced(*Cell, *VeyraShellStyle::MakeText(*WidgetTree,
				FText::Format(LOCTEXT("PendingLine", "{0}: {1} ({2})"), FText::AsNumber(Index + 1), VeyraContentText::ItemName(Entry.Item), GoldText(Entry.Paid)),
				EVeyraShellText::Body));
			AddButton(*Cell, CancelLabel(Index), [this, Index] {
				if (AVeyraPlayerController* Player = Controller.Get())
				{
					Player->RequestCancelPurchase(Index);
				}
			}, true);
			VeyraShellStyle::AddSpaced(*PendingRow, *Cell);
		}
		VeyraShellStyle::AddSpaced(*Content, *PendingRow);
	}

	// Every item, a column per tier, each with its price now and its stats.
	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBox* Column = nullptr;
	int32 ColumnTier = INDEX_NONE;
	for (const FVeyraShopOffer& Offer : View.Offers)
	{
		if (!Column || Offer.Tier != ColumnTier)
		{
			Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			ColumnTier = Offer.Tier;
			VeyraShellStyle::AddSpaced(*Column, *VeyraShellStyle::MakeText(*WidgetTree, TierHeading(Offer.Tier), EVeyraShellText::Heading));
			UHorizontalBoxSlot* ColumnSlot = Columns->AddChildToHorizontalBox(Column);
			ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			ColumnSlot->SetPadding(FMargin(0.0f, 0.0f, GetDefault<UVeyraShellStyleSettings>()->Spacing, 0.0f));
		}
		AddButton(*Column, BuyLabel(Offer.Item, Offer.Price), [this, Item = Offer.Item] {
			if (AVeyraPlayerController* Player = Controller.Get())
			{
				Player->RequestBuyItem(Item);
			}
		}, Offer.Refusal == EVeyraShopRefusal::None);
		const FVeyraItemDefinition* Definition = Tuning.Items.Find(Offer.Item);
		FText Detail = Definition ? VeyraShopModel::DescribeStats(Definition->Stats) : FText::GetEmpty();
		if (const FText Effect = VeyraContentText::ItemDescription(Offer.Item); !Effect.IsEmpty())
		{
			Detail = Detail.IsEmpty() ? Effect : FText::Format(LOCTEXT("StatsAndEffect", "{0}. {1}"), Detail, Effect);
		}
		if (Offer.Refusal != EVeyraShopRefusal::None && Offer.Refusal != EVeyraShopRefusal::NotEnoughGold)
		{
			Detail = FText::Format(LOCTEXT("Refused", "{0} ({1})"), Detail, VeyraShopModel::DescribeRefusal(Offer.Refusal));
		}
		if (!Detail.IsEmpty())
		{
			VeyraShellStyle::AddSpaced(*Column, *VeyraShellStyle::MakeText(*WidgetTree, Detail, EVeyraShellText::Muted));
		}
	}
	VeyraShellStyle::AddSpaced(*Content, *Columns);
}

UVeyraShellButton* UVeyraShopScreen::AddButton(UVerticalBox& Parent, const FText& Label, TFunction<void()> Action, bool bEnabled)
{
	UVeyraShellButton* Button = UVeyraShellButton::Make(*WidgetTree, Label, MoveTemp(Action), bEnabled);
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
