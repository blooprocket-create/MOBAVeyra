// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Water/VeyraWaterRipples.h"

#include "Layout/VeyraRiver.h"

FVeyraRippleWindow VeyraWaterRipples::Follow(const FVeyraRippleWindow& Previous, const FVector2D& Focus, double Size, int32 Resolution)
{
	FVeyraRippleWindow Window;
	if (!(Size > 0.0) || Resolution < 1)
	{
		return Window;
	}
	const double Texel = Size / Resolution;
	const FVector2D Corner = Focus - FVector2D(Size * 0.5);
	const FIntPoint Texels(FMath::FloorToInt32(Corner.X / Texel), FMath::FloorToInt32(Corner.Y / Texel));
	Window.Origin = FVector2D(Texels.X * Texel, Texels.Y * Texel);
	Window.bPlaced = true;
	if (Previous.bPlaced)
	{
		Window.Shift = FIntPoint(FMath::RoundToInt32((Window.Origin.X - Previous.Origin.X) / Texel), FMath::RoundToInt32((Window.Origin.Y - Previous.Origin.Y) / Texel));
	}
	return Window;
}

double VeyraWaterRipples::StrengthAt(double Speed, double Still, double Moving, double FullSpeed)
{
	const double Share = FullSpeed > 0.0 ? FMath::Clamp(Speed / FullSpeed, 0.0, 1.0) : 1.0;
	return FMath::Lerp(Still, Moving, Share);
}

TOptional<FLinearColor> VeyraWaterRipples::SourceIn(const FVeyraRippleWindow& Window, double Size, const FVector2D& At, double Radius, double Strength)
{
	if (!Window.bPlaced || !(Size > 0.0))
	{
		return {};
	}
	const FVector2D Share = (At - Window.Origin) / Size;
	const double Reach = Radius / Size;
	if (Share.X < -Reach || Share.Y < -Reach || Share.X > 1.0 + Reach || Share.Y > 1.0 + Reach)
	{
		return {};
	}
	return FLinearColor(static_cast<float>(Share.X), static_cast<float>(Share.Y), static_cast<float>(Reach), static_cast<float>(Strength));
}

bool VeyraWaterRipples::Wades(const FVeyraRiverShape& River, double SurfaceZ, const FVector& Feet)
{
	return Feet.Z < SurfaceZ && River.SignedDistance(FVector2D(Feet)) < 0.0;
}

FVector2D VeyraWaterRipples::FocusOf(const FVector& Location, const FVector& Direction, double SurfaceZ)
{
	if (Direction.Z < -UE_KINDA_SMALL_NUMBER && Location.Z > SurfaceZ)
	{
		const double Along = (SurfaceZ - Location.Z) / Direction.Z;
		return FVector2D(Location + Direction * Along);
	}
	return FVector2D(Location);
}
