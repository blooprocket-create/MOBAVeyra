// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraShellMotion.h"

#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Hud/VeyraHudMotion.h"
#include "Rendering/DrawElements.h"
#include "Shell/VeyraShellBrushes.h"
#include "Shell/VeyraShellLook.h"
#include "Shell/VeyraShellStyleSettings.h"

namespace
{
	const UVeyraShellStyleSettings& ShellLook()
	{
		return *GetDefault<UVeyraShellStyleSettings>();
	}

	const UVeyraGreyboxSettings& HudMotionLook()
	{
		return *GetDefault<UVeyraGreyboxSettings>();
	}

	double ShellEased(const FVeyraEaseCurve& Curve, double After, double Seconds)
	{
		return VeyraHudEase::Evaluate(Curve, VeyraHudEase::Progress(After, Seconds));
	}

	/** A brush of one of the light textures, drawn whole. */
	FSlateBrush LightBrush(UTexture2D* Texture)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = FVector2D(Texture ? Texture->GetSizeX() : 1, Texture ? Texture->GetSizeY() : 1);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		return Brush;
	}

	/** Light's colour at Strength, premultiplied as the light textures are. */
	FLinearColor Light(const FLinearColor& Color, float Strength)
	{
		const float A = FMath::Clamp(Color.A * Strength, 0.0f, 1.0f);
		return FLinearColor(Color.R * A, Color.G * A, Color.B * A, A);
	}
}

// The page reveal.

FVeyraShellReveal::~FVeyraShellReveal()
{
	if (Ticking.IsValid())
	{
		FTSTicker::RemoveTicker(Ticking);
	}
}

void FVeyraShellReveal::Pose(EVeyraReveal Kind, int32 Index, double After, float& OutOpacity, FWidgetTransform& OutTransform)
{
	const UVeyraShellStyleSettings& Settings = ShellLook();
	const double Own = After - Index * static_cast<double>(Settings.RevealStagger);
	// Whole by two fifths of the way, as the website's panels are.
	OutOpacity = static_cast<float>(VeyraHudEase::Progress(Own, Settings.RevealSeconds * 0.4));
	OutTransform = FWidgetTransform();
	if (Kind == EVeyraReveal::Slide)
	{
		const float Left = 1.0f - static_cast<float>(ShellEased(HudMotionLook().MotionSnap, Own, Settings.RevealSeconds));
		OutTransform.Translation = FVector2D(-Settings.RevealSlide * Left, 0.0f);
		OutTransform.Shear = FVector2D(-Settings.RevealShear * Left, 0.0f);
	}
	else
	{
		const float Left = 1.0f - static_cast<float>(ShellEased(HudMotionLook().MotionSettle, Own, Settings.RevealSeconds));
		OutTransform.Translation = FVector2D(0.0f, Settings.RevealRise * Left);
	}
}

double FVeyraShellReveal::SecondsUntilPlaced(int32 Index)
{
	return Index * static_cast<double>(ShellLook().RevealStagger) + ShellLook().RevealSeconds;
}

void FVeyraShellReveal::Take(TConstArrayView<UWidget*> InParts, TConstArrayView<EVeyraReveal> Kinds)
{
	Parts.Reset();
	for (int32 Index = 0; Index < InParts.Num(); ++Index)
	{
		if (InParts[Index])
		{
			Parts.Add(FPart{ InParts[Index], Kinds.IsValidIndex(Index) ? Kinds[Index] : EVeyraReveal::Rise });
		}
	}
}

void FVeyraShellReveal::Play(TConstArrayView<UWidget*> InParts, TConstArrayView<EVeyraReveal> Kinds)
{
	Finish();
	if (VeyraShellLook::Current().bStillAnimation)
	{
		return;
	}
	Take(InParts, Kinds);
	StartedAt = FPlatformTime::Seconds();
	Apply(StartedAt);
	Ticking = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FVeyraShellReveal::Tick));
}

void FVeyraShellReveal::Continue(TConstArrayView<UWidget*> InParts, TConstArrayView<EVeyraReveal> Kinds)
{
	if (!IsPlaying())
	{
		return;
	}
	Take(InParts, Kinds);
	Apply(FPlatformTime::Seconds());
}

void FVeyraShellReveal::Finish()
{
	for (const FPart& Part : Parts)
	{
		if (UWidget* Widget = Part.Widget.Get())
		{
			Widget->SetRenderOpacity(1.0f);
			Widget->SetRenderTransform(FWidgetTransform());
		}
	}
	Parts.Reset();
	if (Ticking.IsValid())
	{
		FTSTicker::RemoveTicker(Ticking);
		Ticking.Reset();
	}
}

void FVeyraShellReveal::Apply(double Now)
{
	for (int32 Index = 0; Index < Parts.Num(); ++Index)
	{
		if (UWidget* Widget = Parts[Index].Widget.Get())
		{
			float Opacity = 1.0f;
			FWidgetTransform Transform;
			Pose(Parts[Index].Kind, Index, Now - StartedAt, Opacity, Transform);
			Widget->SetRenderOpacity(Opacity);
			Widget->SetRenderTransform(Transform);
		}
	}
}

bool FVeyraShellReveal::Tick(float DeltaSeconds)
{
	const double Now = FPlatformTime::Seconds();
	if (Now - StartedAt >= SecondsUntilPlaced(FMath::Max(0, Parts.Num() - 1)))
	{
		// Its last part placed: everything stands as it was built.
		Ticking.Reset();
		const TArray<FPart> Placed = MoveTemp(Parts);
		for (const FPart& Part : Placed)
		{
			if (UWidget* Widget = Part.Widget.Get())
			{
				Widget->SetRenderOpacity(1.0f);
				Widget->SetRenderTransform(FWidgetTransform());
			}
		}
		return false;
	}
	Apply(Now);
	return true;
}

