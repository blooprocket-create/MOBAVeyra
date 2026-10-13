// Copyright © 2026 Wayfinder Studios. All rights reserved.

// Player profiles (ADR-048 §5): the Profile page, where the player chooses their icon, background, featured Vanguard
// and Match History sharing, and another player's profile opened over the screen. The backend owns every profile;
// these screens show them and ask.

#include "Blueprint/WidgetTree.h"
#include "Client/VeyraClientIntents.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Engine/Texture2D.h"
#include "Shell/VeyraProfileModels.h"
#include "Shell/VeyraShellArt.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellScreen.h"
#include "Shell/VeyraShellStyle.h"
#include "Shell/VeyraShellStyleSettings.h"

#define LOCTEXT_NAMESPACE "VeyraShell"

namespace
{
	using VeyraShellStyle::EVeyraShellSurface;
	using VeyraShellStyle::EVeyraShellText;

	const UVeyraShellStyleSettings& ProfileStyle()
	{
		return *GetDefault<UVeyraShellStyleSettings>();
	}

	uint8 ProfileRole(EVeyraShellText Role)
	{
		return static_cast<uint8>(Role);
	}

	/** Hero's size in pixels: its source in the editor, where the platform data may still be compiling. */
	FIntPoint ProfileArtPixels(const UTexture2D& Hero)
	{
#if WITH_EDITORONLY_DATA
		if (Hero.Source.IsValid())
		{
			return FIntPoint(Hero.Source.GetSizeX(), Hero.Source.GetSizeY());
		}
#endif
		return FIntPoint(Hero.GetSizeX(), Hero.GetSizeY());
	}

	/**
	 * VanguardId's art at Size, as champion select draws it: a portrait cropped around the face, or the widest crop
	 * the illustration allows, with corners of CornerRadius (a circle when negative). With no Vanguard, a neutral
	 * disc or box: the default icon or background.
	 */
	UImage* MakeProfileArt(UWidgetTree& Tree, const FString& VanguardId, const FVector2D& Size, float CornerRadius, bool bPortrait,
		TOptional<FLinearColor> Edge = {}, float EdgeWidth = 0.0f)
	{
		const UVeyraShellStyleSettings& Style = ProfileStyle();
		UTexture2D* Hero = VanguardId.IsEmpty() ? nullptr : VeyraShellArt::HeroOf(VanguardId);
		FBox2f Region(FVector2f::ZeroVector, FVector2f::UnitVector);
		if (Hero)
		{
			const FIntPoint Pixels = ProfileArtPixels(*Hero);
			Region = VeyraShellArt::Crop(VanguardId, Pixels.X, Pixels.Y, static_cast<float>(Size.X / Size.Y), bPortrait);
		}
		UImage* Image = Tree.ConstructWidget<UImage>(UImage::StaticClass());
		Image->SetBrush(VeyraShellArt::Brush(Hero, Region, Size, CornerRadius, Style.PanelColor, Edge.Get(Style.HairlineColor), Edge.IsSet() ? EdgeWidth : Style.FrameWidth));
		return Image;
	}

	/**
	 * Region's panorama at Size (ADR-082 §10): the part of it as wide as Size's proportions allow, its middle at Centre of the
	 * way across, where the panoramas keep their landmarks; a neutral box until it is imported.
	 */
	UImage* MakeRegionArt(UWidgetTree& Tree, const FString& Region, const FVector2D& Size, float CornerRadius, float Centre,
		TOptional<FLinearColor> Edge = {}, float EdgeWidth = 0.0f)
	{
		const UVeyraShellStyleSettings& Style = ProfileStyle();
		UTexture2D* Panorama = VeyraShellArt::BackgroundOf(Region);
		FBox2f Part(FVector2f::ZeroVector, FVector2f::UnitVector);
		if (Panorama)
		{
			const FIntPoint Pixels = ProfileArtPixels(*Panorama);
			const float Wide = Pixels.Y > 0 ? static_cast<float>(Pixels.X) / Pixels.Y : 1.0f;
			const float Share = FMath::Min(1.0f, static_cast<float>(Size.X / Size.Y) / Wide);
			const float Left = FMath::Clamp(Centre - Share / 2.0f, 0.0f, 1.0f - Share);
			Part = FBox2f(FVector2f(Left, 0.0f), FVector2f(Left + Share, 1.0f));
		}
		UImage* Image = Tree.ConstructWidget<UImage>(UImage::StaticClass());
		Image->SetBrush(VeyraShellArt::Brush(Panorama, Part, Size, CornerRadius, Style.PanelColor, Edge.Get(Style.HairlineColor), Edge.IsSet() ? EdgeWidth : Style.FrameWidth));
		return Image;
	}

