// Copyright © 2026 Wayfinder Studios. All rights reserved.

// Champion select's screen, in League's layout: the roster as a bench of portraits across the top
// with the countdown between two draining bars; the player's team down the left and the enemy team
// down the right; the shown Vanguard's art large in the middle; the Flux Spell slots, Lock In, the
// match setup and the mode along the bottom (Pre-Game Client UX Bible §5, 23–40).

#include "Shell/VeyraShellScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Client/VeyraClientIntents.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Engine/Texture2D.h"
#include "Shell/VeyraShellArt.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellStyle.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "Slots/VeyraAbilitySlot.h"

#define LOCTEXT_NAMESPACE "VeyraShell"

namespace
{
	using VeyraShellStyle::EVeyraShellText;
	using VeyraShellStyle::EVeyraShellSurface;

	const UVeyraShellStyleSettings& Style()
	{
		return *GetDefault<UVeyraShellStyleSettings>();
	}

	/** Hero's size in pixels: its source in the editor, where the platform data may still be compiling. */
	FIntPoint SizeOf(const UTexture2D& Hero)
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
	 * VanguardId's art at Size: a portrait crop, or the widest crop the illustration allows, with
	 * rounded corners of CornerRadius (a circle when negative) outlined in Outline. With no art, or no
	 * Vanguard, an empty disc or box of the panel colour.
	 */
	UImage* MakeArt(UWidgetTree& Tree, const FString& VanguardId, const FVector2D& Size, float CornerRadius, bool bPortrait, const FLinearColor& Outline)
	{
		UTexture2D* Hero = VeyraShellArt::HeroOf(VanguardId);
		FBox2f Region(FVector2f::ZeroVector, FVector2f::UnitVector);
		if (Hero)
		{
			const FIntPoint Pixels = SizeOf(*Hero);
			Region = VeyraShellArt::Crop(VanguardId, Pixels.X, Pixels.Y, static_cast<float>(Size.X / Size.Y), bPortrait);
		}
		UImage* Image = Tree.ConstructWidget<UImage>(UImage::StaticClass());
		Image->SetBrush(VeyraShellArt::Brush(Hero, Region, Size, CornerRadius, Style().PanelColor, Outline, Style().FrameWidth));
		return Image;
	}

	/** Child in a box of exactly Size. */
	USizeBox* Sized(UWidgetTree& Tree, UWidget& Child, const FVector2D& Size)
	{
		USizeBox* Box = Tree.ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(Size.X);
		Box->SetHeightOverride(Size.Y);
		Box->AddChild(&Child);
		return Box;
	}

	/**
	 * Text in Role, aligned by Align, with no spacing after it: for the compact seat rows and tiles. Only
	 * descriptions wrap (bWrap): UMG wraps text in an auto-sized slot at its narrowest, so a short label
	 * that may wrap breaks after every word.
	 */
	UTextBlock* AddLine(UWidgetTree& Tree, UPanelWidget& Parent, const FText& Text, EVeyraShellText Role, EHorizontalAlignment Align = HAlign_Left,
		bool bWrap = false)
	{
		UTextBlock* Block = VeyraShellStyle::MakeText(Tree, Text, Role);
		Block->SetAutoWrapText(bWrap);
		Block->SetJustification(Align == HAlign_Right ? ETextJustify::Right : (Align == HAlign_Center ? ETextJustify::Center : ETextJustify::Left));
		if (UVerticalBox* Column = Cast<UVerticalBox>(&Parent))
		{
			Column->AddChildToVerticalBox(Block)->SetHorizontalAlignment(Align);
		}
		else
		{
			Parent.AddChild(Block);
		}
		return Block;
	}

	/** One of the countdown's draining bars, emptying toward the countdown in the middle. */
	UProgressBar* MakePickBar(UWidgetTree& Tree, EProgressBarFillType::Type Fill)
	{
		UProgressBar* Bar = Tree.ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		FProgressBarStyle BarStyle;
		BarStyle.SetBackgroundImage(FSlateColorBrush(Style().PanelColor));
		BarStyle.SetFillImage(FSlateColorBrush(FLinearColor::White));
		Bar->SetWidgetStyle(BarStyle);
		Bar->SetFillColorAndOpacity(Style().AccentColor);
		Bar->SetBarFillType(Fill);
		return Bar;
	}
}

