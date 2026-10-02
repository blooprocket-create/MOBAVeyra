// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraShellStyle.h"
#include "Shell/VeyraShellLook.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"

namespace VeyraShellStyle
{
namespace
{
	const UVeyraShellStyleSettings& Settings()
	{
		return *GetDefault<UVeyraShellStyleSettings>();
	}

	/** A rounded box of Fill, outlined by Outline, the style's button corners. */
	FSlateRoundedBoxBrush ButtonBrush(const FLinearColor& Fill, const FLinearColor& Outline)
	{
		return FSlateRoundedBoxBrush(Fill, Settings().ButtonCornerRadius, Outline, Outline.A > 0.0f ? 1.0f : 0.0f);
	}

	// A texture's side along its gradient, in texels: enough for a smooth ramp once filtered.
	constexpr int32 GradientTexels = 256;
}

FSlateFontInfo FontFor(EVeyraShellText Role)
{
	const UVeyraShellStyleSettings& Style = Settings();
	const TCHAR* Typeface = TEXT("Regular");
	int32 Size = Style.BodyFontSize;
	int32 Tracking = 0;
	switch (Role)
	{
	case EVeyraShellText::Title:
		Typeface = TEXT("Bold");
		Size = Style.TitleFontSize;
		break;
	case EVeyraShellText::Heading:
		Typeface = TEXT("Bold");
		Size = Style.HeadingFontSize;
		break;
	case EVeyraShellText::Body:
	case EVeyraShellText::Muted:
		break;
	case EVeyraShellText::Countdown:
		Typeface = TEXT("Light");
		Size = Style.CountdownFontSize;
		break;
	case EVeyraShellText::Small:
		Size = Style.SmallFontSize;
		break;
	case EVeyraShellText::Display:
		Typeface = TEXT("Black");
		Size = Style.DisplayFontSize;
		Tracking = Style.DisplayLetterSpacing;
		break;
	case EVeyraShellText::Eyebrow:
	case EVeyraShellText::Column:
		Typeface = TEXT("Bold");
		Size = Style.EyebrowFontSize;
		Tracking = Style.EyebrowLetterSpacing;
		break;
	case EVeyraShellText::Button:
		Typeface = TEXT("Medium");
		Size = Style.ButtonFontSize;
		Tracking = Style.ButtonLetterSpacing;
		break;
	case EVeyraShellText::PrimaryButton:
		Typeface = TEXT("Bold");
		Size = Style.ButtonFontSize;
		Tracking = Style.ButtonLetterSpacing;
		break;
	}
	// The player's Interface Text Size; layouts reflow around it (SET-62).
	Size = VeyraShellLook::ScaledFontSize(Size);
	FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(Typeface, Size);
	Font.LetterSpacing = Tracking;
	return Font;
}

UTextBlock* MakeText(UWidgetTree& Tree, const FText& Text, EVeyraShellText Role)
{
	const UVeyraShellStyleSettings& Style = Settings();
	FLinearColor Color = Style.TextColor;
	bool bCapitals = false;
	switch (Role)
	{
	case EVeyraShellText::Muted:
	case EVeyraShellText::Column:
		Color = Style.MutedTextColor;
		bCapitals = Role == EVeyraShellText::Column;
		break;
	case EVeyraShellText::Countdown:
	case EVeyraShellText::Eyebrow:
		Color = Style.AccentColor;
		bCapitals = Role == EVeyraShellText::Eyebrow;
		break;
	case EVeyraShellText::Display:
	case EVeyraShellText::Button:
		bCapitals = true;
		break;
	case EVeyraShellText::PrimaryButton:
		Color = Style.PrimaryTextColor;
		bCapitals = true;
		break;
	default:
		break;
	}
	UTextBlock* Block = Tree.ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(Text);
	Block->SetFont(FontFor(Role));
	Block->SetColorAndOpacity(FSlateColor(Color));
	// A headline stays on one line: measured before its capitals, it would wrap early and overlap.
	Block->SetAutoWrapText(Role != EVeyraShellText::Display);
	// Capitals are presentation only: the text itself stays as written, for tests and scripts.
	if (bCapitals)
	{
		Block->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
	}
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

UBorder* MakeSurface(UWidgetTree& Tree, EVeyraShellSurface Surface, const FMargin& Padding)
{
	const UVeyraShellStyleSettings& Style = Settings();
	UBorder* Border = Tree.ConstructWidget<UBorder>(UBorder::StaticClass());
	// Opaque under Reduce Interface Transparency (SET-75).
	const FLinearColor Fill = VeyraShellLook::Panel(Surface == EVeyraShellSurface::Raised ? Style.SurfaceRaisedColor : Style.SurfaceColor);
	Border->SetBrush(FSlateRoundedBoxBrush(Fill, Style.PanelCornerRadius, Style.HairlineColor, 1.0f));
	Border->SetPadding(Padding);
	return Border;
}

UWidget* MakeRule(UWidgetTree& Tree)
{
	USizeBox* Height = Tree.ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Height->SetHeightOverride(1.0f);
	Height->AddChild(MakeBorder(Tree, Settings().HairlineColor, 0.0f));
	return Height;
}

UTexture2D* CreateGradient(EVeyraShellGradient Gradient)
{
	const bool bAcross = Gradient == EVeyraShellGradient::FromLeft;
	const int32 Width = bAcross ? GradientTexels : 1;
	const int32 Height = bAcross ? 1 : GradientTexels;
	UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8, NAME_None);
	if (!Texture)
	{
		return nullptr;
	}
	Texture->Filter = TF_Bilinear;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;
	Texture->SRGB = false;
	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	FColor* Texels = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
	for (int32 Index = 0; Index < GradientTexels; ++Index)
	{
		// 0 at the dark edge, 1 where the scrim has faded out.
		const float Along = static_cast<float>(Index) / (GradientTexels - 1);
		const float FromEdge = Gradient == EVeyraShellGradient::FromBottom ? 1.0f - Along : Along;
		// Solid near its edge, where text sits, then fading: the side one across most of the screen, the
		// bottom one by the middle, the top one behind the bar alone.
		float Hold = 0.12f;
		float Reach = 0.72f;
		if (Gradient == EVeyraShellGradient::FromBottom)
		{
			Hold = 0.0f;
			Reach = 0.5f;
		}
		else if (Gradient == EVeyraShellGradient::FromTop)
		{
			Hold = 0.04f;
			Reach = 0.24f;
		}
		const float Opacity = 1.0f - FMath::SmoothStep(Hold, Reach, FromEdge);
		Texels[Index] = FColor(255, 255, 255, static_cast<uint8>(FMath::RoundToInt(Opacity * 255.0f)));
	}
	Mip.BulkData.Unlock();
	Texture->UpdateResource();
	return Texture;
}

UImage* MakeGradient(UWidgetTree& Tree, UTexture2D* Texture, const FLinearColor& Color)
{
	UImage* Image = Tree.ConstructWidget<UImage>(UImage::StaticClass());
	if (Texture)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Image->SetBrush(Brush);
	}
	Image->SetColorAndOpacity(Color);
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Image;
}