	UWidget& ProfileSized(UWidgetTree& Tree, UWidget& Child, const FVector2D& Size)
	{
		USizeBox* Box = Tree.ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(Size.X);
		Box->SetHeightOverride(Size.Y);
		Box->AddChild(&Child);
		return *Box;
	}

	/** Adds Child to a vertical box so that it takes the room left. */
	void AddProfileFilling(UPanelWidget& Parent, UWidget& Child)
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
}

void UVeyraShellScreen::AddProfileCard(const FVeyraProfileCardModel& Card, UPanelWidget& Parent)
{
	// A banner (ADR-082 §9): the chosen background's art across it, the featured Vanguard's when none is chosen, shaded from
	// the left so the words read; the icon round in a brass ring; the name in the display face over the Level and what is
	// featured; and the featured Vanguard's art at the right (UX-74).
	const UVeyraShellStyleSettings& Style = ProfileStyle();
	const float Icon = Style.ProfileIconSize;
	const FVector2D Featured(Style.ProfileFeaturedHeight * 0.75f, Style.ProfileFeaturedHeight);
	UBorder* Frame = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Raised, FMargin(0.0f));
	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	// The art covers the banner and is cut at its edges, never stretched to it.
	Layers->SetClipping(EWidgetClipping::ClipToBounds);
	Frame->SetContent(Layers);
	const auto Fill = [Layers](UWidget* Layer) {
		UOverlaySlot* Placed = Layers->AddChildToOverlay(Layer);
		Placed->SetHorizontalAlignment(HAlign_Fill);
		Placed->SetVerticalAlignment(VAlign_Fill);
		return Placed;
	};
	// The banner is always a region's panorama (ADR-082 §10): the one chosen, or the default region for the default background
	// and for a background no longer offered. Whole at its own proportions, scaled to cover the banner, never stretched to it.
	const FString Region = Card.BackgroundRegion.IsEmpty() ? Style.ProfileDefaultRegion : Card.BackgroundRegion;
	{
		const FVector2D ArtSize(Style.ProfileFeaturedHeight * Style.ProfileBackgroundAspect, Style.ProfileFeaturedHeight);
		UScaleBox* Cover = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
		Cover->SetStretch(EStretch::ScaleToFill);
		Cover->AddChild(&ProfileSized(*WidgetTree, *MakeRegionArt(*WidgetTree, Region, ArtSize, 0.0f, 0.5f, FLinearColor::Transparent, 0.0f), ArtSize));
		Fill(Cover);
	}
	if (Gradients.IsValidIndex(0))
	{
		Fill(VeyraShellStyle::MakeGradient(*WidgetTree, Gradients[0], Style.ScrimColor));
	}
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Fill(Row)->SetPadding(FMargin(Style.Spacing * 1.5f));

	UImage* Round = MakeProfileArt(*WidgetTree, Card.IconVanguard, FVector2D(Icon), -1.0f, true, Style.PrimaryColor, Style.FrameWidth * 2.0f);
	UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(&ProfileSized(*WidgetTree, *Round, FVector2D(Icon)));
	IconSlot->SetVerticalAlignment(VAlign_Center);
	IconSlot->SetPadding(FMargin(0.0f, 0.0f, Style.Spacing * 1.5f, 0.0f));
	UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock* Level = AddText(*Texts, Card.Level, ProfileRole(EVeyraShellText::Eyebrow));
	Level->SetColorAndOpacity(FSlateColor(Style.PrimaryColor));
	AddText(*Texts, Card.Name, ProfileRole(EVeyraShellText::Display));
	AddText(*Texts, Card.Featured, ProfileRole(EVeyraShellText::Body));
	UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(Texts);
	TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TextSlot->SetVerticalAlignment(VAlign_Center);
	// The featured Vanguard's base art (UX-74); nothing when none is featured.
	if (!Card.FeaturedVanguard.IsEmpty())
	{
		Row->AddChildToHorizontalBox(&ProfileSized(*WidgetTree, *MakeProfileArt(*WidgetTree, Card.FeaturedVanguard, Featured, Style.PanelCornerRadius, false, Style.PrimaryColor,
			Style.FrameWidth), Featured))->SetVerticalAlignment(VAlign_Center);
	}
	VeyraShellStyle::AddSpaced(Parent, *Frame);
}
void UVeyraShellScreen::BuildProfilePage(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent)
{
	const UVeyraShellStyleSettings& Style = ProfileStyle();
	const FVeyraProfileSettings& Own = Snapshot.ProfileSettings;
	// The draft starts from what is saved, and starts again whenever that changes, as after a save.
	if (Own.bLoaded && (!bProfileDraftReady || !(ProfileDraftBase == Own.Saved)))
	{
		ProfileDraft = Own.Saved;
		ProfileDraftBase = Own.Saved;
		bProfileDraftReady = true;
	}
	const FVeyraProfilePageModel Model = VeyraProfileModels::DescribePage(Snapshot, ProfileDraft);
	AddText(Parent, LOCTEXT("ProfileEyebrow", "How others see you"), ProfileRole(EVeyraShellText::Eyebrow));
	AddText(Parent, LOCTEXT("ProfileTitle", "Profile"), ProfileRole(EVeyraShellText::Display));
	if (!Model.bLoaded)
	{
		AddText(Parent, LOCTEXT("ProfileReading", "Reading your profile..."), ProfileRole(EVeyraShellText::Muted));
		return;
	}
	if (Model.Preview.IsSet())
	{
		AddProfileCard(*Model.Preview, Parent);
	}
	BuildDisplayName(Snapshot, Parent);
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(Rows);
	AddProfileFilling(Parent, *Scroll);
	const bool bCanSave = Client->CanIssue(EVeyraClientIntent::SaveProfileSettings);
	const float Tile = Style.RosterTileSize;
	const auto AddChoices = [this, &Rows, Tile](const FText& Title, const TArray<FVeyraProfileChoice>& Choices, bool bPortrait, TFunction<void(const FString&)> Apply) {
		// Each choice under its heading, as the client's sections are headed (ADR-082 §9).
		AddText(*Rows, Title, ProfileRole(EVeyraShellText::Eyebrow));
		UWrapBox* Wrap = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
		Wrap->SetInnerSlotPadding(FVector2D(ProfileStyle().Spacing / 2.0f));
		for (const FVeyraProfileChoice& Choice : Choices)
		{
			const FString Value = Choice.Value;
			const FVector2D Size = bPortrait ? FVector2D(Tile) : FVector2D(Tile * 1.5f, Tile);
			// A region's panorama shows its landmarks; a Vanguard its art (ADR-082 §10).
			UImage* Picture = !Choice.Region.IsEmpty() ? MakeRegionArt(*WidgetTree, Choice.Region, Size, ProfileStyle().PanelCornerRadius, ProfileStyle().ProfileBackgroundFocus)
														: MakeProfileArt(*WidgetTree, Choice.Vanguard, Size, bPortrait ? -1.0f : ProfileStyle().PanelCornerRadius, bPortrait);
			AddContentButton(*Wrap, Choice.Label, ProfileSized(*WidgetTree, *Picture, Size), [this, Value, Apply] {
				Apply(Value);
				Refresh();
			}, true, Choice.bSelected);
		}
		VeyraShellStyle::AddSpaced(*Rows, *Wrap);
	};
	AddChoices(LOCTEXT("IconsHeading", "Icon"), Model.Icons, true, [this](const FString& Value) { ProfileDraft.Icon = Value; });
	// The default background shows the default region's panorama, as the banner does (ADR-082 §10).
	TArray<FVeyraProfileChoice> Backgrounds = Model.Backgrounds;
	for (FVeyraProfileChoice& Background : Backgrounds)
	{
		if (Background.Region.IsEmpty())
		{
			Background.Region = Style.ProfileDefaultRegion;
		}
	}
	AddChoices(LOCTEXT("BackgroundsHeading", "Background"), Backgrounds, false, [this](const FString& Value) { ProfileDraft.Background = Value; });
	// A Vanguard the player permanently owns, or none; never chosen for them (UX-71).
	AddChoices(LOCTEXT("FeaturedHeading", "Featured Vanguard"), Model.Featured, true, [this](const FString& Value) { ProfileDraft.FeaturedVanguardId = Value; });
	// Private by default; the owner may share it and stop sharing it (UX-72).
	AddKindButton(*Rows, EVeyraShellButtonKind::Tab, VeyraProfileModels::ShareHistoryLabel(), [this] {
		ProfileDraft.bShowMatchHistory = !ProfileDraft.bShowMatchHistory;
		Refresh();
	}, true, Model.bShowsMatchHistory)->KeepLabelOnOneLine();
	UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddKindButton(*Footer, EVeyraShellButtonKind::Primary, VeyraProfileModels::SaveLabel(), [this] { Client->SaveProfileSettings(ProfileDraft); },
		bCanSave && Model.bChanged)->KeepLabelOnOneLine();
	if (!Model.Feedback.IsEmpty())
	{
		AddText(*Footer, Model.Feedback, ProfileRole(EVeyraShellText::Body))->SetAutoWrapText(false);
	}
	VeyraShellStyle::AddSpaced(*Rows, *Footer);
}

