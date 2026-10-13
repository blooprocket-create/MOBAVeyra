// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraShellStyle.h"
#include "Shell/VeyraShellLook.h"
#include "Shell/VeyraShellBrushes.h"
#include "Shell/VeyraShellFonts.h"
#include "Shell/VeyraShellSurface.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
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

	/** Fill darkened towards its foot by the style's SurfaceShade: a surface's depth (ADR-078 §3). */
	FLinearColor Shaded(const FLinearColor& Fill)
	{
		const float Foot = 1.0f - Settings().SurfaceShade;
		return FLinearColor(Fill.R * Foot, Fill.G * Foot, Fill.B * Foot, Fill.A);
	}

	/**
	 * A chamfered button face of Fill, edged in Edge (none when clear). Its outline settings describe the edge baked into
	 * it, which the enhanced focus outline replaces (ADR-055 §3).
	 */
	FSlateBrush ButtonBrush(const FLinearColor& Fill, const FLinearColor& Edge)
	{
		const UVeyraShellStyleSettings& Style = Settings();
		FVeyraFaceLook Look;
		Look.Top = Fill;
		Look.Bottom = Shaded(Fill);
		Look.Edge = Edge;
		Look.Chamfer = Style.ButtonChamfer;
		FSlateBrush Brush = VeyraShellBrushes::Face(Look);
		Brush.OutlineSettings.Color = FSlateColor(Edge);
		Brush.OutlineSettings.Width = Edge.A > 0.0f ? Look.EdgeWidth : 0.0f;
		return Brush;
	}

	/** The primary action's brass, from Head to Foot, with the shine along its top. */
	FSlateBrush PrimaryBrush(const FLinearColor& Head, const FLinearColor& Foot, bool bShine)
	{
		const UVeyraShellStyleSettings& Style = Settings();
		FVeyraFaceLook Look;
		Look.Top = Head;
		Look.Bottom = Foot;
		Look.Shine = bShine ? Style.PrimaryShineColor : FLinearColor::Transparent;
		Look.Chamfer = Style.ButtonChamfer;
		return VeyraShellBrushes::Face(Look);
	}

	// A texture's side along its gradient, in texels: enough for a smooth ramp once filtered.
	constexpr int32 GradientTexels = 256;
}

/** Role's font in the look: at Interface Text Size, and for a chat (bChat) at Chat Text Size on top (ADR-059 §5). */
FSlateFontInfo FontIn(EVeyraShellText Role, bool bChat)
{
	const UVeyraShellStyleSettings& Style = Settings();
	// Each role's family and typeface are the style's (ADR-078 §2); its size and tracking as before.
	const FVeyraTextFace* Face = &Style.BodyFace;
	int32 Size = Style.BodyFontSize;
	int32 Tracking = 0;
	switch (Role)
	{
	case EVeyraShellText::Title:
		Face = &Style.TitleFace;
		Size = Style.TitleFontSize;
		break;
	case EVeyraShellText::Heading:
		Face = &Style.HeadingFace;
		Size = Style.HeadingFontSize;
		break;
	case EVeyraShellText::Body:
	case EVeyraShellText::Muted:
		break;
	case EVeyraShellText::Countdown:
		Face = &Style.CountdownFace;
		Size = Style.CountdownFontSize;
		break;
	case EVeyraShellText::Small:
		Face = &Style.SmallFace;
		Size = Style.SmallFontSize;
		break;
	case EVeyraShellText::Display:
		Face = &Style.DisplayFace;
		Size = Style.DisplayFontSize;
		Tracking = Style.DisplayLetterSpacing;
		break;
	case EVeyraShellText::Eyebrow:
	case EVeyraShellText::Column:
		Face = &Style.EyebrowFace;
		Size = Style.EyebrowFontSize;
		Tracking = Style.EyebrowLetterSpacing;
		break;
	case EVeyraShellText::Button:
		Face = &Style.ButtonFace;
		Size = Style.ButtonFontSize;
		Tracking = Style.ButtonLetterSpacing;
		break;
	case EVeyraShellText::PrimaryButton:
		Face = &Style.PrimaryButtonFace;
		Size = Style.ButtonFontSize;
		Tracking = Style.ButtonLetterSpacing;
		break;
	}
	// The player's Interface Text Size; layouts reflow around it (SET-62).
	Size = bChat ? VeyraShellLook::ScaledChatFontSize(Size) : VeyraShellLook::ScaledFontSize(Size);
	FSlateFontInfo Font = VeyraShellFonts::FontOf(*Face, Size);
	Font.LetterSpacing = Tracking;
	return Font;
}

FSlateFontInfo FontFor(EVeyraShellText Role)
{
	return FontIn(Role, /*bChat*/ false);
}

void SizeForChat(UTextBlock& Block, EVeyraShellText Role)
{
	Block.SetFont(FontIn(Role, /*bChat*/ true));
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
		// Headlines read as written unless the style says otherwise (ADR-078 §2).
		bCapitals = Style.bDisplayInCapitals;
		break;
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
	UVeyraShellSurface* Border = Tree.ConstructWidget<UVeyraShellSurface>(UVeyraShellSurface::StaticClass());
	StyleSurface(*Border, Surface);
	Border->SetPadding(Padding);
	return Border;
}