FText UVeyraShellScreen::AbilitiesLabel(bool bShowing)
{
	return bShowing ? LOCTEXT("HideAbilities", "Hide Abilities") : LOCTEXT("ViewAbilities", "View Abilities");
}

void UVeyraShellScreen::BuildChampionSelect(const FVeyraClientSnapshot& Snapshot)
{
	const FVeyraSelectModel Model = VeyraShellModels::DescribeSelect(Snapshot, Client->GetRemainingPickSeconds(),
		Client->CanIssue(EVeyraClientIntent::HoverVanguard), Client->CanIssue(EVeyraClientIntent::LockVanguard), Client->CanIssue(EVeyraClientIntent::LeaveSelect),
		Client->CanIssue(EVeyraClientIntent::ChooseFluxSpell));
	// Browsing changes the large art (UX 27): the shown Vanguard fills the screen behind everything.
	ShowBackdrop(VeyraShellArt::HeroOf(Model.ShownVanguardId));
	PickSeconds = Model.PickSeconds;

	Content->AddChildToVerticalBox(&MakeSelectHeader(Model));
	UHorizontalBox* Middle = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Middle->AddChildToHorizontalBox(&MakeSeatColumn(Model, /*bAllies*/ true))->SetVerticalAlignment(VAlign_Center);
	UHorizontalBoxSlot* CentreSlot = Middle->AddChildToHorizontalBox(&MakeCentre(Model));
	CentreSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CentreSlot->SetHorizontalAlignment(HAlign_Center);
	CentreSlot->SetVerticalAlignment(VAlign_Center);
	Middle->AddChildToHorizontalBox(&MakeSeatColumn(Model, /*bAllies*/ false))->SetVerticalAlignment(VAlign_Center);
	UVerticalBoxSlot* MiddleSlot = Content->AddChildToVerticalBox(Middle);
	MiddleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	MiddleSlot->SetPadding(FMargin(0.0f, Style().Spacing));
	Content->AddChildToVerticalBox(&MakeSelectFooter(Model));
	BuildSpellPicker(Model);
}