void AddSpaced(UPanelWidget& Parent, UWidget& Child)
{
	const float Spacing = Settings().Spacing;
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
		UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(&Child);
		Slot->SetPadding(FMargin(0.0f, 0.0f, Spacing, 0.0f));
		Slot->SetVerticalAlignment(VAlign_Center);
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
	const UVeyraShellStyleSettings& Style = Settings();
	FButtonStyle Button;
	Button.SetNormal(ButtonBrush(Base, Style.HairlineColor));
	Button.SetHovered(ButtonBrush(Style.ButtonHoveredColor, Style.AccentColor.CopyWithNewOpacity(Style.HairlineColor.A * 3.0f)));
	Button.SetPressed(ButtonBrush(Style.ButtonPressedColor, Style.HairlineColor));
	Button.SetDisabled(ButtonBrush(Style.ButtonDisabledColor, FLinearColor::Transparent));
	Button.SetNormalPadding(FMargin(Padding));
	Button.SetPressedPadding(FMargin(Padding));
	return Button;
}

FButtonStyle ButtonStyleFor(EVeyraShellButton Kind, bool bSelected)
{
	const UVeyraShellStyleSettings& Style = Settings();
	const FMargin Padding(Style.ButtonPadding * 2.0f, Style.ButtonPadding);
	FButtonStyle Button;
	switch (Kind)
	{
	case EVeyraShellButton::Primary:
		Button.SetNormal(ButtonBrush(Style.PrimaryColor, FLinearColor::Transparent));
		Button.SetHovered(ButtonBrush(Style.PrimaryHoveredColor, FLinearColor::Transparent));
		Button.SetPressed(ButtonBrush(Style.PrimaryColor * 0.8f, FLinearColor::Transparent));
		Button.SetDisabled(ButtonBrush(Style.PrimaryColor.CopyWithNewOpacity(0.25f), FLinearColor::Transparent));
		break;
	case EVeyraShellButton::Tab:
	case EVeyraShellButton::Quiet:
	{
		const FLinearColor Lit = bSelected ? Style.SelectedColor : FLinearColor::Transparent;
		Button.SetNormal(ButtonBrush(Lit, FLinearColor::Transparent));
		Button.SetHovered(ButtonBrush(bSelected ? Style.SelectedColor : Style.ButtonHoveredColor, FLinearColor::Transparent));
		Button.SetPressed(ButtonBrush(Style.ButtonPressedColor, FLinearColor::Transparent));
		Button.SetDisabled(ButtonBrush(FLinearColor::Transparent, FLinearColor::Transparent));
		break;
	}
	case EVeyraShellButton::Secondary:
		return ButtonStyle(bSelected ? Style.SelectedColor : Style.ButtonColor, Style.ButtonPadding)
			.SetNormalPadding(Padding)
			.SetPressedPadding(Padding);
	}
	Button.SetNormalPadding(Padding);
	Button.SetPressedPadding(Padding);
	return Button;
}

EVeyraShellText LabelRoleFor(EVeyraShellButton Kind)
{
	return Kind == EVeyraShellButton::Primary ? EVeyraShellText::PrimaryButton : EVeyraShellText::Button;
}
}
