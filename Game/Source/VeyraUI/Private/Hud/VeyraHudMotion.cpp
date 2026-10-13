// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Hud/VeyraHudMotion.h"

namespace VeyraHudEase
{
namespace
{
	/** One coordinate of the curve at parameter T, whose inner control points have that coordinate A and B. */
	double Bezier(double A, double B, double T)
	{
		const double U = 1.0 - T;
		return 3.0 * U * U * T * A + 3.0 * U * T * T * B + T * T * T;
	}

	/** Its slope in T. */
	double Slope(double A, double B, double T)
	{
		const double U = 1.0 - T;
		return 3.0 * U * U * A + 6.0 * U * T * (B - A) + 3.0 * T * T * (1.0 - B);
	}
}

double Evaluate(const FVeyraEaseCurve& Curve, double Time)
{
	const double X = FMath::Clamp(Time, 0.0, 1.0);
	if (X <= 0.0 || X >= 1.0)
	{
		return X;
	}
	// The parameter whose time is X: Newton's steps from X, then halving where the slope is too flat to trust. Numerical
	// precision, not a look: a thousandth of the animation is finer than a frame.
	constexpr int32 NewtonSteps = 8;
	constexpr int32 HalvingSteps = 24;
	constexpr double Within = 1.0e-6;
	double T = X;
	for (int32 Step = 0; Step < NewtonSteps; ++Step)
	{
		const double Error = Bezier(Curve.X1, Curve.X2, T) - X;
		if (FMath::Abs(Error) < Within)
		{
			return Bezier(Curve.Y1, Curve.Y2, T);
		}
		const double Rate = Slope(Curve.X1, Curve.X2, T);
		if (FMath::Abs(Rate) < Within)
		{
			break;
		}
		T = FMath::Clamp(T - Error / Rate, 0.0, 1.0);
	}
	double Low = 0.0;
	double High = 1.0;
	T = X;
	for (int32 Step = 0; Step < HalvingSteps; ++Step)
	{
		const double At = Bezier(Curve.X1, Curve.X2, T);
		if (FMath::Abs(At - X) < Within)
		{
			break;
		}
		(At < X ? Low : High) = T;
		T = (Low + High) / 2.0;
	}
	return Bezier(Curve.Y1, Curve.Y2, T);
}

double Progress(double After, double Seconds)
{
	return Seconds > 0.0 ? FMath::Clamp(After / Seconds, 0.0, 1.0) : 1.0;
}
}

namespace
{
	// A key no frame has asked for in this long is forgotten: a unit gone from view, a tile no longer drawn. Housekeeping,
	// not a look; longer than any frame, so nothing drawn every frame is ever lost.
	constexpr double ForgetAfterSeconds = 2.0;
}

void FVeyraHudMotion::Begin(double InNow, bool bInReduced)
{
	Now = InNow;
	bReduced = bInReduced;
	for (auto It = Entries.CreateIterator(); It; ++It)
	{
		if (Now - It.Value().Touched > ForgetAfterSeconds)
		{
			It.RemoveCurrent();
		}
	}
}

FVeyraHudMotion::FEntry& FVeyraHudMotion::Touch(FName Key)
{
	FEntry& Entry = Entries.FindOrAdd(Key);
	Entry.Touched = Now;
	return Entry;
}

double FVeyraHudMotion::SinceFired(FName Key) const
{
	const FEntry* Entry = Entries.Find(Key);
	return bReduced || !Entry ? LongAgo : Now - Entry->FiredAt;
}

void FVeyraHudMotion::Fire(FName Key)
{
	Touch(Key).FiredAt = Now;
}

double FVeyraHudMotion::OnRise(FName Key, bool bOn)
{
	FEntry& Entry = Touch(Key);
	if (Entry.bSeen && !Entry.bOn && bOn)
	{
		Entry.FiredAt = Now;
	}
	Entry.bSeen = true;
	Entry.bOn = bOn;
	return bReduced ? LongAgo : Now - Entry.FiredAt;
}

double FVeyraHudMotion::Held(FName Key, bool bOn, uint32 Identity)
{
	FEntry& Entry = Touch(Key);
	if (bOn && (!Entry.bOn || Entry.Identity != Identity))
	{
		Entry.OnSince = Now;
	}
	Entry.bOn = bOn;
	Entry.Identity = Identity;
	if (!bOn)
	{
		return -1.0;
	}
	return bReduced ? LongAgo : Now - Entry.OnSince;
}

double FVeyraHudMotion::Trail(FName Key, double Value, double HoldSeconds, double DrainSeconds, const FVeyraEaseCurve& Curve)
{
	FEntry& Entry = Touch(Key);
	const auto ShownAt = [&]() {
		const double Since = Now - Entry.StartedAt - HoldSeconds;
		return Since <= 0.0 ? Entry.From : FMath::Lerp(Entry.From, Entry.To, VeyraHudEase::Evaluate(Curve, VeyraHudEase::Progress(Since, DrainSeconds)));
	};
	if (!Entry.bSeen || bReduced)
	{
		Entry.bSeen = true;
		Entry.From = Entry.To = Value;
		Entry.StartedAt = -LongAgo;
		return Value;
	}
	const double Shown = ShownAt();
	if (Value >= Shown)
	{
		// A rise, or a fall the trail has already passed: it follows at once.
		Entry.From = Entry.To = Value;
		Entry.StartedAt = -LongAgo;
		return Value;
	}
	if (Value < Entry.To)
	{
		// A new fall: hold where the trail stands, then drain to the new value.
		Entry.From = Shown;
		Entry.To = Value;
		Entry.StartedAt = Now;
		return Shown;
	}
	return Shown;
}

FVeyraHudMotion::FCount FVeyraHudMotion::Count(FName Key, double Value, double Seconds, const FVeyraEaseCurve& Curve)
{
	FEntry& Entry = Touch(Key);
	if (!Entry.bSeen)
	{
		Entry.bSeen = true;
		Entry.From = Entry.To = Value;
		Entry.StartedAt = -LongAgo;
	}
	const auto ShownAt = [&]() { return FMath::Lerp(Entry.From, Entry.To, VeyraHudEase::Evaluate(Curve, VeyraHudEase::Progress(Now - Entry.StartedAt, Seconds))); };
	if (Value != Entry.To)
	{
		Entry.From = ShownAt();
		Entry.bRose = Value > Entry.To;
		Entry.To = Value;
		Entry.StartedAt = Now;
	}
	if (bReduced)
	{
		return FCount{ Value, LongAgo, Entry.bRose };
	}
	return FCount{ ShownAt(), Now - Entry.StartedAt, Entry.bRose };
}
