// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Camera/VeyraCameraRules.h"

namespace VeyraCamera
{
FVeyraCameraState Step(const FVeyraCameraState& State, EVeyraCameraMode Mode, const FVeyraCameraInput& Input, const FVeyraCameraLimits& Limits, double DeltaSeconds)
{
	const FVector Moved = (ScreenToGround(Input.Pan) * Limits.PanSpeed + ScreenToGround(Input.EdgePan) * Limits.EdgeScrollSpeed) * DeltaSeconds
		+ ScreenToGround(Input.Drag);
	FVeyraCameraState Next = State;
	if (Input.Vanguard.IsSet() && (Input.bHoldCenter || Mode == EVeyraCameraMode::Locked))
	{
		// Centred, so a Semi-Locked camera starts again from the Vanguard.
		Next.Focus = Input.Vanguard.GetValue();
		Next.Offset = FVector::ZeroVector;
	}
	else if (Input.Vanguard.IsSet() && Mode == EVeyraCameraMode::SemiLocked)
	{
		Next.Offset = ((State.Offset + Moved) * FVector(1.0, 1.0, 0.0)).GetClampedToMaxSize2D(Limits.SemiLockedMaxOffset);
		Next.Focus = Input.Vanguard.GetValue() + Next.Offset;
	}
	else
	{
		Next.Focus = State.Focus + Moved;
		// Where Semi-Locked would pick up from.
		if (Input.Vanguard.IsSet())
		{
			Next.Offset = ((Next.Focus - Input.Vanguard.GetValue()) * FVector(1.0, 1.0, 0.0)).GetClampedToMaxSize2D(Limits.SemiLockedMaxOffset);
		}
	}
	Next.Focus.X = FMath::Clamp(Next.Focus.X, -Limits.HalfExtent, Limits.HalfExtent);
	Next.Focus.Y = FMath::Clamp(Next.Focus.Y, -Limits.HalfExtent, Limits.HalfExtent);
	return Next;
}

EVeyraCameraMode Next(EVeyraCameraMode Mode)
{
	switch (Mode)
	{
	case EVeyraCameraMode::Free:
		return EVeyraCameraMode::Locked;
	case EVeyraCameraMode::Locked:
		return EVeyraCameraMode::SemiLocked;
	case EVeyraCameraMode::SemiLocked:
		break;
	}
	return EVeyraCameraMode::Free;
}

FVector2D EdgePan(const FVector2D& Mouse, const FVector2D& Viewport, double EdgePixels)
{
	FVector2D Pan = FVector2D::ZeroVector;
	if (Viewport.X <= 0.0 || Viewport.Y <= 0.0)
	{
		return Pan;
	}
	Pan.X = Mouse.X <= EdgePixels ? -1.0 : Mouse.X >= Viewport.X - EdgePixels ? 1.0 : 0.0;
	// Screen Y grows down, the pan's up.
	Pan.Y = Mouse.Y <= EdgePixels ? 1.0 : Mouse.Y >= Viewport.Y - EdgePixels ? -1.0 : 0.0;
	return Pan;
}

FVector2D DelayEdgePan(const FVector2D& EdgePan, double DeltaSeconds, double Delay, double& HeldSeconds)
{
	if (EdgePan.IsZero())
	{
		HeldSeconds = 0.0;
		return FVector2D::ZeroVector;
	}
	HeldSeconds += DeltaSeconds;
	return HeldSeconds >= Delay ? EdgePan : FVector2D::ZeroVector;
}

FVector ScreenToGround(const FVector2D& Screen)
{
	return FVector(Screen.Y, Screen.X, 0.0);
}

FVector PanToward(const FVector& From, const FVector& To, double Elapsed, double Seconds)
{
	if (Seconds <= 0.0 || Elapsed >= Seconds)
	{
		return To;
	}
	return FMath::Lerp(From, To, FMath::SmoothStep(0.0, 1.0, FMath::Clamp(Elapsed / Seconds, 0.0, 1.0)));
}
}