UWidget& UVeyraShellScreen::MakeSelectHeader(const FVeyraSelectModel& Model)
{
	const UVeyraShellStyleSettings& Settings = Style();
	UVerticalBox* Header = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

	// The roster as League's bench: a portrait for each Vanguard the player may pick, the taken ones
	// disabled (UX 29).
	UHorizontalBox* Bench = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* BenchTitle = VeyraShellStyle::MakeText(*WidgetTree, LOCTEXT("AvailableVanguards", "AVAILABLE VANGUARDS"), EVeyraShellText::Eyebrow);
	BenchTitle->SetAutoWrapText(false);
	Bench->AddChildToHorizontalBox(BenchTitle)->SetVerticalAlignment(VAlign_Center);
	for (const FVeyraSelectCardModel& Card : Model.Cards)
	{
		UVerticalBox* Tile = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		const FVector2D TileSize(Settings.RosterTileSize, Settings.RosterTileSize);
		UImage* Portrait = MakeArt(*WidgetTree, Card.VanguardId, TileSize, Settings.TileCornerRadius, /*bPortrait*/ true,
			Card.bChosen ? Settings.FrameColor : Settings.PanelColor);
		if (Card.bTaken)
		{
			Portrait->SetColorAndOpacity(Settings.MutedTextColor);
		}
		Tile->AddChildToVerticalBox(Portrait)->SetHorizontalAlignment(HAlign_Center);
		AddLine(*WidgetTree, *Tile, Card.Name, EVeyraShellText::Small, HAlign_Center);
		const FString Id = Card.VanguardId;
		UVeyraShellButton* Button = AddContentButton(*Bench, Card.Name, *Tile, [this, Id] { Client->HoverVanguard(Id); }, Model.bCanChoose && !Card.bTaken, Card.bChosen);
		Cast<UHorizontalBoxSlot>(Button->Slot)->SetPadding(FMargin(Settings.Spacing, 0.0f, 0.0f, 0.0f));
	}
	Header->AddChildToVerticalBox(Bench)->SetHorizontalAlignment(HAlign_Center);

	// The countdown between two draining bars, always in view (UX 31).
	UHorizontalBox* Timer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	PickBars.Reset();
	const bool bBars = PickSeconds > 0.0;
	if (bBars)
	{
		PickBars.Add(MakePickBar(*WidgetTree, EProgressBarFillType::RightToLeft));
		UHorizontalBoxSlot* Left = Timer->AddChildToHorizontalBox(Sized(*WidgetTree, *PickBars.Last(), FVector2D(Settings.PickBarWidth, Settings.PickBarHeight)));
		Left->SetVerticalAlignment(VAlign_Center);
	}
	Countdown = VeyraShellStyle::MakeText(*WidgetTree, Model.Countdown, EVeyraShellText::Countdown);
	Countdown->SetJustification(ETextJustify::Center);
	UHorizontalBoxSlot* CountdownSlot = Timer->AddChildToHorizontalBox(Countdown);
	CountdownSlot->SetPadding(FMargin(Settings.Spacing * 2.0f, 0.0f));
	CountdownSlot->SetVerticalAlignment(VAlign_Center);
	if (bBars)
	{
		PickBars.Add(MakePickBar(*WidgetTree, EProgressBarFillType::LeftToRight));
		UHorizontalBoxSlot* Right = Timer->AddChildToHorizontalBox(Sized(*WidgetTree, *PickBars.Last(), FVector2D(Settings.PickBarWidth, Settings.PickBarHeight)));
		Right->SetVerticalAlignment(VAlign_Center);
		UpdatePickBars();
	}
	Header->AddChildToVerticalBox(Timer)->SetHorizontalAlignment(HAlign_Center);
	AddLine(*WidgetTree, *Header, Model.Phase, EVeyraShellText::Body, HAlign_Center);
	return *Header;
}

UWidget& UVeyraShellScreen::MakeSeatColumn(const FVeyraSelectModel& Model, bool bAllies)
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	// Practice has one team: the right column stays empty, holding the centre in the middle.
	if (bAllies || Model.bTeams)
	{
		AddLine(*WidgetTree, *Column, bAllies ? LOCTEXT("YourTeam", "Your Team") : LOCTEXT("EnemyTeam", "Enemy Team"), EVeyraShellText::Heading,
			bAllies ? HAlign_Left : HAlign_Right);
		for (const FVeyraSelectSeatModel& Seat : Model.Seats)
		{
			if (Seat.bAlly == bAllies)
			{
				Column->AddChildToVerticalBox(&MakeSeatRow(Seat))->SetPadding(FMargin(0.0f, Style().Spacing / 2.0f));
			}
		}
	}
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetWidthOverride(Style().SeatColumnWidth);
	Box->AddChild(Column);
	return *Box;
}

