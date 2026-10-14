// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The stats panel's icons (ADR-082 §4): each stat's glyph as strokes in a box Size units square, drawn as lines in its
 * colour. Art, written as the design board drew it.
 */
namespace VeyraStatGlyphs
{
	/** The box every glyph is drawn in, in units. */
	inline constexpr float Size = 16.0f;

	/** Stat's strokes, each a run of points, by its key in the text table's stat rows; none for a stat without one. */
	VEYRAUI_API TArray<TArray<FVector2D>> Of(const FString& Stat);
}
