// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Math/Color.h"
#include "Styling/SlateBrush.h"

class UTexture2D;

/** A face's fill and edge (ADR-078 §3): what VeyraShellBrushes::Face bakes into its texture. */
struct FVeyraFaceLook
{
	/** The fill at its head and at its foot. */
	FLinearColor Top = FLinearColor::Transparent;
	FLinearColor Bottom = FLinearColor::Transparent;
	/** The edge along its straight sides, and over its cut corners; transparent for none. */
	FLinearColor Edge = FLinearColor::Transparent;
	FLinearColor Tick = FLinearColor::Transparent;
	/** The edge's width, in slate units. */
	float EdgeWidth = 1.0f;
	/** A thin light along the top, inside the edge; transparent for none. */
	FLinearColor Shine = FLinearColor::Transparent;
	/** The cut across the top-left and bottom-right corners, in slate units. */
	float Chamfer = 0.0f;

	bool operator==(const FVeyraFaceLook& Other) const = default;
};

/** The texels a face is made from, row by row from its top, and the room its corners keep. */
struct FVeyraFaceTexels
{
	int32 Width = 0;
	int32 Height = 0;
	TArray<FColor> Texels;
	/** How many slate units each corner keeps when the face stretches. */
	float Rim = 0.0f;
	/** Texels per slate unit. */
	int32 TexelsPerUnit = 1;

	const FColor& At(int32 Column, int32 Row) const { return Texels[Row * Width + Column]; }
};

/**
 * The Smoked Crucible's surfaces (ADR-078 §3): chamfered shapes drawn as nine-sliced boxes, so one small texture
 * fits any size. Each texture is made from its parameters the first time it is asked for, then shared.
 */
namespace VeyraShellBrushes
{
	/**
	 * A chamfered shape, white at its head and Shade darker at its foot, for a brush's tint to colour: a surface's fill,
	 * whose tint stays its colour (and goes opaque under Reduce Interface Transparency, SET-75).
	 */
	VEYRAUI_API FSlateBrush Mask(float Chamfer, float Shade);

	/** The chamfered outline alone: Edge along the sides, Tick over the cut corners, transparent inside. */
	VEYRAUI_API FSlateBrush Outline(float Chamfer, const FLinearColor& Edge, const FLinearColor& Tick, float EdgeWidth = 1.0f);

	/** A whole face with Look baked in, under a white tint: a button state, whose brush is all it draws. */
	VEYRAUI_API FSlateBrush Face(const FVeyraFaceLook& Look);

	/** Look of the face Brush draws, if Face made it. */
	VEYRAUI_API const FVeyraFaceLook* LookOf(const FSlateBrush& Brush);

	/** A line Height high along the foot, in Color, clear above it: the shown tab's underline. */
	VEYRAUI_API FSlateBrush Underline(float Height, const FLinearColor& Color);

	/**
	 * A diamond Size slate units tall, filled in Fill and edged in Edge (either clear for none): the Flux mark beside the
	 * wordmark and a currency's amount.
	 */
	VEYRAUI_API FSlateBrush Diamond(float Size, const FLinearColor& Fill, const FLinearColor& Edge);

	/**
	 * Light for the HUD's and the shell's motion (ADR-082 §3), white for a tint to colour: a soft round glow falling to
	 * nothing at its rim; a ring Thickness of its radius wide, its edges smooth; and a streak across, clear at both ends
	 * and brightest in its middle, for a glint or a sheen. Each is premultiplied (its colour as strong as its alpha), so it
	 * adds light as well as it blends.
	 */
	VEYRAUI_API UTexture2D* GlowTexture();
	VEYRAUI_API UTexture2D* RingTexture(float Thickness);
	VEYRAUI_API UTexture2D* StreakTexture();

	/** The glow as an image brush Size slate units across, for the shell. */
	VEYRAUI_API FSlateBrush Glow(float Size);

	/** A coin Size slate units across, filled in Fill with a ring in Ring inside its rim: Gold's mark beside its amount (ADR-082 §8). */
	VEYRAUI_API FSlateBrush Coin(float Size, const FLinearColor& Fill, const FLinearColor& Ring);

	/** Nothing at all: a state that draws no surface. */
	VEYRAUI_API FSlateBrush None();

	/** Look's face as texels, as the texture Face makes is filled. */
	VEYRAUI_API FVeyraFaceTexels Rasterise(const FVeyraFaceLook& Look);

	/** How many textures have been made. For tests. */
	VEYRAUI_API int32 TextureCount();
}
