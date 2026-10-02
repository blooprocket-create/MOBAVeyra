// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Scoreboard/VeyraScoreboard.h"
#include "Shell/VeyraShellArt.h"
#include "Engine/Texture2D.h"
#include "Components/Image.h"
#include "Brushes/SlateRoundedBoxBrush.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Shell/VeyraShellStyle.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "Text/VeyraContentText.h"

#define LOCTEXT_NAMESPACE "VeyraScoreboard"

bool UVeyraScoreboard::Initialize()
{
	const bool bFirst = Super::Initialize();
	if (bFirst && WidgetTree && !WidgetTree->RootWidget)
	{
		// Centred over the match, which stays in play: clicks pass through to it.
		SetVisibility(ESlateVisibility::HitTestInvisible);
		const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
		UOverlay* Screen = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(Style.ScoreboardWidth);
		UBorder* Panel = VeyraShellStyle::MakeSurface(*WidgetTree, VeyraShellStyle::EVeyraShellSurface::Raised, FMargin(Style.Spacing * 2.0f));
		Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Panel->SetContent(Columns);
		Size->AddChild(Panel);
		if (UOverlaySlot* Centred = Screen->AddChildToOverlay(Size))
		{
			Centred->SetHorizontalAlignment(HAlign_Center);
			Centred->SetVerticalAlignment(VAlign_Center);
		}
		WidgetTree->RootWidget = Screen;
	}
	return bFirst;
}

void UVeyraScoreboard::Show(const APlayerController& InController)
{
	Controller = &InController;
	bBuilt = false;
	Refresh();
}

void UVeyraScoreboard::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Refresh();
}

void UVeyraScoreboard::Refresh()
{
	const APlayerController* Viewer = Controller.Get();
	const UWorld* World = Viewer ? Viewer->GetWorld() : nullptr;
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}
	TArray<const APlayerState*> Participants;
	for (const APlayerState* Member : GameState->PlayerArray)
	{
		Participants.Add(Member);
	}
	// A client's player list leaves out players who left, whose PlayerStates the server keeps for their
	// return; they keep their line, marked (Match Flow Bible §3; ADR-019 §1).
	for (TActorIterator<APlayerState> It(World); It; ++It)
	{
		if (It->IsInactive() && !Participants.Contains(*It))
		{
			Participants.Add(*It);
		}
	}
	FVeyraScoreboardView Latest = VeyraScoreboardModel::Describe(Participants, Viewer->PlayerState);
	const FVeyraSideColors LatestSides =
		VeyraInterfacePreferences::Resolve(*GetDefault<UVeyraGreyboxSettings>(), SettingsStore ? SettingsStore : VeyraInterfacePreferences::StoreOf(Viewer)).SideColors;
	if (bBuilt && Latest == View && LatestSides == Sides)
	{
		return;
	}
	View = MoveTemp(Latest);
	Sides = LatestSides;
	Rebuild();
}

