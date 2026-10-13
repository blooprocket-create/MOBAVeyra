// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Fonts/SlateFontInfo.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "Templates/SharedPointer.h"

struct FCompositeFont;

/**
 * The client's type (ADR-078 §2): each family of UVeyraShellStyleSettings a composite font read from the files the
 * game ships, built once. Each keeps the engine default font's other scripts and its fallback, so a name in another
 * alphabet still draws.
 */
namespace VeyraShellFonts
{
	/** Family's composite font; the engine's default font while the family has no face. */
	VEYRAUI_API TSharedRef<const FCompositeFont> FamilyFont(EVeyraFontFamily Family);

	/** Face's font at Size, in slate units. */
	VEYRAUI_API FSlateFontInfo FontOf(const FVeyraTextFace& Face, float Size);

	/** Drops the built families, so the next read builds them from the settings again. For tests. */
	VEYRAUI_API void Reset();
}
