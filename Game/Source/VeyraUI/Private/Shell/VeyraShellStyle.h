// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Internationalization/Text.h"
#include "Math/Color.h"

class UBorder;
class UPanelWidget;
class UTextBlock;
class UWidget;
class UWidgetTree;
struct FButtonStyle;

/** The shell's widgets in its style (UVeyraShellStyleSettings), built in C++. */
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
	};

	/** A text block for Text in Role's size and colour. */
	UTextBlock* MakeText(UWidgetTree& Tree, const FText& Text, EVeyraShellText Role);

	/** A border filled with Color, padded by the shell's spacing. */
	UBorder* MakeBorder(UWidgetTree& Tree, const FLinearColor& Color, float Padding);

	/** Adds Child to Parent, a vertical or horizontal box, followed by the shell's spacing. */
	void AddSpaced(UPanelWidget& Parent, UWidget& Child);

	/** A button style whose background is Base, lighter when hovered and pressed, with Padding around its content. */
	FButtonStyle ButtonStyle(const FLinearColor& Base, float Padding);
}