void UVeyraShellScreen::BuildProfileOverlay(const FVeyraClientSnapshot& Snapshot)
{
	const FVeyraProfileViewModel Model = VeyraProfileModels::DescribeView(Snapshot, Client->CanIssue(EVeyraClientIntent::LoadMoreProfileMatches));
	if (!Model.bOpen)
	{
		return;
	}
	const UVeyraShellStyleSettings& Style = ProfileStyle();
	UBorder* Scrim = VeyraShellStyle::MakeBorder(*WidgetTree, Style.MenuScrimColor, 0.0f);
	Scrim->SetHorizontalAlignment(HAlign_Center);
	Scrim->SetVerticalAlignment(VAlign_Center);
	UOverlaySlot* ScrimSlot = Popup->AddChildToOverlay(Scrim);
	ScrimSlot->SetHorizontalAlignment(HAlign_Fill);
	ScrimSlot->SetVerticalAlignment(VAlign_Fill);
	UBorder* Panel = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Raised, FMargin(Style.Spacing * 2.0f));
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Rows);
	USizeBox* PanelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PanelBox->SetWidthOverride(Style.DialogWidth);
	PanelBox->AddChild(Panel);
	Scrim->SetContent(PanelBox);

	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* Eyebrow = AddText(*Header, LOCTEXT("ProfileOverlayEyebrow", "Player profile"), ProfileRole(EVeyraShellText::Eyebrow));
	if (UHorizontalBoxSlot* EyebrowSlot = Cast<UHorizontalBoxSlot>(Eyebrow->Slot))
	{
		EyebrowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	AddKindButton(*Header, EVeyraShellButtonKind::Quiet, VeyraProfileModels::CloseLabel(), [this] { Client->CloseProfile(); },
		Client->CanIssue(EVeyraClientIntent::CloseProfile))->KeepLabelOnOneLine();
	VeyraShellStyle::AddSpaced(*Rows, *Header);
	if (!Model.Status.IsEmpty())
	{
		AddText(*Rows, Model.Status, ProfileRole(EVeyraShellText::Body));
		return;
	}
	if (Model.Card.IsSet())
	{
		AddProfileCard(*Model.Card, *Rows);
	}
	if (Model.Opened.IsSet())
	{
		// A shared match: the same Scoreboard and Detailed Statistics as the owner's own record (ADR-048 §3).
		AddKindButton(*Rows, EVeyraShellButtonKind::Quiet, LOCTEXT("BackToProfileHistory", "Back to Match History"), [this] { Client->CloseProfileMatch(); },
			Client->CanIssue(EVeyraClientIntent::CloseProfileMatch))->KeepLabelOnOneLine();
		AddText(*Rows, Model.Opened->Headline, ProfileRole(EVeyraShellText::Title));
		USizeBox* ReportBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ReportBox->SetHeightOverride(Style.ProfileFeaturedHeight * 2.0f);
		UVerticalBox* Report = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		ReportBox->AddChild(Report);
		BuildReport(Snapshot, Model.Opened->Report, *Report);
		VeyraShellStyle::AddSpaced(*Rows, *ReportBox);
		return;
	}
	AddText(*Rows, LOCTEXT("ProfileHistoryHeading", "Match History"), ProfileRole(EVeyraShellText::Heading));
	// The owner's own filters (ADR-048 §3).
	if (Snapshot.ProfileView.Profile.bSharesMatchHistory)
	{
		const VeyraBackendProtocol::FHistoryFilter& Current = Snapshot.ProfileView.Filter;
		const auto Load = [this](const VeyraBackendProtocol::FHistoryFilter& Filter) { Client->FilterProfileMatches(Filter); };
		const bool bCanFilter = Client->CanIssue(EVeyraClientIntent::FilterProfileMatches);
		AddHistoryFilter(*Rows, Model.Vanguards, Current, [](VeyraBackendProtocol::FHistoryFilter& Filter, const FString& Value) { Filter.VanguardId = Value; }, Load, bCanFilter);
		AddHistoryFilter(*Rows, Model.Modes, Current, [](VeyraBackendProtocol::FHistoryFilter& Filter, const FString& Value) { Filter.Mode = Value; }, Load, bCanFilter);
		AddHistoryFilter(*Rows, Model.Outcomes, Current, [](VeyraBackendProtocol::FHistoryFilter& Filter, const FString& Value) { Filter.Outcome = Value; }, Load, bCanFilter);
	}
	if (!Model.HistoryNote.IsEmpty())
	{
		AddText(*Rows, Model.HistoryNote, ProfileRole(EVeyraShellText::Muted));
	}
	// The records and Load More scroll within a bounded height, so a full page never pushes the popup past the screen.
	USizeBox* ListBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ListBox->SetMaxDesiredHeight(Style.ProfileHistoryMaxHeight);
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(List);
	ListBox->AddChild(Scroll);
	VeyraShellStyle::AddSpaced(*Rows, *ListBox);
	const bool bCanOpen = Client->CanIssue(EVeyraClientIntent::OpenProfileMatch);
	for (const FVeyraHistoryRow& Row : Model.Rows)
	{
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		const FString MatchId = Row.MatchId;
		AddNamedButton(*Line, EVeyraShellButtonKind::Quiet, FText::Format(LOCTEXT("OpenProfileMatch", "Open {0}"), Row.Summary), LOCTEXT("OpenShort", "Open"),
			[this, MatchId] { Client->OpenProfileMatch(MatchId); }, bCanOpen);
		AddText(*Line, Row.Summary, ProfileRole(EVeyraShellText::Body))->SetAutoWrapText(false);
		VeyraShellStyle::AddSpaced(*List, *Line);
	}
	if (Model.bOffersLoadMore)
	{
		AddButton(*List, LOCTEXT("ProfileLoadMore", "Load More"), [this] { Client->LoadMoreProfileMatches(); });
	}
}

