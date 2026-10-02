// Copyright © 2026 Wayfinder Studios. All rights reserved.

// The Collection, the top bar's account readout and the results screen's rewards (ADR-045 §8). The
// backend owns every level, balance, price and entitlement; these screens show them and ask.

#include "Blueprint/WidgetTree.h"
#include "Client/VeyraClientIntents.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Shell/VeyraProgressionModels.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellScreen.h"
#include "Shell/VeyraShellStyle.h"
#include "Shell/VeyraShellStyleSettings.h"

#define LOCTEXT_NAMESPACE "VeyraShell"

namespace
{
	using VeyraBackendProtocol::ECurrency;
	using VeyraShellStyle::EVeyraShellSurface;
	using VeyraShellStyle::EVeyraShellText;

	const UVeyraShellStyleSettings& CollectionStyle()
	{
		return *GetDefault<UVeyraShellStyleSettings>();
	}

	uint8 CollectionRole(EVeyraShellText Role)
	{
		return static_cast<uint8>(Role);
	}

	/** Adds Child to a vertical box so that it takes the room left. */
	void AddCollectionFilling(UPanelWidget& Parent, UWidget& Child)
	{
		if (UVerticalBox* Column = Cast<UVerticalBox>(&Parent))
		{
			Column->AddChildToVerticalBox(&Child)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		else
		{
			Parent.AddChild(&Child);
		}
	}

	/** The purchase a confirmation names: the Vanguard and the currency, in the screen's confirmation ID. */
	FString PurchaseConfirmId(const FString& VanguardId, ECurrency Currency)
	{
		return VanguardId + TEXT("|") + VeyraBackendProtocol::CurrencyName(Currency);
	}
}

void UVeyraShellScreen::AddProgressionReadout(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Bar)
{
	const FVeyraProgressionBarModel Model = VeyraProgressionModels::DescribeBar(Snapshot.Progression);
	if (!Model.bShown)
	{
		return;
	}
	UVerticalBox* Readout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBox* Level = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddText(*Level, Model.Level, CollectionRole(EVeyraShellText::Heading))->SetAutoWrapText(false);
	AddText(*Level, Model.XP, CollectionRole(EVeyraShellText::Muted))->SetAutoWrapText(false);
	Readout->AddChildToVerticalBox(Level);
	AddText(*Readout, Model.Currencies, CollectionRole(EVeyraShellText::Muted))->SetAutoWrapText(false);
	VeyraShellStyle::AddSpaced(Bar, *Readout);
}

void UVeyraShellScreen::BuildCollection(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent)
{
	const UVeyraShellStyleSettings& Style = CollectionStyle();
	const FVeyraCollectionModel Model = VeyraProgressionModels::DescribeCollection(Snapshot, Client->CanIssue(EVeyraClientIntent::PurchaseVanguard));
	AddText(Parent, LOCTEXT("CollectionEyebrow", "Your Vanguards"), CollectionRole(EVeyraShellText::Eyebrow));
	AddText(Parent, LOCTEXT("CollectionTitle", "Collection"), CollectionRole(EVeyraShellText::Display));
	if (!Model.Balance.IsEmpty())
	{
		AddText(Parent, Model.Balance, CollectionRole(EVeyraShellText::Muted));
	}
	if (!Model.Feedback.IsEmpty())
	{
		AddText(Parent, Model.Feedback, CollectionRole(EVeyraShellText::Body));
	}
	if (!Model.bLoaded)
	{
		AddText(Parent, LOCTEXT("CollectionReading", "Reading your Collection..."), CollectionRole(EVeyraShellText::Muted));
		return;
	}
	// An opened card's detail and Buy sit above the roster, which scrolls beneath.
	if (const FVeyraCollectionCard* Opened = Model.Cards.FindByPredicate([this](const FVeyraCollectionCard& Card) { return OpenCardId == CollectionCardKey(Card.VanguardId); }))
	{
		BuildCollectionDetail(Snapshot, *Opened, Parent);
	}
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	UWrapBox* Roster = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	Roster->SetInnerSlotPadding(FVector2D(Style.Spacing));
	Scroll->AddChild(Roster);
	AddCollectionFilling(Parent, *Scroll);
	// Every released Vanguard, whatever the player owns (Bible §4); its status says whether it can be played now.
	for (const FVeyraCollectionCard& Card : Model.Cards)
	{
		const TArray<TPair<FText, uint8>> Plate = {
			{ Card.Name, CollectionRole(EVeyraShellText::Heading) },
			{ Card.Status, CollectionRole(EVeyraShellText::Muted) },
			{ Card.MasteryShort, CollectionRole(EVeyraShellText::Muted) },
		};
		const FString Key = CollectionCardKey(Card.VanguardId);
		AddArtCard(*Roster, VeyraProgressionModels::CollectionCardLabel(Card.VanguardId), Card.VanguardId, Plate, [this, Key] { OpenCard(Key); }, true, OpenCardId == Key);
	}
}

void UVeyraShellScreen::BuildCollectionDetail(const FVeyraClientSnapshot& Snapshot, const FVeyraCollectionCard& Card, UPanelWidget& Parent)
{
	const UVeyraShellStyleSettings& Style = CollectionStyle();
	UBorder* Panel = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Raised, FMargin(Style.Spacing));
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Rows);
	AddText(*Rows, Card.Name, CollectionRole(EVeyraShellText::Title));
	AddText(*Rows, Card.Status, CollectionRole(EVeyraShellText::Body));
	for (const FText& Line : Card.Details)
	{
		AddText(*Rows, Line, CollectionRole(EVeyraShellText::Muted));
	}
	if (Card.bPurchasable)
	{
		const struct FOffer
		{
			ECurrency Currency;
			int64 Price;
			bool bCanBuy;
		} Offers[] = { { ECurrency::Flux, Card.PriceFlux, Card.bCanBuyWithFlux }, { ECurrency::RefinedFlux, Card.PriceRefinedFlux, Card.bCanBuyWithRefinedFlux } };
		const FOffer* Asked = nullptr;
		for (const FOffer& Offer : Offers)
		{
			if (Confirm == EVeyraShellConfirm::Purchase && ConfirmId == PurchaseConfirmId(Card.VanguardId, Offer.Currency))
			{
				Asked = &Offer;
			}
		}
		if (Asked)
		{
			// The purchase's question, naming the price, the currency and the balance (Bible §7); browsing never spends.
			AddText(*Rows, VeyraProgressionModels::ConfirmBuyPrompt(Card.VanguardId, Asked->Currency, Asked->Price, Snapshot.Progression), CollectionRole(EVeyraShellText::Body));
			UHorizontalBox* Answers = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			const FString VanguardId = Card.VanguardId;
			const ECurrency Currency = Asked->Currency;
			AddNamedButton(*Answers, EVeyraShellButtonKind::Primary, VeyraProgressionModels::ConfirmBuyLabel(Card.VanguardId, Asked->Currency, Asked->Price),
				LOCTEXT("ConfirmPurchase", "Buy"), [this, VanguardId, Currency] {
					Client->PurchaseVanguard(VanguardId, Currency);
					AskToConfirm(EVeyraShellConfirm::None, FString());
				},
				Asked->bCanBuy);
			AddNamedButton(*Answers, EVeyraShellButtonKind::Quiet, VeyraShellModels::CancelConfirmLabel(), LOCTEXT("CancelPurchase", "Cancel"),
				[this] { AskToConfirm(EVeyraShellConfirm::None, FString()); });
			VeyraShellStyle::AddSpaced(*Rows, *Answers);
		}
		else
		{
			UHorizontalBox* Buys = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			for (const FOffer& Offer : Offers)
			{
				const FString Id = PurchaseConfirmId(Card.VanguardId, Offer.Currency);
				AddKindButton(*Buys, EVeyraShellButtonKind::Quiet, VeyraProgressionModels::BuyLabel(Card.VanguardId, Offer.Currency, Offer.Price),
					[this, Id] { AskToConfirm(EVeyraShellConfirm::Purchase, Id); }, Offer.bCanBuy)
					->KeepLabelOnOneLine();
			}
			VeyraShellStyle::AddSpaced(*Rows, *Buys);
		}
	}
	VeyraShellStyle::AddSpaced(Parent, *Panel);
}

void UVeyraShellScreen::BuildRewards(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent)
{
	const FVeyraRewardsModel Model = VeyraProgressionModels::DescribeRewards(Snapshot.Result, Snapshot.RewardsWait);
	if (!Model.bShown)
	{
		return;
	}
	const UVeyraShellStyleSettings& Style = CollectionStyle();
	UBorder* Panel = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Raised, FMargin(Style.Spacing));
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Rows);
	// Account rewards and Mastery, apart from the match's own Gold, XP and Team Flux (Client & Platform Bible §3).
	AddText(*Rows, LOCTEXT("RewardsEyebrow", "Your rewards"), CollectionRole(EVeyraShellText::Eyebrow));
	UWrapBox* Lines = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	Lines->SetInnerSlotPadding(FVector2D(Style.Spacing, 0.0f));
	for (const FText& Line : Model.Lines)
	{
		AddText(*Lines, Line, CollectionRole(EVeyraShellText::Body))->SetAutoWrapText(false);
	}
	Rows->AddChildToVerticalBox(Lines);
	VeyraShellStyle::AddSpaced(Parent, *Panel);
}

#undef LOCTEXT_NAMESPACE
