// Copyright © 2026 Wayfinder Studios. All rights reserved.

// The Collection, the top bar's account readout and the results screen's rewards (ADR-045 §8). The
// backend owns every level, balance, price and entitlement; these screens show them and ask.

#include "Blueprint/WidgetTree.h"
#include "Client/VeyraClientIntents.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Shell/VeyraProgressionModels.h"
#include "Shell/VeyraShellBrushes.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellLook.h"
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

	/** A fixed gap of Size slate units in a row. */
	void AddCollectionGap(UWidgetTree& Tree, UPanelWidget& Parent, float Size)
	{
		USpacer* Gap = Tree.ConstructWidget<USpacer>(USpacer::StaticClass());
		Gap->SetSize(FVector2D(Size, Size));
		Parent.AddChild(Gap);
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

bool UVeyraShellScreen::AddProgressionReadout(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Bar)
{
	const FVeyraProgressionBarModel Model = VeyraProgressionModels::DescribeBar(Snapshot.Progression);
	if (!Model.bShown)
	{
		return false;
	}
	const UVeyraShellStyleSettings& Style = CollectionStyle();
	UHorizontalBox* Account = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	// The level's number on its badge; the badge's tooltip says it in words.
	USizeBox* Badge = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Badge->SetWidthOverride(Style.LevelBadgeSize);
	Badge->SetHeightOverride(Style.LevelBadgeSize);
	UBorder* BadgeFace = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	BadgeFace->SetBrush(VeyraShellStyle::EdgedFace(VeyraShellLook::Panel(Style.SurfaceRaisedColor), Style.FrameColor));
	BadgeFace->SetHorizontalAlignment(HAlign_Center);
	BadgeFace->SetVerticalAlignment(VAlign_Center);
	BadgeFace->SetPadding(FMargin(0.0f));
	UTextBlock* Number = VeyraShellStyle::MakeText(*WidgetTree, Model.LevelNumber, EVeyraShellText::Heading);
	Number->SetFont(VeyraShellStyle::FontFor(EVeyraShellText::Countdown));
	FSlateFontInfo NumberFont = Number->GetFont();
	NumberFont.Size = VeyraShellLook::ScaledFontSize(Style.HeadingFontSize);
	Number->SetFont(NumberFont);
	Number->SetAutoWrapText(false);
	BadgeFace->SetContent(Number);
	BadgeFace->SetToolTipText(Model.Level);
	Badge->AddChild(BadgeFace);
	Account->AddChildToHorizontalBox(Badge)->SetVerticalAlignment(VAlign_Center);
	AddCollectionGap(*WidgetTree, *Account, Style.Spacing);
	// The player's name over the level's progress, and the XP under it.
	UVerticalBox* Who = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock* Name = AddText(*Who, FText::FromString(Snapshot.DisplayName), CollectionRole(EVeyraShellText::Heading));
	Name->SetAutoWrapText(false);
	Name->SetToolTipText(Model.Level);
	Who->AddChildToVerticalBox(VeyraShellStyle::MakeMeter(*WidgetTree, Model.Fraction, Style.AccentColor, Style.HairlineColor, Style.ProgressionBarWidth,
		Style.ProgressionBarHeight))->SetPadding(FMargin(0.0f, Style.ProgressionBarHeight, 0.0f, Style.ProgressionBarHeight));
	UTextBlock* Xp = AddText(*Who, FText::Format(LOCTEXT("TopBarLevelXp", "{0}  ·  {1}"), Model.Level, Model.XP), CollectionRole(EVeyraShellText::Small));
	Xp->SetAutoWrapText(false);
	Xp->SetColorAndOpacity(FSlateColor(Style.MutedTextColor));
	Account->AddChildToHorizontalBox(Who)->SetVerticalAlignment(VAlign_Center);
	AddCollectionGap(*WidgetTree, *Account, Style.ScreenPadding / 2.0f);
	// Each balance beside its mark: Flux filled in the accent, Refined Flux outlined in brass. Never Team Flux.
	const TPair<FText, FSlateBrush> Balances[] = {
		{ Model.Flux, VeyraShellBrushes::Diamond(Style.CurrencyGlyphSize, Style.AccentColor, FLinearColor::Transparent) },
		{ Model.RefinedFlux, VeyraShellBrushes::Diamond(Style.CurrencyGlyphSize, FLinearColor::Transparent, Style.FrameColor) },
	};
	for (const TPair<FText, FSlateBrush>& Balance : Balances)
	{
		Account->AddChildToHorizontalBox(VeyraShellStyle::MakeGlyph(*WidgetTree, Balance.Value))->SetVerticalAlignment(VAlign_Center);
		AddCollectionGap(*WidgetTree, *Account, Style.Spacing / 2.0f);
		UTextBlock* Amount = AddText(*Account, Balance.Key, CollectionRole(EVeyraShellText::Heading));
		Amount->SetAutoWrapText(false);
		Amount->SetToolTipText(Model.Currencies);
		AddCollectionGap(*WidgetTree, *Account, Style.Spacing);
	}
	VeyraShellStyle::AddSpaced(Bar, *Account);
	return true;
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
	// The roster's search and tabs (UX-19; ADR-058 §3): they narrow what shows, never what is owned.
	AddRosterSearch(Parent, { EVeyraRosterTab::All, EVeyraRosterTab::Owned, EVeyraRosterTab::FreeRotation });
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
	// Every card is built, and the search and tab show some of them, so typing narrows them in place.
	for (const FVeyraCollectionCard& Card : Model.Cards)
	{
		const TArray<TPair<FText, uint8>> Plate = {
			{ Card.Name, CollectionRole(EVeyraShellText::Heading) },
			{ Card.Status, CollectionRole(EVeyraShellText::Muted) },
			{ Card.MasteryShort, CollectionRole(EVeyraShellText::Muted) },
		};
		const FString Key = CollectionCardKey(Card.VanguardId);
		UVeyraShellButton* Button =
			AddArtCard(*Roster, VeyraProgressionModels::CollectionCardLabel(Card.VanguardId), Card.VanguardId, Plate, [this, Key] { OpenCard(Key); }, true, OpenCardId == Key);
		RosterCards.Add({ Button, Card.VanguardId, FVeyraRosterEntry{ Card.Name, Card.bOwned, Card.bRotation, Card.bFavorite } });
	}
	RosterNoMatch = AddText(*Roster, LOCTEXT("NoVanguardMatches", "No Vanguard matches."), CollectionRole(EVeyraShellText::Muted));
	ApplyRosterFilter();
}

void UVeyraShellScreen::ApplyRosterFilter()
{
	bool bAny = false;
	for (const FRosterCard& Card : RosterCards)
	{
		const bool bShown = VeyraRosterFilter::Shows(Card.Entry, GetRosterTab(), GetRosterSearch());
		bAny |= bShown;
		if (UWidget* Widget = Card.Widget.Get())
		{
			Widget->SetVisibility(bShown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}
	if (UWidget* NoMatch = RosterNoMatch.Get())
	{
		NoMatch->SetVisibility(bAny ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

bool UVeyraShellScreen::IsRosterCardShown(const FString& VanguardId) const
{
	const FRosterCard* Card = RosterCards.FindByPredicate([&VanguardId](const FRosterCard& Each) { return Each.VanguardId == VanguardId; });
	const UWidget* Widget = Card ? Card->Widget.Get() : nullptr;
	return Widget && Widget->GetVisibility() != ESlateVisibility::Collapsed;
}

void UVeyraShellScreen::AddRosterSearch(UPanelWidget& Parent, TConstArrayView<EVeyraRosterTab> Tabs)
{
	UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	RosterSearchBox = MakeTextField(LOCTEXT("RosterSearchHint", "Search Vanguards"), GetRosterSearch(), true);
	RosterSearchBox->OnTextChanged.AddUniqueDynamic(this, &UVeyraShellScreen::HandleRosterSearchChanged);
	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Width->SetWidthOverride(CollectionStyle().SettingsSearchWidth);
	Width->AddChild(RosterSearchBox);
	Line->AddChildToHorizontalBox(Width)->SetVerticalAlignment(VAlign_Center);
	for (const EVeyraRosterTab Tab : Tabs)
	{
		AddNamedButton(*Line, EVeyraShellButtonKind::Tab, RosterTabLabel(Tab), VeyraRosterFilter::TabName(Tab), [this, Tab] { ShowRosterTab(Tab); }, true, GetRosterTab() == Tab)
			->KeepLabelOnOneLine();
	}
	VeyraShellStyle::AddSpaced(Parent, *Line);
}

FText UVeyraShellScreen::RosterTabLabel(EVeyraRosterTab Tab)
{
	return FText::Format(LOCTEXT("RosterTab", "Show {0}"), VeyraRosterFilter::TabName(Tab));
}

const FString& UVeyraShellScreen::GetRosterSearch() const
{
	return Shown == EVeyraShellScreen::ChampionSelect ? SelectSearch : CollectionSearch;
}

EVeyraRosterTab UVeyraShellScreen::GetRosterTab() const
{
	return Shown == EVeyraShellScreen::ChampionSelect ? SelectTab : CollectionTab;
}

void UVeyraShellScreen::HandleRosterSearchChanged(const FText& Text)
{
	(Shown == EVeyraShellScreen::ChampionSelect ? SelectSearch : CollectionSearch) = Text.ToString();
	// In place: a rebuild would replace the very field being typed in.
	ApplyRosterFilter();
}

void UVeyraShellScreen::SetRosterSearch(const FString& Search)
{
	if (RosterSearchBox)
	{
		// Through the field, as typing is.
		RosterSearchBox->SetText(FText::FromString(Search));
	}
	HandleRosterSearchChanged(FText::FromString(Search));
}

void UVeyraShellScreen::ShowRosterTab(EVeyraRosterTab Tab)
{
	(Shown == EVeyraShellScreen::ChampionSelect ? SelectTab : CollectionTab) = Tab;
	ShownSignature.Reset();
	Refresh();
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
	// Favorites are marked while browsing, never in champion select (UX-30; ADR-058 §3), owned or not.
	const FString FavoriteId = Card.VanguardId;
	const bool bFavorite = Card.bFavorite;
	AddNamedButton(*Rows, EVeyraShellButtonKind::Quiet, VeyraProgressionModels::FavoriteLabel(Card.VanguardId, Card.bFavorite),
		Card.bFavorite ? LOCTEXT("Unfavorite", "Remove from Favorites") : LOCTEXT("Favorite", "Add to Favorites"),
		[this, FavoriteId, bFavorite] { Client->SetFavoriteVanguard(FavoriteId, !bFavorite); }, Client->CanIssue(EVeyraClientIntent::SetFavoriteVanguard))
		->KeepLabelOnOneLine();
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
