// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraShellMotes.h"

#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Shell/VeyraShellBrushes.h"
#include "Shell/VeyraShellLook.h"
#include "Shell/VeyraShellStyleSettings.h"

namespace
{
	const UVeyraShellStyleSettings& Style()
	{
		return *GetDefault<UVeyraShellStyleSettings>();
	}

	// How a mote is drawn, not tuning: its glow reaches this many times its radius, and no frame's step is longer than this,
	// so a hitch never flings the motes.
	constexpr float GlowReach = 5.0f;
	constexpr float LongestStep = 0.1f;
	// The share of the height a mote fades in over from the foot, and out over toward the head.
	constexpr float FadeInShare = 0.2f;
	constexpr float FadeOutShare = 0.25f;
}

void SVeyraShellMotes::Construct(const FArguments& InArgs)
{
	// One seed for the menus: the same drift each time, which is all it needs to be.
	constexpr int32 Seed = 7177;
	Random.Initialize(Seed);
	SetVisibility(EVisibility::HitTestInvisible);
	if (!VeyraShellLook::Current().bStillAnimation)
	{
		RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SVeyraShellMotes::Step));
	}
}

SVeyraShellMotes::FMote SVeyraShellMotes::Made(const FVector2D& Area, bool bAnywhere)
{
	const UVeyraShellStyleSettings& Settings = Style();
	FMote Mote;
	// Mostly small, now and then large, as the square of a fair draw falls.
	const float Draw = Random.FRand();
	Mote.Radius = Settings.MoteRadiusMin + Draw * Draw * (Settings.MoteRadiusMax - Settings.MoteRadiusMin);
	Mote.At = FVector2D(Random.FRand() * Area.X, bAnywhere ? Random.FRand() * Area.Y : Area.Y + Mote.Radius * GlowReach);
	Mote.Rise = FMath::Lerp(Settings.MoteRiseMin, Settings.MoteRiseMax, Random.FRand());
	Mote.Phase = Random.FRand() * UE_TWO_PI;
	// Sway between a fifth of a turn and four fifths a second, as the website's motes do.
	Mote.SwaySpeed = 0.2f + Random.FRand() * 0.6f;
	Mote.Flicker = Random.FRand() * UE_TWO_PI;
	Mote.bBrass = Random.FRand() < Settings.MoteBrassShare;
	Mote.bHot = Random.FRand() < Settings.MoteHotShare;
	return Mote;
}

void SVeyraShellMotes::Advance(const FVector2D& Area, float Seconds)
{
	const UVeyraShellStyleSettings& Settings = Style();
	if (Area.X <= 0.0 || Area.Y <= 0.0)
	{
		return;
	}
	const int32 Count = FMath::Min(Settings.MoteMost, FMath::RoundToInt32(Area.X * Area.Y / Settings.MoteArea));
	const bool bResized = !Area.Equals(LastAreaMoved, 1.0);
	if (bResized || Motes.Num() != Count)
	{
		Motes.Reset();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Motes.Add(Made(Area, true));
		}
		LastAreaMoved = Area;
	}
	const float Step = FMath::Min(Seconds, LongestStep);
	for (FMote& Mote : Motes)
	{
		Mote.At.Y -= Mote.Rise * Step;
		Mote.Phase += Mote.SwaySpeed * Step;
		// The flicker turns three times faster than the sway.
		Mote.Flicker += Step * 3.0f;
		if (Mote.At.Y < -Mote.Radius * GlowReach)
		{
			Mote = Made(Area, false);
		}
	}
}

EActiveTimerReturnType SVeyraShellMotes::Step(double CurrentTime, float DeltaTime)
{
	if (VeyraShellLook::Current().bStillAnimation)
	{
		Motes.Reset();
		return EActiveTimerReturnType::Stop;
	}
	Advance(LastArea, DeltaTime);
	Invalidate(EInvalidateWidgetReason::Paint);
	return EActiveTimerReturnType::Continue;
}

int32 SVeyraShellMotes::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D Area = AllottedGeometry.GetLocalSize();
	LastArea = Area;
	if (Motes.IsEmpty() || Area.Y <= 0.0)
	{
		return LayerId;
	}
	const UVeyraShellStyleSettings& Settings = Style();
	static const FSlateBrush Glow = VeyraShellBrushes::Glow(1.0f);
	const FLinearColor Teal = Settings.AccentColor;
	const FLinearColor Brass = Settings.PrimaryColor;
	const FLinearColor Hot = FLinearColor::White;
	for (const FMote& Mote : Motes)
	{
		const float X = static_cast<float>(Mote.At.X) + FMath::Sin(Mote.Phase) * Settings.MoteSway;
		const float Y = static_cast<float>(Mote.At.Y);
		// Fading in from the foot and out toward the head, and flickering.
		const float Life = FMath::Clamp((static_cast<float>(Area.Y) - Y) / (static_cast<float>(Area.Y) * FadeInShare), 0.0f, 1.0f)
			* FMath::Clamp(Y / (static_cast<float>(Area.Y) * FadeOutShare), 0.0f, 1.0f);
		const float Alpha = Life * (0.55f + 0.45f * FMath::Sin(Mote.Flicker));
		const FLinearColor Color = Mote.bBrass ? Brass : Teal;
		const auto Dot = [&](float Radius, const FLinearColor& Tint, float Strength) {
			const float A = FMath::Clamp(Strength, 0.0f, 1.0f);
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(FVector2f(Radius * 2.0f), FSlateLayoutTransform(FVector2f(X - Radius, Y - Radius))),
				&Glow, ESlateDrawEffect::PreMultipliedAlpha, FLinearColor(Tint.R * A, Tint.G * A, Tint.B * A, A));
		};
		Dot(Mote.Radius * GlowReach, Color, 0.5f * Alpha);
		Dot(Mote.Radius * 1.6f, Mote.bHot ? Hot : Color, Alpha);
	}
	return LayerId;
}

TSharedRef<SWidget> UVeyraShellMotes::RebuildWidget()
{
	Motes = SNew(SVeyraShellMotes);
	return Motes.ToSharedRef();
}

void UVeyraShellMotes::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Motes.Reset();
}
