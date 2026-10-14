// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Hud/VeyraHudPainter.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Texture.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Hud/VeyraHudMotion.h"
#include "Rendering/SlateRenderer.h"
#include "Shell/VeyraShellBrushes.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "Styling/CoreStyle.h"

namespace
{
	/** A triangle of Points' A, B and C, each its own colour and UV. */
	FCanvasUVTri Tri(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& ColorA, const FLinearColor& ColorB, const FLinearColor& ColorC,
		const FVector2D& UVA = FVector2D::ZeroVector, const FVector2D& UVB = FVector2D::ZeroVector, const FVector2D& UVC = FVector2D::ZeroVector)
	{
		FCanvasUVTri Made;
		Made.V0_Pos = A;
		Made.V1_Pos = B;
		Made.V2_Pos = C;
		Made.V0_Color = ColorA;
		Made.V1_Color = ColorB;
		Made.V2_Color = ColorC;
		Made.V0_UV = UVA;
		Made.V1_UV = UVB;
		Made.V2_UV = UVC;
		return Made;
	}
}

FVeyraHudPainter::FVeyraHudPainter(UCanvas& InCanvas, const UVeyraGreyboxSettings& InSettings, const UFont* InFont, float HudScale, FVeyraHudMotion* InMotion)
	: Canvas(InCanvas)
	, Settings(InSettings)
	, FontAsset(InFont)
	, Scale((InSettings.HudReferenceHeight > 0.0f ? InCanvas.ClipY / InSettings.HudReferenceHeight : 1.0f) * HudScale)
	, Motion(InMotion)
{
	const UVeyraShellStyleSettings& Look = *GetDefault<UVeyraShellStyleSettings>();
	SurfaceFill = VeyraShellBrushes::Mask(Look.PanelChamfer, Look.SurfaceShade);
	SurfaceEdge = VeyraShellBrushes::Outline(Look.PanelChamfer, Look.EdgeColor, Look.EdgeTickColor);
}

FVeyraHudPainter FVeyraHudPainter::At(float NewScale) const
{
	FVeyraHudPainter Painter = *this;
	Painter.Scale = NewScale;
	return Painter;
}

FVeyraHudPainter FVeyraHudPainter::Moved(float Opacity, const FVector2D& By) const
{
	FVeyraHudPainter Painter = *this;
	Painter.Alpha *= FMath::Clamp(Opacity, 0.0f, 1.0f);
	Painter.Offset += By;
	return Painter;
}

FSlateFontInfo FVeyraHudPainter::Font(const TCHAR* Typeface, int32 Size) const
{
	const int32 Scaled = FMath::Max(1, FMath::RoundToInt32(Size * Scale));
	return FontAsset ? FSlateFontInfo(FontAsset, Scaled, FName(Typeface)) : FCoreStyle::GetDefaultFontStyle(Typeface, Scaled);
}

void FVeyraHudPainter::Rect(const FVector2D& TopLeft, const FVector2D& Size, const FLinearColor& Color) const
{
	if (Size.X <= 0.0 || Size.Y <= 0.0)
	{
		return;
	}
	FCanvasTileItem Tile(TopLeft + Offset, GWhiteTexture, Size, Faded(Color));
	Tile.BlendMode = SE_BLEND_Translucent;
	Canvas.DrawItem(Tile);
}

