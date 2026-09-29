// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraShellStyle.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"

namespace VeyraShellStyle
{
UTextBlock* MakeText(UWidgetTree& Tree, const FText& Text, EVeyraShellText Role)
{
	const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
	int32 Size = Style.BodyFontSize;
	FLinearColor Color = Style.TextColor;
	const TCHAR* Typeface = TEXT("Regular");
	switch (Role)
	{
	case EVeyraShellText::Title:
		Size = Style.TitleFontSize;
		Color = Style.AccentColor;
		Typeface = TEXT("Bold");
		break;
	case EVeyraShellText::Heading:
		Size = Style.HeadingFontSize;
		Typeface = TEXT("Bold");
		break;
	case EVeyraShellText::Body:
		break;
	case EVeyraShellText::Muted:
		Color = Style.MutedTextColor;
		break;
	case EVeyraShellText::Countdown:
		Size = Style.CountdownFontSize;
		Color = Style.AccentColor;
		Typeface = TEXT("Bold");
		break;
	case EVeyraShellText::Small:
		Size = Style.SmallFontSize;
		break;
	}
	UTextBlock* Block = Tree.ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(Text);
	Block->SetFont(FCoreStyle::GetDefaultFontStyle(Typeface, Size));
	Block->SetColorAndOpacity(FSlateColor(Color));
	Block->SetAutoWrapText(true);
	return Block;
}

UBorder* MakeBorder(UWidgetTree& Tree, const FLinearColor& Color, float Padding)
{
	UBorder* Border = Tree.ConstructWidget<UBorder>(UBorder::StaticClass());
	Border->SetBrush(FSlateColorBrush(FLinearColor::White));
	Border->SetBrushColor(Color);
	Border->SetPadding(FMargin(Padding));
	return Border;
}

void AddSpaced(UPanelWidget& Parent, UWidget& Child)
{
	const float Spacing = GetDefault<UVeyraShellStyleSettings>()->Spacing;
	if (UVerticalBox* Column = Cast<UVerticalBox>(&Parent))
	{
		UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(&Child);
		Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, Spacing));
		// A button keeps its label's width; text and panels fill the column.
		if (Child.IsA<UButton>())
		{
			Slot->SetHorizontalAlignment(HAlign_Left);
		}
	}
	else if (UHorizontalBox* Row = Cast<UHorizontalBox>(&Parent))
	{
		Row->AddChildToHorizontalBox(&Child)->SetPadding(FMargin(0.0f, 0.0f, Spacing, 0.0f));
	}
	else if (UWrapBox* Wrap = Cast<UWrapBox>(&Parent))
	{
		Wrap->AddChildToWrapBox(&Child)->SetPadding(FMargin(0.0f, 0.0f, Spacing, Spacing));
	}
	else
	{
		Parent.AddChild(&Child);
	}
}

FButtonStyle ButtonStyle(const FLinearColor& Base, float Padding)
{
	const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
	FButtonStyle Button;
	Button.SetNormal(FSlateColorBrush(Base));
	Button.SetHovered(FSlateColorBrush(Style.ButtonHoveredColor));
	Button.SetPressed(FSlateColorBrush(Style.ButtonPressedColor));
	Button.SetDisabled(FSlateColorBrush(Style.ButtonDisabledColor));
	Button.SetNormalPadding(FMargin(Padding));
	Button.SetPressedPadding(FMargin(Padding));
	return Button;
}
}