void UVeyraScoreboard::Rebuild()
{
	bBuilt = true;
	if (!Columns)
	{
		return;
	}
	Columns->ClearChildren();
	const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
	for (const FVeyraScoreboardSide& Side : View.Sides)
	{
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UTextBlock* Heading = VeyraShellStyle::MakeText(*WidgetTree, SideHeading(Side), VeyraShellStyle::EVeyraShellText::Heading);
		Heading->SetColorAndOpacity(Side.bAllies ? Sides.Ally : Sides.Enemy);
		VeyraShellStyle::AddSpaced(*Column, *Heading);
		for (const FVeyraScoreboardRow& Row : Side.Rows)
		{
			// A card for each player: the Vanguard's face, its line and its build; the viewer's own outlined.
			UBorder* Card = VeyraShellStyle::MakeSurface(*WidgetTree, Row.bLocal ? VeyraShellStyle::EVeyraShellSurface::Raised : VeyraShellStyle::EVeyraShellSurface::Panel,
				FMargin(Style.Spacing / 2.0f));
			if (Row.bLocal)
			{
				Card->SetBrush(FSlateRoundedBoxBrush(Style.SurfaceRaisedColor, Style.PanelCornerRadius, Style.AccentColor.CopyWithNewOpacity(0.6f), 1.0f));
			}
			UHorizontalBox* Inside = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			Card->SetContent(Inside);
			const FString VanguardId = Row.Vanguard.ToString();
			UTexture2D* Hero = VeyraShellArt::HeroOf(VanguardId);
			const FVector2D FaceSize(Style.PortraitSize, Style.PortraitSize);
			UImage* Face = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			const FBox2f Crop = Hero ? VeyraShellArt::Crop(VanguardId, Hero->GetSizeX(), Hero->GetSizeY(), 1.0f, true) : FBox2f(FVector2f::ZeroVector, FVector2f::UnitVector);
			Face->SetBrush(VeyraShellArt::Brush(Hero, Crop, FaceSize, Style.ButtonCornerRadius, Style.SurfaceColor, Side.bAllies ? Sides.Ally : Sides.Enemy, 1.0f));
			if (Row.bAway)
			{
				Face->SetColorAndOpacity(Style.MutedTextColor);
			}
			Inside->AddChildToHorizontalBox(Face)->SetVerticalAlignment(VAlign_Center);
			UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			UTextBlock* Line = VeyraShellStyle::MakeText(*WidgetTree, RowLine(Row), VeyraShellStyle::EVeyraShellText::Body);
			if (Row.bLocal)
			{
				Line->SetColorAndOpacity(Style.AccentColor);
			}
			Texts->AddChild(Line);
			UTextBlock* Build = VeyraShellStyle::MakeText(*WidgetTree, ItemsLine(Row), VeyraShellStyle::EVeyraShellText::Small);
			Build->SetColorAndOpacity(FSlateColor(Style.MutedTextColor));
			Texts->AddChild(Build);
			UHorizontalBoxSlot* TextSlot = Inside->AddChildToHorizontalBox(Texts);
			TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			TextSlot->SetVerticalAlignment(VAlign_Center);
			TextSlot->SetPadding(FMargin(Style.Spacing, 0.0f, 0.0f, 0.0f));
			VeyraShellStyle::AddSpaced(*Column, *Card);
		}
		if (UHorizontalBoxSlot* ColumnSlot = Columns->AddChildToHorizontalBox(Column))
		{
			// The two teams share the width evenly.
			ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			ColumnSlot->SetPadding(FMargin(Style.Spacing, 0.0f));
		}
	}
}

TArray<FString> UVeyraScoreboard::GetLines() const
{
	TArray<FString> Lines;
	if (WidgetTree)
	{
		WidgetTree->ForEachWidget([&Lines](UWidget* Widget) {
			if (const UTextBlock* Text = Cast<UTextBlock>(Widget))
			{
				Lines.Add(Text->GetText().ToString());
			}
		});
	}
	return Lines;
}

FText UVeyraScoreboard::SideHeading(const FVeyraScoreboardSide& Side)
{
	return FText::Format(Side.bAllies ? LOCTEXT("Allies", "Your team   {0} kills") : LOCTEXT("Enemies", "Enemy team   {0} kills"), FText::AsNumber(Side.Kills));
}

FText UVeyraScoreboard::RowLine(const FVeyraScoreboardRow& Row)
{
	const FText Vanguard = Row.Vanguard.IsValid() ? VeyraContentText::VanguardName(Row.Vanguard) : LOCTEXT("NoVanguard", "No Vanguard");
	const FText Line = FText::Format(LOCTEXT("Row", "{0}  {1}   Lv {2}   {3}   CS {4}"), Vanguard, FText::FromString(Row.Name), FText::AsNumber(Row.Level),
		VeyraScoreboardModel::KdaText(Row), FText::AsNumber(Row.CreepScore));
	const FText WithBounty = Row.Bounty > 0 ? FText::Format(LOCTEXT("RowBounty", "{0}   Bounty {1}"), Line, FText::AsNumber(Row.Bounty)) : Line;
	return Row.bAway ? FText::Format(LOCTEXT("RowAway", "{0}   (disconnected)"), WithBounty) : WithBounty;
}

FText UVeyraScoreboard::ItemsLine(const FVeyraScoreboardRow& Row)
{
	TArray<FText> Names;
	for (const FVeyraContentId& Item : Row.Items)
	{
		Names.Add(Item.IsValid() ? VeyraContentText::ItemName(Item) : LOCTEXT("EmptySlot", "-"));
	}
	return Names.IsEmpty() ? LOCTEXT("NoItems", "No items") : FText::Join(LOCTEXT("ItemSeparator", "  |  "), Names);
}

#undef LOCTEXT_NAMESPACE