void FVeyraHudPainter::Gradient(const FVector2D& TopLeft, const FVector2D& Size, const FLinearColor& Top, const FLinearColor& Bottom) const
{
	if (Size.X <= 0.0 || Size.Y <= 0.0)
	{
		return;
	}
	const FVector2D A = TopLeft + Offset;
	const FVector2D B = A + FVector2D(Size.X, 0.0);
	const FVector2D C = A + Size;
	const FVector2D D = A + FVector2D(0.0, Size.Y);
	const FLinearColor Head = Faded(Top);
	const FLinearColor Foot = Faded(Bottom);
	TArray<FCanvasUVTri> Triangles{ Tri(A, B, C, Head, Head, Foot), Tri(A, C, D, Head, Foot, Foot) };
	FCanvasTriangleItem Item(Triangles, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas.DrawItem(Item);
}

void FVeyraHudPainter::Outline(const FVector2D& TopLeft, const FVector2D& Size, const FLinearColor& Color, float Thickness) const
{
	FCanvasBoxItem Item(TopLeft + Offset, Size);
	Item.SetColor(Faded(Color));
	Item.LineThickness = Thickness;
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas.DrawItem(Item);
}

void FVeyraHudPainter::Line(const FVector2D& From, const FVector2D& To, const FLinearColor& Color, float Thickness) const
{
	FCanvasLineItem Item(From + Offset, To + Offset);
	Item.SetColor(Faded(Color));
	Item.LineThickness = Thickness;
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas.DrawItem(Item);
}

void FVeyraHudPainter::Polygon(TConstArrayView<FVector2D> Points, const FLinearColor& Color, TConstArrayView<FLinearColor> Colors) const
{
	if (Points.Num() < 3)
	{
		return;
	}
	const bool bEach = Colors.Num() == Points.Num();
	const auto ColorAt = [&](int32 Index) { return Faded(bEach ? Colors[Index] : Color); };
	TArray<FCanvasUVTri> Triangles;
	Triangles.Reserve(Points.Num() - 2);
	for (int32 Index = 2; Index < Points.Num(); ++Index)
	{
		Triangles.Add(Tri(Points[0] + Offset, Points[Index - 1] + Offset, Points[Index] + Offset, ColorAt(0), ColorAt(Index - 1), ColorAt(Index)));
	}
	FCanvasTriangleItem Item(Triangles, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas.DrawItem(Item);
}

void FVeyraHudPainter::Arc(const FVector2D& Centre, float Inner, float Outer, double From, double To, const FLinearColor& Color) const
{
	const double Start = FMath::Clamp(From, 0.0, 1.0);
	const double End = FMath::Clamp(To, 0.0, 1.0);
	if (End <= Start || Outer <= Inner)
	{
		return;
	}
	// Steps fine enough that the ring's edge reads round: one for every few pixels of its outer edge.
	constexpr float PixelsPerStep = 4.0f;
	const int32 Steps = FMath::Max(2, FMath::CeilToInt32(UE_TWO_PI * Outer * (End - Start) / PixelsPerStep));
	const FLinearColor Shown = Faded(Color);
	const auto PointAt = [&](double Share, float Radius) {
		const double Angle = -UE_DOUBLE_HALF_PI + UE_DOUBLE_TWO_PI * Share;
		return Centre + Offset + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
	};
	TArray<FCanvasUVTri> Triangles;
	Triangles.Reserve(Steps * 2);
	for (int32 Step = 0; Step < Steps; ++Step)
	{
		const double A = FMath::Lerp(Start, End, static_cast<double>(Step) / Steps);
		const double B = FMath::Lerp(Start, End, static_cast<double>(Step + 1) / Steps);
		const FVector2D InA = PointAt(A, Inner);
		const FVector2D OutA = PointAt(A, Outer);
		const FVector2D InB = PointAt(B, Inner);
		const FVector2D OutB = PointAt(B, Outer);
		Triangles.Add(Tri(InA, OutA, OutB, Shown, Shown, Shown));
		Triangles.Add(Tri(InA, OutB, InB, Shown, Shown, Shown));
	}
	FCanvasTriangleItem Item(Triangles, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas.DrawItem(Item);
}

TArray<FVector2D> FVeyraHudPainter::Chamfered(const FVector2D& TopLeft, const FVector2D& Size, float Cut)
{
	const double C = FMath::Clamp(static_cast<double>(Cut), 0.0, FMath::Min(Size.X, Size.Y) / 2.0);
	const FVector2D BottomRight = TopLeft + Size;
	return { FVector2D(TopLeft.X + C, TopLeft.Y), FVector2D(BottomRight.X, TopLeft.Y), FVector2D(BottomRight.X, BottomRight.Y - C), FVector2D(BottomRight.X - C, BottomRight.Y),
		FVector2D(TopLeft.X, BottomRight.Y), FVector2D(TopLeft.X, TopLeft.Y + C) };
}

FVector2D FVeyraHudPainter::IntoChamfer(const FVector2D& Point, const FVector2D& Centre, const FVector2D& TopLeft, const FVector2D& Size, float Cut)
{
	const double C = FMath::Clamp(static_cast<double>(Cut), 0.0, FMath::Min(Size.X, Size.Y) / 2.0);
	const FVector2D Local = Point - TopLeft;
	const FVector2D From = Centre - TopLeft;
	const FVector2D Along = Local - From;
	const double Sum = Local.X + Local.Y;
	// Where the ray from the centre crosses the cut's line x + y = Edge.
	const auto Onto = [&](double Edge) {
		const double Rate = Along.X + Along.Y;
		if (FMath::IsNearlyZero(Rate))
		{
			return Point;
		}
		const double T = FMath::Clamp((Edge - From.X - From.Y) / Rate, 0.0, 1.0);
		return TopLeft + From + Along * T;
	};
	if (Sum < C)
	{
		return Onto(C);
	}
	if (Sum > Size.X + Size.Y - C)
	{
		return Onto(Size.X + Size.Y - C);
	}
	return Point;
}

void FVeyraHudPainter::Image(const UTexture* Texture, const FVector2D& TopLeft, const FVector2D& Size, const FLinearColor& Tint, const FVector2D& UV0, const FVector2D& UV1,
	bool bAdd) const
{
	if (!Texture || !Texture->GetResource() || Size.X <= 0.0 || Size.Y <= 0.0)
	{
		return;
	}
	// Light is added by its colour alone, so its strength goes into its colour.
	const FLinearColor Shown = Faded(Tint);
	const FLinearColor Added(Shown.R * Shown.A, Shown.G * Shown.A, Shown.B * Shown.A, 1.0f);
	FCanvasTileItem Tile(TopLeft + Offset, Texture->GetResource(), Size, UV0, UV1, bAdd ? Added : Shown);
	Tile.BlendMode = bAdd ? SE_BLEND_Additive : SE_BLEND_Translucent;
	Canvas.DrawItem(Tile);
}

void FVeyraHudPainter::ImageIn(const UTexture* Texture, TConstArrayView<FVector2D> Points, const FBox2D& Bounds, const FLinearColor& Tint, const FVector2D& UV0,
	const FVector2D& UV1) const
{
	const FVector2D Span = Bounds.GetSize();
	if (!Texture || !Texture->GetResource() || Points.Num() < 3 || Span.X <= 0.0 || Span.Y <= 0.0)
	{
		return;
	}
	const auto UVOf = [&](const FVector2D& Point) { return UV0 + (Point - Bounds.Min) / Span * (UV1 - UV0); };
	const FLinearColor Shown = Faded(Tint);
	TArray<FCanvasUVTri> Triangles;
	Triangles.Reserve(Points.Num() - 2);
	for (int32 Index = 2; Index < Points.Num(); ++Index)
	{
		Triangles.Add(Tri(Points[0] + Offset, Points[Index - 1] + Offset, Points[Index] + Offset, Shown, Shown, Shown, UVOf(Points[0]), UVOf(Points[Index - 1]),
			UVOf(Points[Index])));
	}
	FCanvasTriangleItem Item(Triangles, Texture->GetResource());
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas.DrawItem(Item);
}

TArray<FVector2D> FVeyraHudPainter::Circle(const FVector2D& Centre, float Radius)
{
	// Enough points that the edge reads round: one for every few pixels of it, within bounds that keep a small one whole and
	// a large one cheap.
	constexpr float PixelsPerPoint = 4.0f;
	constexpr int32 LeastPoints = 16;
	constexpr int32 MostPoints = 72;
	const int32 Count = FMath::Clamp(FMath::CeilToInt32(UE_TWO_PI * Radius / PixelsPerPoint), LeastPoints, MostPoints);
	TArray<FVector2D> Points;
	Points.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const double Angle = UE_DOUBLE_TWO_PI * Index / Count;
		Points.Add(Centre + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
	}
	return Points;
}

void FVeyraHudPainter::Glow(const FVector2D& Centre, float Radius, const FLinearColor& Color) const
{
	Image(VeyraShellBrushes::GlowTexture(), Centre - FVector2D(Radius), FVector2D(Radius * 2.0f), Color, FVector2D(0.0, 0.0), FVector2D(1.0, 1.0), true);
}

void FVeyraHudPainter::Ring(const FVector2D& Centre, float Radius, float Thickness, const FLinearColor& Color, bool bAdd) const
{
	Image(VeyraShellBrushes::RingTexture(Thickness), Centre - FVector2D(Radius), FVector2D(Radius * 2.0f), Color, FVector2D(0.0, 0.0), FVector2D(1.0, 1.0), bAdd);
}

void FVeyraHudPainter::Sweep(const FVector2D& TopLeft, const FVector2D& Size, double Progress, float Width, const FLinearColor& Color) const
{
	if (Width <= 0.0f || Progress <= 0.0 || Progress >= 1.0)
	{
		return;
	}
	const double BandLeft = -Width + (Size.X + Width) * Progress;
	const double Left = FMath::Max(BandLeft, 0.0);
	const double Right = FMath::Min(BandLeft + Width, Size.X);
	if (Right <= Left)
	{
		return;
	}
	Image(VeyraShellBrushes::StreakTexture(), TopLeft + FVector2D(Left, 0.0), FVector2D(Right - Left, Size.Y), Color, FVector2D((Left - BandLeft) / Width, 0.0),
		FVector2D((Right - BandLeft) / Width, 1.0), true);
}

void FVeyraHudPainter::Burst(const FVector2D& Centre, double After, float Radius, const FLinearColor& Color, uint32 Seed) const
{
	const double Seconds = Settings.MotionBurstSeconds;
	if (After < 0.0 || After >= Seconds)
	{
		return;
	}
	// The shapes of the website's press burst (ADR-082 §3), as shares of its radius and its time.
	const double Out = Eased(Settings.MotionSnap, After, Seconds);
	const double Linear = After / Seconds;
	const FLinearColor Hot = Settings.MotionHotColor;
	// The ring grows from a seventh of its reach to past it, fading as it goes.
	const float RingRadius = Radius * static_cast<float>(0.15 + 1.1 * Out);
	Ring(Centre, RingRadius * 1.08f, 0.22f, Color.CopyWithNewOpacity(Color.A * 0.5f * static_cast<float>(1.0 - Linear)), true);
	Ring(Centre, RingRadius, 0.1f, Hot.CopyWithNewOpacity(static_cast<float>(1.0 - Linear)), true);
	// The core: white-hot, shrinking over the first two thirds of the burst.
	const double CoreShare = 0.65;
	if (Linear < CoreShare)
	{
		const double Core = Linear / CoreShare;
		Glow(Centre, Radius * static_cast<float>(0.5 * (1.2 - Core)), Hot.CopyWithNewOpacity(static_cast<float>(1.0 - Core)));
		Glow(Centre, Radius * static_cast<float>(0.9 * (1.2 - Core)), Color.CopyWithNewOpacity(Color.A * 0.6f * static_cast<float>(1.0 - Core)));
	}
	// Four slashes flung out a quarter turn apart, the set turned by Seed and each a little off true; whole for most of the
	// burst, then fading.
	const double FadeFrom = 0.6;
	const float SlashOpacity = static_cast<float>(Linear < FadeFrom ? 1.0 : 1.0 - (Linear - FadeFrom) / (1.0 - FadeFrom));
	constexpr double QuarterTurn = 90.0;
	constexpr int32 JitterDegrees = 24;
	const double Base = static_cast<double>(Seed % 90u);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const double Jitter = static_cast<double>((Seed >> (Index * 5)) % JitterDegrees) - JitterDegrees / 2.0;
		const double Angle = FMath::DegreesToRadians(Base + Index * QuarterTurn + Jitter);
		const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));
		const float Start = Radius * static_cast<float>(0.13 + 0.6 * Out);
		const float Length = Radius * static_cast<float>(0.2 + 0.55 * Out);
		const float Thickness = FMath::Max(1.0f, S(2.6f) * static_cast<float>(1.0 - 0.4 * Out));
		Line(Centre + Direction * Start, Centre + Direction * (Start + Length), Color.CopyWithNewOpacity(Color.A * SlashOpacity * 0.6f), Thickness * 2.2f);
		Line(Centre + Direction * Start, Centre + Direction * (Start + Length * 0.7f), Hot.CopyWithNewOpacity(SlashOpacity), Thickness);
	}
}