UWidget& UVeyraShellScreen::MakeSeatRow(const FVeyraSelectSeatModel& Seat)
{
	const UVeyraShellStyleSettings& Settings = Style();
	// The teammates' names, hovers or locks and status (UX 28, 35); the enemy's locks only.
	// A smoked row; the player's own outlined in the frame's gold.
	UBorder* Row = VeyraShellStyle::MakeSurface(*WidgetTree, Seat.bYou ? EVeyraShellSurface::Raised : EVeyraShellSurface::Panel, FMargin(Settings.Spacing / 2.0f));
	if (Seat.bYou)
	{
		Row->SetBrush(FSlateRoundedBoxBrush(Settings.SurfaceRaisedColor, Settings.PanelCornerRadius, Settings.FrameColor.CopyWithNewOpacity(0.7f), 1.0f));
	}
	UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Row->SetContent(Line);

	UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	const EHorizontalAlignment Align = Seat.bAlly ? HAlign_Left : HAlign_Right;
	const FText Vanguard = !Seat.Vanguard.IsEmpty() ? Seat.Vanguard : (Seat.bAlly ? LOCTEXT("SeatChoosing", "Choosing...") : LOCTEXT("SeatUnknown", "Unknown"));
	UTextBlock* VanguardLine = AddLine(*WidgetTree, *Texts, Vanguard, EVeyraShellText::Heading, Align);
	if (Seat.bYou)
	{
		VanguardLine->SetColorAndOpacity(FSlateColor(Settings.FrameColor));
	}
	AddLine(*WidgetTree, *Texts, Seat.Name, EVeyraShellText::Body, Align);
	AddLine(*WidgetTree, *Texts, Seat.StatusText, EVeyraShellText::Small, Align);

	const FVector2D PortraitSize(Settings.PortraitSize, Settings.PortraitSize);
	UImage* Portrait = MakeArt(*WidgetTree, Seat.VanguardId, PortraitSize, /*a circle*/ -1.0f, /*bPortrait*/ true,
		Seat.bAlly ? Settings.AllyColor : Settings.EnemyColor);
	if (Seat.bAlly)
	{
		// The player's own starting spells beside the portrait, as League shows summoner spells, by
		// initial; a teammate's are not shared (ADR-015 §5), so theirs stay blank.
		UVerticalBox* Spells = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		for (int32 SpellSlot = 0; SpellSlot < static_cast<int32>(UE_ARRAY_COUNT(VeyraAbilitySlots::Spells)); ++SpellSlot)
		{
			const FText Spell = Seat.Spells.IsValidIndex(SpellSlot) ? Seat.Spells[SpellSlot] : FText::GetEmpty();
			UBorder* Tile = VeyraShellStyle::MakeBorder(*WidgetTree, Settings.ButtonColor, 0.0f);
			Tile->SetBrush(FSlateRoundedBoxBrush(Settings.ButtonColor, Settings.ButtonCornerRadius, Settings.HairlineColor, 1.0f));
			Tile->SetHorizontalAlignment(HAlign_Center);
			Tile->SetVerticalAlignment(VAlign_Center);
			Tile->SetContent(VeyraShellStyle::MakeText(*WidgetTree, Spell.IsEmpty() ? FText::GetEmpty() : FText::FromString(Spell.ToString().Left(1)), EVeyraShellText::Small));
			Spells->AddChildToVerticalBox(Sized(*WidgetTree, *Tile, FVector2D(Settings.SeatSpellSize, Settings.SeatSpellSize)))->SetPadding(FMargin(0.0f, 1.0f));
		}
		Line->AddChildToHorizontalBox(Spells)->SetVerticalAlignment(VAlign_Center);
		Line->AddChildToHorizontalBox(Portrait)->SetPadding(FMargin(Settings.Spacing / 2.0f, 0.0f));
		UHorizontalBoxSlot* TextSlot = Line->AddChildToHorizontalBox(Texts);
		TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TextSlot->SetVerticalAlignment(VAlign_Center);
	}
	else
	{
		UHorizontalBoxSlot* TextSlot = Line->AddChildToHorizontalBox(Texts);
		TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TextSlot->SetVerticalAlignment(VAlign_Center);
		Line->AddChildToHorizontalBox(Portrait)->SetPadding(FMargin(Settings.Spacing / 2.0f, 0.0f, 0.0f, 0.0f));
	}
	return *Row;
}