void StyleSurface(UBorder& Border, EVeyraShellSurface Surface)
{
	const UVeyraShellStyleSettings& Style = Settings();
	// The fill is tinted its colour, opaque under Reduce Interface Transparency (SET-75); the edge is the brass hairline
	// with its corner ticks (ADR-078 §3).
	FSlateBrush Fill = VeyraShellBrushes::Mask(Style.PanelChamfer, Style.SurfaceShade);
	Fill.TintColor = FSlateColor(VeyraShellLook::Panel(Surface == EVeyraShellSurface::Raised ? Style.SurfaceRaisedColor : Style.SurfaceColor));
	Border.SetBrush(Fill);
	if (UVeyraShellSurface* Edged = Cast<UVeyraShellSurface>(&Border))
	{
		Edged->SetOutline(VeyraShellBrushes::Outline(Style.PanelChamfer, Style.EdgeColor, Style.EdgeTickColor));
	}
}

void StyleSurface(UBorder& Border, const FLinearColor& Fill, const FLinearColor& Edge)
{
	const UVeyraShellStyleSettings& Style = Settings();
	FSlateBrush Brush = VeyraShellBrushes::Mask(Style.PanelChamfer, Style.SurfaceShade);
	Brush.TintColor = FSlateColor(Fill);
	Border.SetBrush(Brush);
	if (UVeyraShellSurface* Edged = Cast<UVeyraShellSurface>(&Border))
	{
		Edged->SetOutline(VeyraShellBrushes::Outline(Style.PanelChamfer, Edge, Edge));
	}
}

FSlateBrush EdgedFace(const FLinearColor& Fill, const FLinearColor& Edge)
{
	return ButtonBrush(Fill, Edge);
}

void StyleTextField(UEditableTextBox& Box, float Padding)
{
	const UVeyraShellStyleSettings& Style = Settings();
	FEditableTextBoxStyle FieldStyle = Box.GetWidgetStyle();
	const FLinearColor Fill = VeyraShellLook::Panel(Style.SurfaceRaisedColor);
	const FVeyraFocusEdge Edge = VeyraShellLook::FieldFocusEdge();
	FVeyraFaceLook Look;
	Look.Top = Fill;
	Look.Bottom = Shaded(Fill);
	Look.Edge = Style.HairlineColor;
	Look.Chamfer = Style.ButtonChamfer;
	const FSlateBrush Plain = VeyraShellBrushes::Face(Look);
	Look.Edge = Edge.Color;
	Look.EdgeWidth = Edge.Width;
	const FSlateBrush Focused = VeyraShellBrushes::Face(Look);
	FieldStyle.SetBackgroundImageNormal(Plain);
	FieldStyle.SetBackgroundImageHovered(Plain);
	FieldStyle.SetBackgroundImageFocused(Focused);
	FieldStyle.SetBackgroundImageReadOnly(Plain);
	FieldStyle.SetForegroundColor(FSlateColor(Style.TextColor));
	FieldStyle.SetFocusedForegroundColor(FSlateColor(Style.TextColor));
	FieldStyle.SetPadding(FMargin(Padding));
	FieldStyle.SetFont(FontFor(EVeyraShellText::Body));
	Box.SetWidgetStyle(FieldStyle);
}

void StyleChatField(UEditableTextBox& Box, float Padding)
{
	StyleTextField(Box, Padding);
	FEditableTextBoxStyle FieldStyle = Box.GetWidgetStyle();
	FieldStyle.SetFont(FontIn(EVeyraShellText::Body, /*bChat*/ true));
	Box.SetWidgetStyle(FieldStyle);
}

UWidget* MakeRule(UWidgetTree& Tree)
{
	USizeBox* Height = Tree.ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Height->SetHeightOverride(1.0f);
	Height->AddChild(MakeBorder(Tree, Settings().HairlineColor, 0.0f));
	return Height;
}

UWidget* MakeGlyph(UWidgetTree& Tree, const FSlateBrush& Brush)
{
	UImage* Glyph = Tree.ConstructWidget<UImage>(UImage::StaticClass());
	Glyph->SetBrush(Brush);
	Glyph->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Glyph;
}

UWidget* MakeInitialTile(UWidgetTree& Tree, const FText& Name, float Size)
{
	const UVeyraShellStyleSettings& Style = Settings();
	USizeBox* Box = Tree.ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetWidthOverride(Size);
	Box->SetHeightOverride(Size);
	UBorder* Tile = Tree.ConstructWidget<UBorder>(UBorder::StaticClass());
	Tile->SetBrush(EdgedFace(VeyraShellLook::Panel(Style.SurfaceRaisedColor), Style.ButtonEdgeColor));
	Tile->SetHorizontalAlignment(HAlign_Center);
	Tile->SetVerticalAlignment(VAlign_Center);
	Tile->SetPadding(FMargin(0.0f));
	const FString Letter = Name.ToString().Left(1).ToUpper();
	UTextBlock* Initial = MakeText(Tree, FText::FromString(Letter), EVeyraShellText::Heading);
	// The display face, at about half the tile, whatever the player's text size: it is a mark, not a word.
	Initial->SetFont(VeyraShellFonts::FontOf(Style.DisplayFace, FMath::RoundToFloat(Size * 0.45f)));
	Initial->SetAutoWrapText(false);
	Tile->SetContent(Initial);
	Box->AddChild(Tile);
	return Box;
}