void UVeyraShellScreen::BuildDisplayName(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent)
{
	// One row (ADR-082 §9): the name now with what a change costs, the field for a new one, and the change itself; a change's
	// question takes the row's place while it is asked.
	const UVeyraShellStyleSettings& Style = ProfileStyle();
	const FVeyraDisplayNameModel Model = VeyraProfileModels::DescribeName(Snapshot, FDateTime::UtcNow());
	if (!Model.bLoaded)
	{
		return;
	}
	UBorder* Panel = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Panel, FMargin(Style.Spacing));
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Rows);
	AddText(*Rows, LOCTEXT("DisplayNameHeading", "Display name"), ProfileRole(EVeyraShellText::Eyebrow));
	UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBox* Now = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddText(*Now, Model.Current, ProfileRole(EVeyraShellText::Heading))->SetAutoWrapText(false);
	AddText(*Now, Model.Cost, ProfileRole(EVeyraShellText::Muted));
	if (!Model.Next.IsEmpty())
	{
		AddText(*Now, Model.Next, ProfileRole(EVeyraShellText::Muted));
	}
	UHorizontalBoxSlot* NowSlot = Line->AddChildToHorizontalBox(Now);
	NowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	NowSlot->SetVerticalAlignment(VAlign_Center);
	const FVeyraNameOffer* Asked = Model.Offers.FindByPredicate([this](const FVeyraNameOffer& Offer) {
		return Confirm == EVeyraShellConfirm::NameChange && ConfirmId == (Offer.Currency.IsEmpty() ? FString(TEXT("free")) : Offer.Currency);
	});
	if (Asked && !NameDraft.TrimStartAndEnd().IsEmpty())
	{
		Rows->AddChild(Line);
		// The change's question, naming the price; the old name is anyone's once it commits (Profiles Bible §4).
		AddText(*Rows, VeyraProfileModels::NameChangePrompt(NameDraft.TrimStartAndEnd(), Asked->Price), ProfileRole(EVeyraShellText::Body));
		UHorizontalBox* Answers = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		const FString Currency = Asked->Currency;
		AddKindButton(*Answers, EVeyraShellButtonKind::Primary, VeyraProfileModels::ConfirmNameChangeLabel(), [this, Currency] {
			Client->ChangeDisplayName(NameDraft, Currency);
			AskToConfirm(EVeyraShellConfirm::None, FString());
		}, Client->CanIssue(EVeyraClientIntent::ChangeDisplayName))->KeepLabelOnOneLine();
		AddKindButton(*Answers, EVeyraShellButtonKind::Quiet, VeyraProfileModels::CancelNameChangeLabel(),
			[this] { AskToConfirm(EVeyraShellConfirm::None, FString()); })->KeepLabelOnOneLine();
		VeyraShellStyle::AddSpaced(*Rows, *Answers);
	}
	else
	{
		NameBox = MakeTextField(LOCTEXT("NewNameHint", "New name"), NameDraft, Model.bCanChange);
		NameBox->OnTextChanged.AddUniqueDynamic(this, &UVeyraShellScreen::HandleNameChanged);
		USizeBox* Field = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Field->SetWidthOverride(Style.ProfileNameFieldWidth);
		Field->AddChild(NameBox);
		UHorizontalBoxSlot* FieldSlot = Line->AddChildToHorizontalBox(Field);
		FieldSlot->SetVerticalAlignment(VAlign_Center);
		FieldSlot->SetPadding(FMargin(Style.Spacing, 0.0f));
		const bool bCanAsk = Model.bCanChange && Client->CanIssue(EVeyraClientIntent::ChangeDisplayName);
		for (const FVeyraNameOffer& Offer : Model.Offers)
		{
			const FString Id = Offer.Currency.IsEmpty() ? FString(TEXT("free")) : Offer.Currency;
			UVeyraShellButton* Change = AddKindButton(*Line, EVeyraShellButtonKind::Secondary, Offer.Label, [this, Id] { AskToConfirm(EVeyraShellConfirm::NameChange, Id); }, bCanAsk);
			Change->KeepLabelOnOneLine();
			if (UHorizontalBoxSlot* ChangeSlot = Cast<UHorizontalBoxSlot>(Change->Slot))
			{
				ChangeSlot->SetVerticalAlignment(VAlign_Center);
			}
		}
		Rows->AddChild(Line);
	}
	if (!Model.Feedback.IsEmpty())
	{
		AddText(*Rows, Model.Feedback, ProfileRole(EVeyraShellText::Body));
	}
	VeyraShellStyle::AddSpaced(Parent, *Panel);
}