// The lit button.

double SVeyraShellButton::SinceSheen() const
{
	return FPlatformTime::Seconds() - SheenAt;
}

double SVeyraShellButton::SinceBurst() const
{
	return FPlatformTime::Seconds() - BurstAt;
}

void SVeyraShellButton::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SButton::OnMouseEnter(MyGeometry, MouseEvent);
	if (bSheen && IsEnabled() && !VeyraShellLook::Current().bStillAnimation)
	{
		SheenAt = FPlatformTime::Seconds();
		Animate();
	}
}

FReply SVeyraShellButton::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (IsEnabled() && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && !VeyraShellLook::Current().bStillAnimation)
	{
		BurstAt = FPlatformTime::Seconds();
		BurstPoint = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		++BurstSeed;
		Animate();
	}
	return SButton::OnMouseButtonDown(MyGeometry, MouseEvent);
}

void SVeyraShellButton::Animate()
{
	if (!Timer.IsValid())
	{
		Timer = RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SVeyraShellButton::Step));
	}
}

EActiveTimerReturnType SVeyraShellButton::Step(double CurrentTime, float DeltaTime)
{
	Invalidate(EInvalidateWidgetReason::Paint);
	if (SinceSheen() < ShellLook().SheenSeconds || SinceBurst() < HudMotionLook().MotionBurstSeconds)
	{
		return EActiveTimerReturnType::Continue;
	}
	Timer.Reset();
	return EActiveTimerReturnType::Stop;
}

int32 SVeyraShellButton::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 Layer = SButton::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const double Sheen = SinceSheen();
	const double Burst = SinceBurst();
	const UVeyraShellStyleSettings& Settings = ShellLook();
	const UVeyraGreyboxSettings& Moves = HudMotionLook();
	if (Sheen >= Settings.SheenSeconds && Burst >= Moves.MotionBurstSeconds)
	{
		return Layer;
	}
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	// The light stays on the button, as the website's does.
	OutDrawElements.PushClip(FSlateClippingZone(AllottedGeometry));
	++Layer;
	const ESlateDrawEffect Effect = ESlateDrawEffect::PreMultipliedAlpha;
	if (Sheen < Settings.SheenSeconds)
	{
		// A white-hot band crossing it from the left, slow then rushing.
		static const FSlateBrush Streak = LightBrush(VeyraShellBrushes::StreakTexture());
		const float Band = static_cast<float>(Size.X) * 0.55f;
		const float Across = static_cast<float>(ShellEased(Moves.MotionSmear, Sheen, Settings.SheenSeconds));
		const float Left = -Band + (static_cast<float>(Size.X) + Band) * Across;
		FSlateDrawElement::MakeBox(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(FVector2f(Band, Size.Y), FSlateLayoutTransform(FVector2f(Left, 0.0f))), &Streak, Effect,
			Light(Moves.MotionHotColor, Settings.SheenOpacity));
	}
	if (Burst < Moves.MotionBurstSeconds)
	{
		// The website's press burst: a ring growing and thinning, a white-hot core shrinking, and four slashes flung out.
		static const FSlateBrush Ring = LightBrush(VeyraShellBrushes::RingTexture(0.12f));
		static const FSlateBrush Core = LightBrush(VeyraShellBrushes::GlowTexture());
		const double Out = ShellEased(Moves.MotionSnap, Burst, Moves.MotionBurstSeconds);
		const float Linear = static_cast<float>(Burst / Moves.MotionBurstSeconds);
		const float Reach = Settings.PressBurstRadius;
		const FVector2f At(BurstPoint);
		const float RingRadius = Reach * static_cast<float>(0.15 + 1.1 * Out);
		FSlateDrawElement::MakeBox(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(FVector2f(RingRadius * 2.0f), FSlateLayoutTransform(At - FVector2f(RingRadius))), &Ring,
			Effect, Light(Moves.MotionHotColor, 1.0f - Linear));
		constexpr float CoreShare = 0.65f;
		if (Linear < CoreShare)
		{
			const float Shrunk = Reach * 0.4f * (1.2f - Linear / CoreShare);
			FSlateDrawElement::MakeBox(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(FVector2f(Shrunk * 2.0f), FSlateLayoutTransform(At - FVector2f(Shrunk))), &Core, Effect,
				Light(Moves.MotionHotColor, 1.0f - Linear / CoreShare));
		}
		constexpr float FadeFrom = 0.6f;
		const float SlashOpacity = Linear < FadeFrom ? 1.0f : 1.0f - (Linear - FadeFrom) / (1.0f - FadeFrom);
		const double Base = static_cast<double>((BurstSeed * 37u) % 90u);
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const double Angle = FMath::DegreesToRadians(Base + Index * 90.0);
			const FVector2f Direction(static_cast<float>(FMath::Cos(Angle)), static_cast<float>(FMath::Sin(Angle)));
			const float Start = Reach * static_cast<float>(0.13 + 0.6 * Out);
			const float Length = Reach * static_cast<float>(0.2 + 0.55 * Out);
			const TArray<FVector2f> Points{ At + Direction * Start, At + Direction * (Start + Length) };
			FSlateDrawElement::MakeLines(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None,
				Settings.AccentColor.CopyWithNewOpacity(SlashOpacity), true, 2.5f);
		}
	}
	OutDrawElements.PopClip();
	return Layer;
}
