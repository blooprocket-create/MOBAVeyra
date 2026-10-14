// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "VeyraHudMotion.generated.h"

/**
 * A timing curve as a cubic Bézier from (0,0) to (1,1) through two control points, as a stylesheet writes one: how a
 * part of the HUD moves over its time (ADR-082 §1).
 */
USTRUCT()
struct FVeyraEaseCurve
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "Motion")
	float X1 = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Motion")
	float Y1 = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Motion")
	float X2 = 1.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Motion")
	float Y2 = 1.0f;

	/** Whether it is a curve at all: both control points' times inside 0 to 1, so each time has one value. */
	bool IsValid() const { return X1 >= 0.0f && X1 <= 1.0f && X2 >= 0.0f && X2 <= 1.0f; }
};

namespace VeyraHudEase
{
	/** Curve's value at Time, 0 at 0 and 1 at 1; Time is clamped to that range. A curve may overshoot between them. */
	VEYRAUI_API double Evaluate(const FVeyraEaseCurve& Curve, double Time);

	/** How far through Seconds an animation is After seconds in, 0 to 1; one of no length is over. */
	VEYRAUI_API double Progress(double After, double Seconds);
}

/**
 * What the HUD remembers between frames to move (ADR-082 §2): when something turned on, a bar's drained part that lags its
 * value, and a count that runs to its new value. Presentation only, on the client, keyed by name; a key no frame asks for
 * is forgotten. With Reduce UI Animation every moment reads as long over and every value as it is.
 */
class VEYRAUI_API FVeyraHudMotion
{
public:
	/** A frame starts at Now, real seconds; Reduced holds every motion still. Forgets keys no recent frame asked for. */
	void Begin(double Now, bool bReduced);

	double GetNow() const { return Now; }
	bool IsReduced() const { return bReduced; }

	/** Seconds since Key fired; a very large number when it never has, or under Reduce UI Animation. */
	double SinceFired(FName Key) const;

	/** Key fires now. */
	void Fire(FName Key);

	/**
	 * Fires Key when bOn turns true, though not on the frame Key is first seen, so what is already on when the HUD starts
	 * plays nothing; returns SinceFired.
	 */
	double OnRise(FName Key, bool bOn);

	/**
	 * Seconds Key has been on with this Identity; negative while it is off. A new identity starts it again, as a tooltip
	 * moving to another tile.
	 */
	double Held(FName Key, bool bOn, uint32 Identity = 0);

	/**
	 * A bar's trail behind Value (Key's own): a fall holds for HoldSeconds then drains to it over DrainSeconds along Curve;
	 * a rise is followed at once.
	 */
	double Trail(FName Key, double Value, double HoldSeconds, double DrainSeconds, const FVeyraEaseCurve& Curve);

	/** A count: Value as shown, running from the last value to a new one over Seconds along Curve. */
	struct FCount
	{
		double Shown = 0.0;
		/** Seconds since it last changed, and whether that was a rise; a very large number when it never has. */
		double SinceChange = 0.0;
		bool bRose = false;
	};
	FCount Count(FName Key, double Value, double Seconds, const FVeyraEaseCurve& Curve);

	/** How many keys it remembers. For tests. */
	int32 Num() const { return Entries.Num(); }

	/** A moment so long ago that every animation has ended. */
	static constexpr double LongAgo = 1.0e9;

private:
	struct FEntry
	{
		double Touched = 0.0;
		double FiredAt = -LongAgo;
		bool bSeen = false;
		bool bOn = false;
		uint32 Identity = 0;
		double OnSince = 0.0;
		/** A trail's or a count's run: from, to, and when it started; the count's last change and its direction. */
		double From = 0.0;
		double To = 0.0;
		double StartedAt = -LongAgo;
		bool bRose = false;
	};

	FEntry& Touch(FName Key);

	TMap<FName, FEntry> Entries;
	double Now = 0.0;
	bool bReduced = false;
};
