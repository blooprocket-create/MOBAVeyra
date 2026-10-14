// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Internationalization/Text.h"
#include "Layout/Margin.h"
#include "Math/Color.h"
#include "Shell/VeyraShellButton.h"

class UBorder;
class UEditableTextBox;
class UImage;
class UPanelWidget;
class UTextBlock;
class UTexture2D;
class UWidget;
class UWidgetTree;
struct FButtonStyle;
struct FSlateFontInfo;

/**
 * The shell's widgets in its style (UVeyraShellStyleSettings), built in C++. Its design system follows
 * the Art Bible (v0.1 §4): smoked translucent surfaces with thin outlines, restrained accents, one
 * primary action per screen, and two tiers of type, a display face for identity and a legible face
 * for function. Where the bible leaves UI geometry and fonts open, the values are provisional
 * presentation settings.
 */
namespace VeyraShellStyle
{
	enum class EVeyraShellText : uint8
	{
		Title,
		Heading,
		Body,
		Muted,
		Countdown,
		/** Names under portraits and statuses in champion select. */
		Small,
		/** A screen's headline: the display tier, large, capitals, tracked. */
		Display,
		/** A small label above a title or a group, in the accent: capitals, widely tracked. */
		Eyebrow,
		/** A button's label: capitals, tracked. */
		Button,
		/** A primary button's label, in the dark that reads on the primary colour. */
		PrimaryButton,
		/** A table's column headings: small capitals, muted. */
		Column,
	};

	/** What a button is for (UVeyraShellButton), which sets how loudly it speaks. */
	using EVeyraShellButton = EVeyraShellButtonKind;

	/** How far a surface stands out from what is behind it. */
	enum class EVeyraShellSurface : uint8
	{
		/** A panel or card over the world. */
		Panel,
		/** A header, a bar, a row that stands out. */
		Raised,
	};

	/** Which way a scrim darkens art so text reads over it. */
	enum class EVeyraShellGradient : uint8
	{
		/** Dark at the left edge, clear by the middle. */
		FromLeft,
		/** Dark at the bottom edge, clear by the middle. */
		FromBottom,
		/** Dark at the top edge, clear quickly. */
		FromTop,
		/** Dark at the right edge, clear a third of the way in: champion select's enemy column (ADR-078 §5). */
		FromRight,
	};

	/** The font Role uses. */
	FSlateFontInfo FontFor(EVeyraShellText Role);

	/** A text block for Text in Role's size and colour. */
	UTextBlock* MakeText(UWidgetTree& Tree, const FText& Text, EVeyraShellText Role);

	/** A border filled with Color, padded by the shell's spacing. */
	UBorder* MakeBorder(UWidgetTree& Tree, const FLinearColor& Color, float Padding);

	/**
	 * A chamfered surface of the given kind (ADR-078 §3), padded by Padding: a UVeyraShellSurface, its fill tinted the
	 * surface's colour and its brass edge drawn over it.
	 */
	UBorder* MakeSurface(UWidgetTree& Tree, EVeyraShellSurface Surface, const FMargin& Padding);

	/** Gives Border the surface's fill, and its edge too when it is a UVeyraShellSurface. */
	void StyleSurface(UBorder& Border, EVeyraShellSurface Surface);

	/** As StyleSurface, in Fill with an Edge of its own: a row or card that stands out, such as the player's own. */
	void StyleSurface(UBorder& Border, const FLinearColor& Fill, const FLinearColor& Edge);

	/** A small chamfered face of Fill edged in Edge, for a tile, a swatch or a banner drawn by a plain border. */
	FSlateBrush EdgedFace(const FLinearColor& Fill, const FLinearColor& Edge);

	/** A one-unit-high rule across its slot, in the outline colour. */
	UWidget* MakeRule(UWidgetTree& Tree);

	/** An image of Brush at its own size, which clicks pass through: a mark such as the Flux diamond. */
	UWidget* MakeGlyph(UWidgetTree& Tree, const FSlateBrush& Brush);

	/** A chamfered tile Size across with Name's first letter in the display face: a player, until profiles show icons here. */
	UWidget* MakeInitialTile(UWidgetTree& Tree, const FText& Name, float Size);

	/** A round dot Size across in Color, such as a friend's status beside its words. */
	UWidget* MakeDot(UWidgetTree& Tree, const FLinearColor& Color, float Size);

	/** A line Width by Height filled Fraction (0 to 1) of the way in Fill, over Track: progress toward something. */
	UWidget* MakeMeter(UWidgetTree& Tree, float Fraction, const FLinearColor& Fill, const FLinearColor& Track, float Width, float Height);

	/**
	 * Styles Box as every text field of the client is, in the current look: a raised field, its fill opaque when panels
	 * are, and its focused edge the look's, the enhanced outline under Enhanced focus (ADR-055 §3). Padding is inside it.
	 */
	void StyleTextField(UEditableTextBox& Box, float Padding);

	/** Styles Box as a chat's entry field: StyleTextField's look, its text at Chat Text Size (SET-66; ADR-059 §5). */
	void StyleChatField(UEditableTextBox& Box, float Padding);

	/** Sizes a chat line in Role at Chat Text Size, on top of the look's text size. */
	void SizeForChat(UTextBlock& Block, EVeyraShellText Role);

	/**
	 * A new white gradient texture, opaque at Gradient's dark edge; the image that shows it tints it.
	 * Its holder keeps it alive.
	 */
	UTexture2D* CreateGradient(EVeyraShellGradient Gradient);

	/** An image of Texture tinted Color, stretched over its slot. */
	UImage* MakeGradient(UWidgetTree& Tree, UTexture2D* Texture, const FLinearColor& Color);

	/** Adds Child to Parent, a vertical or horizontal box, followed by the shell's spacing. */
	void AddSpaced(UPanelWidget& Parent, UWidget& Child);

	/** A button style whose background is Base, lighter when hovered and pressed, with Padding around its content. */
	FButtonStyle ButtonStyle(const FLinearColor& Base, float Padding);

	/** A button style for Kind, lit as the shown one when bSelected. */
	FButtonStyle ButtonStyleFor(EVeyraShellButton Kind, bool bSelected);

	/** The text role for Kind's label. */
	EVeyraShellText LabelRoleFor(EVeyraShellButton Kind);
}
