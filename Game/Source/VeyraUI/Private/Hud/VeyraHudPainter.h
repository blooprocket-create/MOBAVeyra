// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"

class UCanvas;
class UFont;
class UTexture;
class UVeyraGreyboxSettings;
class FVeyraHudMotion;
struct FVeyraEaseCurve;

/**
 * Draws the match HUD on the canvas at its scale, in the client's surfaces and type (ADR-078 §3, ADR-080 §1), and moves
 * it (ADR-082 §2): every colour's opacity is multiplied by Alpha and every position moved by Offset, so any part can fade
 * and slide in without its layout knowing.
 */
struct FVeyraHudPainter
{
	UCanvas& Canvas;
	const UVeyraGreyboxSettings& Settings;
	const UFont* FontAsset = nullptr;
	float Scale = 1.0f;
	float Alpha = 1.0f;
	FVector2D Offset = FVector2D::ZeroVector;
	/** What the HUD remembers between frames; none where nothing moves, as a test's canvas. */
	FVeyraHudMotion* Motion = nullptr;

	/** The Smoked Crucible's surface (ADR-078 §3): its chamfered fill, tinted the surface's colour, and its brass edge. */
	FSlateBrush SurfaceFill;
	FSlateBrush SurfaceEdge;

	FVeyraHudPainter(UCanvas& InCanvas, const UVeyraGreyboxSettings& InSettings, const UFont* InFont, float HudScale = 1.0f, FVeyraHudMotion* InMotion = nullptr);

	/** Like Like at Scale pixels per designed unit: one of the deck's sections, or the team panels (ADR-059 §1). */
	FVeyraHudPainter At(float NewScale) const;

	/** Like this one, faded to Opacity of its own and moved by By more. */
	FVeyraHudPainter Moved(float Opacity, const FVector2D& By) const;

	float S(float Units) const { return Units * Scale; }

	FLinearColor Faded(const FLinearColor& Color) const { return Color.CopyWithNewOpacity(Color.A * Alpha); }

	/** Typeface at Size, scaled; from the HUD's font asset, without which the canvas draws no text. */
	FSlateFontInfo Font(const TCHAR* Typeface, int32 Size) const;

	// Shapes.

	void Rect(const FVector2D& TopLeft, const FVector2D& Size, const FLinearColor& Color) const;

	/** A rectangle shading from Top at its head to Bottom at its foot. */
	void Gradient(const FVector2D& TopLeft, const FVector2D& Size, const FLinearColor& Top, const FLinearColor& Bottom) const;

	void Outline(const FVector2D& TopLeft, const FVector2D& Size, const FLinearColor& Color, float Thickness = 1.0f) const;

	void Line(const FVector2D& From, const FVector2D& To, const FLinearColor& Color, float Thickness = 1.0f) const;

	/** A convex polygon, filled from its first point; each point its own colour when Colors has one for each. */
	void Polygon(TConstArrayView<FVector2D> Points, const FLinearColor& Color, TConstArrayView<FLinearColor> Colors = {}) const;

	/** A ring between Inner and Outer from the top, clockwise from From to To of the way round (0 to 1). */
	void Arc(const FVector2D& Centre, float Inner, float Outer, double From, double To, const FLinearColor& Color) const;

	/** The corners of a box with its top-left and bottom-right corners cut Cut across, clockwise from the top. */
	static TArray<FVector2D> Chamfered(const FVector2D& TopLeft, const FVector2D& Size, float Cut);

	/** Point moved toward Centre onto the chamfered box's edge where it lies in a cut corner. */
	static FVector2D IntoChamfer(const FVector2D& Point, const FVector2D& Centre, const FVector2D& TopLeft, const FVector2D& Size, float Cut);

	// Images and light.

	/** Texture over Size at TopLeft, its UV0 to UV1, in Tint; added as light when bAdd. */
	void Image(const UTexture* Texture, const FVector2D& TopLeft, const FVector2D& Size, const FLinearColor& Tint, const FVector2D& UV0 = FVector2D(0.0, 0.0),
		const FVector2D& UV1 = FVector2D(1.0, 1.0), bool bAdd = false) const;

	/** Texture's UV0 to UV1 over a convex polygon inside Bounds, each point's UV where it falls in them: a chamfered icon, a round face. */
	void ImageIn(const UTexture* Texture, TConstArrayView<FVector2D> Points, const FBox2D& Bounds, const FLinearColor& Tint, const FVector2D& UV0 = FVector2D(0.0, 0.0),
		const FVector2D& UV1 = FVector2D(1.0, 1.0)) const;

	/** The points of a circle of Radius around Centre: enough that its edge reads round at its size. */
	static TArray<FVector2D> Circle(const FVector2D& Centre, float Radius);

	/** A soft round light of Radius at Centre, added. */
	void Glow(const FVector2D& Centre, float Radius, const FLinearColor& Color) const;

	/** A ring of Radius at Centre, Thickness of its radius wide. */
	void Ring(const FVector2D& Centre, float Radius, float Thickness, const FLinearColor& Color, bool bAdd = false) const;

	/**
	 * A band of light Width wide crossing the box at TopLeft from left to right as Progress runs 0 to 1, clipped to the box:
	 * a glint, a sheen.
	 */
	void Sweep(const FVector2D& TopLeft, const FVector2D& Size, double Progress, float Width, const FLinearColor& Color) const;

	/**
	 * A burst at Centre After seconds after it came (ADR-082 §3): a ring growing to Radius and thinning, a white-hot core
	 * that shrinks, and four slashes flung out, turned by Seed. Nothing once it is over.
	 */
	void Burst(const FVector2D& Centre, double After, float Radius, const FLinearColor& Color, uint32 Seed = 0) const;

	// Surfaces.

	/** Brush drawn nine-sliced over Size at TopLeft in Tint: its corners keep their designed size at the HUD's scale. */
	void Box(const FSlateBrush& Brush, const FVector2D& TopLeft, const FVector2D& Size, const FLinearColor& Tint) const;

	/** A smoked, chamfered surface with its brass edge (ADR-078 §3). */
	void Surface(const FVector2D& TopLeft, const FVector2D& Size) const;

	// Text.

	FVector2D Measure(const FString& Text, const FSlateFontInfo& Font) const;

	void Text(const FVector2D& TopLeft, const FString& Text, const FSlateFontInfo& Font, const FLinearColor& Color, bool bShadow = false) const;

	void TextCentred(const FVector2D& Centre, const FString& Text, const FSlateFontInfo& Font, const FLinearColor& Color, bool bShadow = false) const;

	/** A key's name in a small cap at a tile's top-left corner. */
	void KeyCap(const FVector2D& TileTopLeft, const FString& Key) const;

	/** Up to MaxLines lines of Text, each no wider than Width, in Font. */
	TArray<FString> Wrap(const FString& Text, const FSlateFontInfo& Font, float Width, int32 MaxLines = MAX_int32) const;

	// Motion.

	/** Curve's value After seconds into Seconds (0 to 1, a settle may pass 1). */
	static double Eased(const FVeyraEaseCurve& Curve, double After, double Seconds);

	/**
	 * Like this one as a panel After seconds since it opened: rising Rise units into place and fading in over the panel's
	 * seconds along the snap (ADR-082 §2).
	 */
	FVeyraHudPainter CuttingIn(double After) const;
};