void FVeyraHudPainter::Box(const FSlateBrush& Brush, const FVector2D& TopLeft, const FVector2D& Size, const FLinearColor& Tint) const
{
	const UTexture* Texture = Cast<UTexture>(Brush.GetResourceObject());
	if (!Texture || !Texture->GetResource() || Size.X <= 0.0 || Size.Y <= 0.0)
	{
		return;
	}
	const FMargin& Margin = Brush.Margin;
	const double Left = FMath::Min(static_cast<double>(S(Margin.Left * Brush.ImageSize.X)), Size.X / 2.0);
	const double Right = FMath::Min(static_cast<double>(S(Margin.Right * Brush.ImageSize.X)), Size.X / 2.0);
	const double Top = FMath::Min(static_cast<double>(S(Margin.Top * Brush.ImageSize.Y)), Size.Y / 2.0);
	const double Bottom = FMath::Min(static_cast<double>(S(Margin.Bottom * Brush.ImageSize.Y)), Size.Y / 2.0);
	const double Xs[] = { 0.0, Left, Size.X - Right, Size.X };
	const double Us[] = { 0.0, Margin.Left, 1.0 - Margin.Right, 1.0 };
	const double Ys[] = { 0.0, Top, Size.Y - Bottom, Size.Y };
	const double Vs[] = { 0.0, Margin.Top, 1.0 - Margin.Bottom, 1.0 };
	const FLinearColor Shown = Faded(Tint);
	for (int32 Row = 0; Row < 3; ++Row)
	{
		for (int32 Column = 0; Column < 3; ++Column)
		{
			const FVector2D Piece(Xs[Column + 1] - Xs[Column], Ys[Row + 1] - Ys[Row]);
			if (Piece.X <= 0.0 || Piece.Y <= 0.0)
			{
				continue;
			}
			FCanvasTileItem Tile(TopLeft + Offset + FVector2D(Xs[Column], Ys[Row]), Texture->GetResource(), Piece, FVector2D(Us[Column], Vs[Row]),
				FVector2D(Us[Column + 1], Vs[Row + 1]), Shown);
			Tile.BlendMode = SE_BLEND_Translucent;
			Canvas.DrawItem(Tile);
		}
	}
}