void UVeyraShellScreen::BuildChooseName(const FVeyraClientSnapshot& Snapshot)
{
	const UVeyraShellStyleSettings& Style = ProfileStyle();
	UBorder* Panel = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Raised, FMargin(Style.Spacing * 2.0f));
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Rows);
	AddText(*Rows, LOCTEXT("ChooseNameEyebrow", "Welcome back"), ProfileRole(EVeyraShellText::Eyebrow));
	AddText(*Rows, LOCTEXT("ChooseNameTitle", "Choose a new name"), ProfileRole(EVeyraShellText::Title));
	AddText(*Rows, LOCTEXT("ChooseNameBody", "Another player took your name while you were away. Everything else on your account is as you left it, and choosing a new name is free."),
		ProfileRole(EVeyraShellText::Body));
	NameBox = MakeTextField(LOCTEXT("ChooseNameHint", "New name"), NameDraft, true);
	NameBox->OnTextChanged.AddUniqueDynamic(this, &UVeyraShellScreen::HandleNameChanged);
	VeyraShellStyle::AddSpaced(*Rows, *NameBox);
	AddKindButton(*Rows, EVeyraShellButtonKind::Primary, VeyraProfileModels::ChooseNameLabel(), [this] { Client->ChangeDisplayName(NameDraft, FString()); },
		Client->CanIssue(EVeyraClientIntent::ChangeDisplayName));
	if (const FText Feedback = VeyraProfileModels::NameFeedbackText(Snapshot.DisplayNameChange.Feedback); !Feedback.IsEmpty())
	{
		AddText(*Rows, Feedback, ProfileRole(EVeyraShellText::Body));
	}
	AddCentred(*Panel);
}

void UVeyraShellScreen::HandleNameChanged(const FText& Text)
{
	NameDraft = Text.ToString();
}

void UVeyraShellScreen::SetNameDraft(const FString& Name)
{
	if (NameBox)
	{
		NameBox->SetText(FText::FromString(Name));
	}
	NameDraft = Name;
}

#undef LOCTEXT_NAMESPACE