UWidget& UVeyraShellScreen::MakeCentre(const FVeyraSelectModel& Model)
{
	const UVeyraShellStyleSettings& Settings = Style();
	UVerticalBox* Centre = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	const FVector2D FrameSize(Settings.SplashWidth, Settings.SplashHeight);
	UOverlay* Frame = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	UImage* Splash = MakeArt(*WidgetTree, Model.ShownVanguardId, FrameSize, Settings.SplashCornerRadius, /*bPortrait*/ false, Settings.FrameColor);
	Frame->AddChildToOverlay(Splash);
	if (Model.ShownVanguardId.IsEmpty())
	{
		UOverlaySlot* Prompt = Frame->AddChildToOverlay(VeyraShellStyle::MakeText(*WidgetTree, LOCTEXT("ChooseAVanguard", "Choose a Vanguard"), EVeyraShellText::Muted));
		Prompt->SetHorizontalAlignment(HAlign_Center);
		Prompt->SetVerticalAlignment(VAlign_Center);
	}
	else if (bShowAbilities)
	{
		// View Abilities lays the kit over the art: the passive, then Q, W, E and R.
		UBorder* Kit = VeyraShellStyle::MakeBorder(*WidgetTree, Settings.MenuScrimColor, Settings.Spacing * 2.0f);
		UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Kit->SetContent(Lines);
		for (const FVeyraAbilityLineModel& Ability : Model.Abilities)
		{
			AddLine(*WidgetTree, *Lines, FText::Format(LOCTEXT("AbilityLine", "{0}: {1}"), Ability.Key, Ability.Name), EVeyraShellText::Heading);
			UTextBlock* Description = AddLine(*WidgetTree, *Lines, Ability.Description, EVeyraShellText::Muted, HAlign_Left, /*bWrap*/ true);
			Cast<UVerticalBoxSlot>(Description->Slot)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, Settings.Spacing / 2.0f));
		}
		UOverlaySlot* KitSlot = Frame->AddChildToOverlay(Kit);
		KitSlot->SetHorizontalAlignment(HAlign_Fill);
		KitSlot->SetVerticalAlignment(VAlign_Fill);
	}
	Centre->AddChildToVerticalBox(Sized(*WidgetTree, *Frame, FrameSize))->SetHorizontalAlignment(HAlign_Center);

	if (!Model.ShownVanguardId.IsEmpty())
	{
		UTextBlock* Name = AddLine(*WidgetTree, *Centre, Model.ShownName, EVeyraShellText::Title, HAlign_Center);
		Cast<UVerticalBoxSlot>(Name->Slot)->SetPadding(FMargin(0.0f, Settings.Spacing, 0.0f, 0.0f));
		AddLine(*WidgetTree, *Centre, Model.ShownTitle, EVeyraShellText::Muted, HAlign_Center);
		UVeyraShellButton* Toggle = AddButton(*Centre, AbilitiesLabel(bShowAbilities), [this] {
			bShowAbilities = !bShowAbilities;
			Refresh();
		});
		Cast<UVerticalBoxSlot>(Toggle->Slot)->SetHorizontalAlignment(HAlign_Center);
		Toggle->KeepLabelOnOneLine();
	}
	return *Centre;
}