void FVeyraHudPainter::Surface(const FVector2D& TopLeft, const FVector2D& Size) const
{
	Box(SurfaceFill, TopLeft, Size, Settings.HudSurfaceColor);
	Box(SurfaceEdge, TopLeft, Size, FLinearColor::White);
}

FVector2D FVeyraHudPainter::Measure(const FString& Text, const FSlateFontInfo& Font) const
{
	if (Text.IsEmpty() || !FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
	{
		return FVector2D::ZeroVector;
	}
	return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font);
}

void FVeyraHudPainter::Text(const FVector2D& TopLeft, const FString& Text, const FSlateFontInfo& Font, const FLinearColor& Color, bool bShadow) const
{
	if (Text.IsEmpty() || Alpha <= 0.0f)
	{
		return;
	}
	FCanvasTextItem Item(TopLeft + Offset, FText::FromString(Text), Font, Faded(Color));
	if (bShadow)
	{
		Item.EnableShadow(Faded(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f)));
	}
	Canvas.DrawItem(Item);
}

void FVeyraHudPainter::TextCentred(const FVector2D& Centre, const FString& Text, const FSlateFontInfo& Font, const FLinearColor& Color, bool bShadow) const
{
	this->Text(Centre - Measure(Text, Font) / 2.0, Text, Font, Color, bShadow);
}

