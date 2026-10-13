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
#include "Shell/VeyraShellLook.h"
#include "Shell/VeyraShellStyle.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "Text/VeyraContentText.h"

#define LOCTEXT_NAMESPACE "VeyraScoreboard"

bool UVeyraScoreboard::Initialize()
{
	const bool bFirst = Super::Initialize();
	if (bFirst && WidgetTree && !WidgetTree->RootWidget)
	{
		// Centred over the match, which stays in play: clicks pass through to it. Part of the match's HUD, it keeps
		// the HUD's own text and panels, whatever look the menus take (ADR-055 §2).
		const FVeyraScopedShellLook Hud{ FVeyraShellLook() };
		SetVisibility(ESlateVisibility::HitTestInvisible);
		const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
		UOverlay* Screen = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(Style.ScoreboardWidth);
		UBorder* Panel = VeyraShellStyle::MakeSurface(*WidgetTree, VeyraShellStyle::EVeyraShellSurface::Raised, FMargin(Style.Spacing * 2.0f));
		// Both teams stacked, the viewer's first, their columns lined up (ADR-082 §8).
		Board = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Board);
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
	ItemTiles = 0;
	if (!Board)
	{
		return;
	}
	Board->ClearChildren();
	const FVeyraScopedShellLook Hud{ FVeyraShellLook() };
	BuiltLook = VeyraShellLook::Current();
	const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
	const FBox2f Whole(FVector2f::ZeroVector, FVector2f::UnitVector);
	// A fixed-width cell of Child in Row, centred on the row's line.
	const auto AddCell = [this](UHorizontalBox& Row, UWidget& Child, float Width) {
		USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Cell->SetWidthOverride(Width);
		Cell->AddChild(&Child);
		Row.AddChildToHorizontalBox(Cell)->SetVerticalAlignment(VAlign_Center);
	};
	const auto AddGap = [this](UHorizontalBox& Row, float Width) {
		USizeBox* Gap = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Gap->SetWidthOverride(Width);
		Row.AddChildToHorizontalBox(Gap);
	};
	const auto Text = [this](const FText& Words, VeyraShellStyle::EVeyraShellText Role, const FLinearColor* Color = nullptr) {
		UTextBlock* Made = VeyraShellStyle::MakeText(*WidgetTree, Words, Role);
		Made->SetAutoWrapText(false);
		if (Color)
		{
			Made->SetColorAndOpacity(FSlateColor(*Color));
		}
		return Made;
	};
	// The columns, as the results' scoreboard sets them (ADR-078 §5): the Vanguard and its player beside the portrait, the
	// figures, then the build as art (ADR-082 §8).
	const float SideBar = 3.0f;
	const float Portrait = Style.ReportPortraitSize;
	const float NameWidth = Style.ReportLabelWidth;
	const float FigureWidth = Style.ReportColumnWidth * 0.75f;
	const float RowInset = FMath::Max(Style.Spacing / 2.0f, Style.PanelChamfer * 0.75f);
	for (int32 SideIndex = 0; SideIndex < View.Sides.Num(); ++SideIndex)
	{
		const FVeyraScoreboardSide& Side = View.Sides[SideIndex];
		const FLinearColor SideColor = Side.bAllies ? Sides.Ally : Sides.Enemy;
		// The team's heading in its side's colour, its kills in it; the column heads over the first team only.
		UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		AddGap(*Head, RowInset);
		UTextBlock* Heading = Text(SideHeading(Side), VeyraShellStyle::EVeyraShellText::Heading, &SideColor);
		AddCell(*Head, *Heading, SideBar + Style.Spacing + Portrait + Style.Spacing + NameWidth);
		if (SideIndex == 0)
		{
			for (const TPair<FText, float>& Column : { TPair<FText, float>(LOCTEXT("ColKda", "K / D / A"), FigureWidth), TPair<FText, float>(LOCTEXT("ColCs", "CS"), FigureWidth),
					 TPair<FText, float>(LOCTEXT("ColBounty", "Bounty"), FigureWidth), TPair<FText, float>(LOCTEXT("ColBuild", "Build"), Style.ReportIconSize * 6.0f) })
			{
				AddCell(*Head, *Text(Column.Key, VeyraShellStyle::EVeyraShellText::Column), Column.Value);
			}
		}
		VeyraShellStyle::AddSpaced(*Board, *Head);
		Board->AddChild(VeyraShellStyle::MakeRule(*WidgetTree));
		for (const FVeyraScoreboardRow& Row : Side.Rows)
		{
			// The player's own row stands out on a raised surface edged in the accent; every row is inset alike, so the
			// columns line up.
			UBorder* Surface = Row.bLocal ? VeyraShellStyle::MakeSurface(*WidgetTree, VeyraShellStyle::EVeyraShellSurface::Raised, FMargin(RowInset))
										  : VeyraShellStyle::MakeBorder(*WidgetTree, FLinearColor::Transparent, RowInset);
			if (Row.bLocal)
			{
				VeyraShellStyle::StyleSurface(*Surface, Style.SurfaceRaisedColor, Style.AccentColor.CopyWithNewOpacity(0.6f));
			}
			UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			Surface->SetContent(Line);
			// Its side's bar, then the Vanguard's face with its level on a plate at its corner.
			USizeBox* Bar = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Bar->SetWidthOverride(SideBar);
			Bar->SetHeightOverride(Portrait);
			Bar->AddChild(VeyraShellStyle::MakeBorder(*WidgetTree, SideColor.CopyWithNewOpacity(Row.bAway ? 0.3f : 0.9f), 0.0f));
			Line->AddChildToHorizontalBox(Bar)->SetVerticalAlignment(VAlign_Center);
			AddGap(*Line, Style.Spacing);
			const FString VanguardId = Row.Vanguard.ToString();
			UTexture2D* Hero = VeyraShellArt::HeroOf(VanguardId);
			const FBox2f Crop = Hero ? VeyraShellArt::Crop(VanguardId, Hero->GetSizeX(), Hero->GetSizeY(), 1.0f, true) : Whole;
			UOverlay* FaceStack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
			USizeBox* FaceBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			FaceBox->SetWidthOverride(Portrait);
			FaceBox->SetHeightOverride(Portrait);
			UImage* Face = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			Face->SetBrush(VeyraShellArt::Brush(Hero, Crop, FVector2D(Portrait), Style.ButtonCornerRadius, Style.SurfaceColor, SideColor, 1.0f));
			if (Row.bAway)
			{
				Face->SetColorAndOpacity(Style.MutedTextColor);
			}
			FaceBox->AddChild(Face);
			FaceStack->AddChildToOverlay(FaceBox);
			UBorder* Badge = VeyraShellStyle::MakeBorder(*WidgetTree, Style.SurfaceColor, 1.0f);
			VeyraShellStyle::StyleSurface(*Badge, Style.SurfaceColor.CopyWithNewOpacity(1.0f), Style.PrimaryColor);
			Badge->SetPadding(FMargin(4.0f, 0.0f));
			Badge->SetContent(Text(FText::AsNumber(Row.Level), VeyraShellStyle::EVeyraShellText::Small));
			if (UOverlaySlot* Corner = FaceStack->AddChildToOverlay(Badge))
			{
				Corner->SetHorizontalAlignment(HAlign_Right);
				Corner->SetVerticalAlignment(VAlign_Bottom);
			}
			Line->AddChildToHorizontalBox(FaceStack)->SetVerticalAlignment(VAlign_Center);
			AddGap(*Line, Style.Spacing);
			// The Vanguard over its player; the player's own name in the accent.
			UVerticalBox* Who = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			Who->AddChild(Text(VanguardLine(Row), VeyraShellStyle::EVeyraShellText::Heading));
			const FLinearColor NameColor = Row.bLocal ? Style.AccentColor : Style.MutedTextColor;
			Who->AddChild(Text(RowLine(Row), VeyraShellStyle::EVeyraShellText::Small, &NameColor));
			AddCell(*Line, *Who, NameWidth);
			// Its figures: K / D / A, creep score, and its bounty in brass while it has one.
			AddCell(*Line, *Text(VeyraScoreboardModel::KdaText(Row), VeyraShellStyle::EVeyraShellText::Body), FigureWidth);
			AddCell(*Line, *Text(FText::AsNumber(Row.CreepScore), VeyraShellStyle::EVeyraShellText::Body), FigureWidth);
			const FLinearColor BountyColor = Style.PrimaryColor;
			AddCell(*Line, *Text(Row.Bounty > 0 ? FText::AsNumber(Row.Bounty) : FText::FromString(TEXT("-")), VeyraShellStyle::EVeyraShellText::Body,
				Row.Bounty > 0 ? &BountyColor : &Style.MutedTextColor), FigureWidth);
			// The build as art, each slot's item in its tooltip and the whole build in the row's.
			for (const FVeyraContentId& Item : Row.Items)
			{
				USizeBox* Tile = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				Tile->SetWidthOverride(Style.ReportIconSize);
				Tile->SetHeightOverride(Style.ReportIconSize);
				UImage* Art = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
				const FString ItemId = Item.IsValid() ? Item.ToString() : FString();
				Art->SetBrush(VeyraShellArt::Brush(ItemId.IsEmpty() ? nullptr : VeyraShellArt::ItemIconOf(ItemId), Whole, FVector2D(Style.ReportIconSize), Style.ButtonCornerRadius,
					Style.SurfaceColor, Style.HairlineColor, 1.0f));
				if (Item.IsValid())
				{
					Art->SetToolTipText(VeyraContentText::ItemName(Item));
				}
				Tile->AddChild(Art);
				UHorizontalBoxSlot* TileSlot = Line->AddChildToHorizontalBox(Tile);
				TileSlot->SetVerticalAlignment(VAlign_Center);
				TileSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
				++ItemTiles;
			}
			Line->SetToolTipText(ItemsLine(Row));
			Board->AddChild(Surface);
		}
		if (SideIndex + 1 < View.Sides.Num())
		{
			USizeBox* Between = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Between->SetHeightOverride(Style.Spacing * 1.5f);
			Board->AddChild(Between);
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

FText UVeyraScoreboard::VanguardLine(const FVeyraScoreboardRow& Row)
{
	return Row.Vanguard.IsValid() ? VeyraContentText::VanguardName(Row.Vanguard) : LOCTEXT("NoVanguard", "No Vanguard");
}

FText UVeyraScoreboard::RowLine(const FVeyraScoreboardRow& Row)
{
	const FText Name = FText::FromString(Row.Name);
	return Row.bAway ? FText::Format(LOCTEXT("RowAway", "{0}   (disconnected)"), Name) : Name;
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