UWidget& UVeyraShellScreen::MakeSelectFooter(const FVeyraSelectModel& Model)
{
	const UVeyraShellStyleSettings& Settings = Style();
	UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	// Your Match Setup once locked in (UX 38), where League keeps its chat.
	UVerticalBox* Setup = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (!Model.Setup.IsEmpty())
	{
		AddLine(*WidgetTree, *Setup, Model.Setup, EVeyraShellText::Body, HAlign_Left, /*bWrap*/ true);
	}
	if (Model.bOffersLeave)
	{
		AddLine(*WidgetTree, *Setup, LOCTEXT("LeaveWarning", "Leaving ends champion select for everyone and takes your party out of the queue."),
			EVeyraShellText::Muted, HAlign_Left, /*bWrap*/ true);
	}
	UHorizontalBoxSlot* SetupSlot = Footer->AddChildToHorizontalBox(Setup);
	SetupSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	SetupSlot->SetVerticalAlignment(VAlign_Bottom);

	// The two starting Flux Spell slots, each opening its picker, then Lock In (UX 36; ADR-015 §7).
	UHorizontalBox* Loadout = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (const FVeyraSpellSlotModel& SlotModel : Model.SpellSlots)
	{
		UVerticalBox* Tile = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		AddLine(*WidgetTree, *Tile, SlotModel.Title, EVeyraShellText::Small, HAlign_Center);
		AddLine(*WidgetTree, *Tile, SlotModel.Chosen, EVeyraShellText::Body, HAlign_Center);
		USizeBox* Box = Sized(*WidgetTree, *Tile, FVector2D(Settings.SpellTileSize, Settings.SpellTileSize));
		const int32 SpellSlot = SlotModel.Slot;
		AddContentButton(*Loadout, SlotModel.Title, *Box, [this, SpellSlot] { OpenSpellPicker(SpellSlot); }, Model.bCanChooseSpells,
			OpenSpellSlot == SpellSlot);
	}
	const FString LockInId = Model.LockInVanguardId;
	UTextBlock* LockInText = VeyraShellStyle::MakeText(*WidgetTree, LOCTEXT("LockInTile", "LOCK IN"), EVeyraShellText::Heading);
	LockInText->SetJustification(ETextJustify::Center);
	if (Model.bCanLockIn)
	{
		// Dark on the gold it is filled with while it can be pressed, as League's reads.
		LockInText->SetColorAndOpacity(FSlateColor(Settings.BackgroundColor));
	}
	USizeBox* LockInBox = Sized(*WidgetTree, *LockInText, FVector2D(Settings.LockInWidth, Settings.SpellTileSize));
	Cast<USizeBoxSlot>(LockInText->Slot)->SetVerticalAlignment(VAlign_Center);
	AddContentButton(*Loadout, LOCTEXT("LockIn", "Lock In"), *LockInBox, [this, LockInId] { Client->LockVanguard(LockInId); }, Model.bCanLockIn, Model.bCanLockIn);
	UHorizontalBoxSlot* LoadoutSlot = Footer->AddChildToHorizontalBox(Loadout);
	LoadoutSlot->SetVerticalAlignment(VAlign_Bottom);

	UVerticalBox* Mode = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (Model.bOffersLeave)
	{
		UVeyraShellButton* Leave = AddButton(*Mode, LOCTEXT("LeaveSelect", "Leave"), [this] { Client->LeaveSelect(); }, Model.bCanLeave);
		Cast<UVerticalBoxSlot>(Leave->Slot)->SetHorizontalAlignment(HAlign_Right);
		Leave->KeepLabelOnOneLine();
	}
	AddLine(*WidgetTree, *Mode, Model.ModeLabel, EVeyraShellText::Heading, HAlign_Right);
	UHorizontalBoxSlot* ModeSlot = Footer->AddChildToHorizontalBox(Mode);
	ModeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ModeSlot->SetVerticalAlignment(VAlign_Bottom);
	return *Footer;
}