void FVeyraHudPainter::KeyCap(const FVector2D& TileTopLeft, const FString& Key) const
{
	const FSlateFontInfo KeyFont = Font(TEXT("Bold"), Settings.HudSmallFontSize);
	const FVector2D Pad(S(4.0f), S(1.0f));
	Text(TileTopLeft + Pad, Key, KeyFont, Settings.TextColor, true);
}

TArray<FString> FVeyraHudPainter::Wrap(const FString& Text, const FSlateFontInfo& Font, float Width, int32 MaxLines) const
{
	TArray<FString> Words;
	Text.ParseIntoArrayWS(Words);
	TArray<FString> Lines;
	FString Line;
	for (const FString& Word : Words)
	{
		const FString Tried = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
		if (!Line.IsEmpty() && Measure(Tried, Font).X > Width)
		{
			Lines.Add(Line);
			Line = Word;
		}
		else
		{
			Line = Tried;
		}
	}
	if (!Line.IsEmpty())
	{
		Lines.Add(Line);
	}
	if (Lines.Num() > MaxLines)
	{
		Lines.SetNum(MaxLines);
		Lines.Last() += TEXT("...");
	}
	return Lines;
}

double FVeyraHudPainter::Eased(const FVeyraEaseCurve& Curve, double After, double Seconds)
{
	return VeyraHudEase::Evaluate(Curve, VeyraHudEase::Progress(After, Seconds));
}

FVeyraHudPainter FVeyraHudPainter::CuttingIn(double After) const
{
	const double Seconds = Settings.MotionPanelSeconds;
	const double Into = Eased(Settings.MotionSnap, After, Seconds);
	// Whole by two thirds of the way, as the website's panels are.
	const float Opacity = static_cast<float>(VeyraHudEase::Progress(After, Seconds * 0.66));
	return Moved(Opacity, FVector2D(0.0, S(Settings.MotionPanelRise) * (1.0 - Into)));
}