UWidget* MakeDot(UWidgetTree& Tree, const FLinearColor& Color, float Size)
{
	USizeBox* Box = Tree.ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetWidthOverride(Size);
	Box->SetHeightOverride(Size);
	UBorder* Dot = Tree.ConstructWidget<UBorder>(UBorder::StaticClass());
	Dot->SetBrush(FSlateRoundedBoxBrush(Color, Size / 2.0f));
	Box->AddChild(Dot);
	return Box;
}

UWidget* MakeMeter(UWidgetTree& Tree, float Fraction, const FLinearColor& Fill, const FLinearColor& Track, float Width, float Height)
{
	const float Filled = FMath::Clamp(Fraction, 0.0f, 1.0f);
	USizeBox* Box = Tree.ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetWidthOverride(Width);
	Box->SetHeightOverride(Height);
	UHorizontalBox* Line = Tree.ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Box->AddChild(Line);
	// Two parts sharing the width by their fractions; an empty part takes no room.
	for (const TPair<FLinearColor, float>& Part : { TPair<FLinearColor, float>(Fill, Filled), TPair<FLinearColor, float>(Track, 1.0f - Filled) })
	{
		UHorizontalBoxSlot* Slot = Line->AddChildToHorizontalBox(MakeBorder(Tree, Part.Key, 0.0f));
		FSlateChildSize Size(ESlateSizeRule::Fill);
		Size.Value = Part.Value;
		Slot->SetSize(Size);
		Slot->SetVerticalAlignment(VAlign_Fill);
	}
	return Box;
}

UTexture2D* CreateGradient(EVeyraShellGradient Gradient)
{
	const bool bAcross = Gradient == EVeyraShellGradient::FromLeft || Gradient == EVeyraShellGradient::FromRight;
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
		const float FromEdge = Gradient == EVeyraShellGradient::FromBottom || Gradient == EVeyraShellGradient::FromRight ? 1.0f - Along : Along;
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
		else if (Gradient == EVeyraShellGradient::FromRight)
		{
			Hold = 0.06f;
			Reach = 0.36f;
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
	// A brass hairline at rest, the accent under the pointer, none while unavailable (ADR-078 §3).
	Button.SetNormal(ButtonBrush(Base, Style.ButtonEdgeColor));
	Button.SetHovered(ButtonBrush(Style.ButtonHoveredColor, Style.AccentColor));
	Button.SetPressed(ButtonBrush(Style.ButtonPressedColor, Style.ButtonEdgeColor));
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
		// Brass from a lit head to a shaded foot; brighter under the pointer, sunk when pressed.
		Button.SetNormal(PrimaryBrush(Style.PrimaryHighlightColor, Style.PrimaryShadeColor, /*bShine*/ true));
		Button.SetHovered(PrimaryBrush(Style.PrimaryHighlightColor, Style.PrimaryHoveredColor, /*bShine*/ true));
		Button.SetPressed(PrimaryBrush(Style.PrimaryColor, Style.PrimaryShadeColor, /*bShine*/ false));
		Button.SetDisabled(PrimaryBrush(Style.PrimaryColor.CopyWithNewOpacity(0.25f), Style.PrimaryShadeColor.CopyWithNewOpacity(0.25f), /*bShine*/ false));
		break;
	case EVeyraShellButton::Tab:
		// The shown tab is underlined in the accent; another shows a hairline under the pointer.
		Button.SetNormal(bSelected ? VeyraShellBrushes::Underline(Style.TabUnderlineHeight, Style.AccentColor) : VeyraShellBrushes::None());
		Button.SetHovered(VeyraShellBrushes::Underline(Style.TabUnderlineHeight, bSelected ? Style.AccentColor : Style.ButtonEdgeColor));
		Button.SetPressed(VeyraShellBrushes::Underline(Style.TabUnderlineHeight, Style.AccentColor));
		Button.SetDisabled(VeyraShellBrushes::None());
		break;
	case EVeyraShellButton::Quiet:
		// Text until the pointer finds it; lit while it is the one shown.
		Button.SetNormal(bSelected ? ButtonBrush(Style.SelectedColor, FLinearColor::Transparent) : VeyraShellBrushes::None());
		Button.SetHovered(ButtonBrush(bSelected ? Style.SelectedColor : Style.ButtonHoveredColor, FLinearColor::Transparent));
		Button.SetPressed(ButtonBrush(Style.ButtonPressedColor, FLinearColor::Transparent));
		Button.SetDisabled(VeyraShellBrushes::None());
		break;
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