void UVeyraShellScreen::BuildSpellPicker(const FVeyraSelectModel& Model)
{
	if (!Model.SpellSlots.IsValidIndex(OpenSpellSlot))
	{
		OpenSpellSlot = INDEX_NONE;
		return;
	}
	const UVeyraShellStyleSettings& Settings = Style();
	const FVeyraSpellSlotModel& SlotModel = Model.SpellSlots[OpenSpellSlot];
	// League's summoner spell picker: over everything, until a choice or Close.
	UBorder* Scrim = VeyraShellStyle::MakeBorder(*WidgetTree, Settings.MenuScrimColor, 0.0f);
	Scrim->SetHorizontalAlignment(HAlign_Center);
	Scrim->SetVerticalAlignment(VAlign_Center);
	UOverlaySlot* ScrimSlot = Popup->AddChildToOverlay(Scrim);
	ScrimSlot->SetHorizontalAlignment(HAlign_Fill);
	ScrimSlot->SetVerticalAlignment(VAlign_Fill);

	UBorder* Panel = VeyraShellStyle::MakeSurface(*WidgetTree, VeyraShellStyle::EVeyraShellSurface::Raised, FMargin(Settings.Spacing * 2.0f));
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Rows);
	USizeBox* PanelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PanelBox->SetWidthOverride(Settings.PickerWidth);
	PanelBox->AddChild(Panel);
	Scrim->SetContent(PanelBox);

	AddText(*Rows, SlotModel.Title, static_cast<uint8>(EVeyraShellText::Heading));
	AddText(*Rows, SlotModel.Unlock, static_cast<uint8>(EVeyraShellText::Muted));
	UWrapBox* Choices = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	for (const FVeyraSpellChoiceModel& Choice : SlotModel.Choices)
	{
		UVerticalBox* Tile = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		AddLine(*WidgetTree, *Tile, Choice.Name, EVeyraShellText::Heading);
		if (!Choice.Description.IsEmpty())
		{
			AddLine(*WidgetTree, *Tile, Choice.Description, EVeyraShellText::Small, HAlign_Left, /*bWrap*/ true);
		}
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(Settings.PickerTileWidth);
		Box->AddChild(Tile);
		const int32 SpellSlot = SlotModel.Slot;
		const FString SpellId = Choice.SpellId;
		AddContentButton(*Choices, Choice.Name, *Box, [this, SpellSlot, SpellId] {
			// Closed first, so the rebuild the choice starts shows the screen without the picker.
			OpenSpellSlot = INDEX_NONE;
			Client->ChooseFluxSpell(SpellSlot, SpellId);
			Refresh();
		}, Model.bCanChooseSpells, Choice.bChosen);
	}
	VeyraShellStyle::AddSpaced(*Rows, *Choices);
	AddButton(*Rows, LOCTEXT("CloseSpellPicker", "Close"), [this] {
		OpenSpellSlot = INDEX_NONE;
		Refresh();
	});
}

void UVeyraShellScreen::OpenSpellPicker(int32 SpellSlot)
{
	OpenSpellSlot = OpenSpellSlot == SpellSlot ? INDEX_NONE : SpellSlot;
	Refresh();
}

void UVeyraShellScreen::ShowBackdrop(UTexture2D* Hero)
{
	if (!Hero)
	{
		BackdropBox->SetVisibility(ESlateVisibility::Collapsed);
		Backdrop->SetBrush(FSlateBrush());
		return;
	}
	const FIntPoint Pixels = SizeOf(*Hero);
	FSlateBrush Brush;
	Brush.SetResourceObject(Hero);
	Brush.ImageSize = FVector2D(Pixels.X, Pixels.Y);
	Backdrop->SetBrush(Brush);
	// Dimmed, so the columns and text read over it.
	Backdrop->SetColorAndOpacity(Style().BackdropTint);
	BackdropBox->SetVisibility(ESlateVisibility::HitTestInvisible);
}

UTexture2D* UVeyraShellScreen::GetBackdrop() const
{
	return Backdrop && BackdropBox && BackdropBox->GetVisibility() != ESlateVisibility::Collapsed ? Cast<UTexture2D>(Backdrop->GetBrush().GetResourceObject()) : nullptr;
}

void UVeyraShellScreen::UpdatePickBars()
{
	const float Fraction = PickSeconds > 0.0 ? static_cast<float>(FMath::Clamp(Client->GetRemainingPickSeconds() / PickSeconds, 0.0, 1.0)) : 0.0f;
	for (const TObjectPtr<UProgressBar>& Bar : PickBars)
	{
		Bar->SetPercent(Fraction);
	}
}

UVeyraShellButton* UVeyraShellScreen::AddContentButton(UPanelWidget& Parent, const FText& Label, UWidget& ButtonContent, TFunction<void()> Action, bool bEnabled,
	bool bSelected)
{
	UVeyraShellButton* Button = UVeyraShellButton::MakeWithContent(*WidgetTree, Label, ButtonContent, MoveTemp(Action), bEnabled, bSelected);
	Buttons.Add(Button);
	VeyraShellStyle::AddSpaced(Parent, *Button);
	return Button;
}

#undef LOCTEXT_NAMESPACE
