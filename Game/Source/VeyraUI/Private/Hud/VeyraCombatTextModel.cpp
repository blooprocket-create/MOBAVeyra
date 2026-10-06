// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Hud/VeyraCombatTextModel.h"

namespace VeyraCombatTextView
{
namespace
{
	bool IsShown(EVeyraCombatTextKind Kind, const FVeyraCombatTextOptions& Options)
	{
		switch (Kind)
		{
		case EVeyraCombatTextKind::DamageDealt:
			return Options.bDamageDealt;
		case EVeyraCombatTextKind::DamageReceived:
			return Options.bDamageReceived;
		case EVeyraCombatTextKind::Healing:
			return Options.bHealing;
		case EVeyraCombatTextKind::Shielding:
			return Options.bShielding;
		case EVeyraCombatTextKind::Gold:
			return Options.bGold;
		}
		return false;
	}

	/** How soon a number must follow the last it would join, or nothing when it joins none: Gold always merges. */
	TOptional<double> MergeWindow(EVeyraCombatTextKind Kind, const FVeyraCombatTextOptions& Options)
	{
		if (Kind == EVeyraCombatTextKind::Gold)
		{
			return Options.GoldMergeSeconds;
		}
		return Options.bReduced ? Options.MergeSeconds : TOptional<double>();
	}

	/** A number drawn whole: under half a point rounds to nothing. */
	constexpr double LeastShownAmount = 0.5;

	struct FRunning
	{
		FVeyraCombatTextShown Shown;
		TWeakObjectPtr<AActor> Other;
		double LastAt = 0.0;
		/** The arrivals it adds up, by index. */
		TArray<int32, TInlineAllocator<4>> Parts;
	};

	/**
	 * The totals Arrivals add up to, in the order they arrived, without the kinds the player turned off: under Reduced
	 * density a number arriving within MergeSeconds of the last between the same two units, of the same kind and type,
	 * joins that running total.
	 */
	TArray<FRunning> RunningTotals(TConstArrayView<FVeyraCombatTextArrival> Arrivals, const FVeyraCombatTextOptions& Options)
	{
		TArray<FRunning> Running;
		for (int32 Index = 0; Index < Arrivals.Num(); ++Index)
		{
			const FVeyraCombatTextArrival& Arrival = Arrivals[Index];
			const FVeyraCombatTextLine& Line = Arrival.Line;
			if (!IsShown(Line.Kind, Options))
			{
				continue;
			}
			const bool bCritical = Line.bCritical && Options.bCritEmphasis;
			// The latest total between the same two units, or at the same place, of the same kind and type, takes a number
			// arriving soon enough.
			FRunning* Joins = nullptr;
			if (const TOptional<double> Window = MergeWindow(Line.Kind, Options))
			{
				for (int32 Latest = Running.Num() - 1; Latest >= 0; --Latest)
				{
					FRunning& Each = Running[Latest];
					const bool bSamePlace = Each.Shown.bFixed == Line.bFixed && (!Line.bFixed || Each.Shown.Where.Equals(FVector(Line.Where)));
					if (Each.Shown.Unit.Get() == Line.Unit.Get() && Each.Other.Get() == Line.Other.Get() && bSamePlace && Each.Shown.Kind == Line.Kind
						&& Each.Shown.DamageType == Line.DamageType)
					{
						Joins = Arrival.ReceivedAt - Each.LastAt <= Window.GetValue() ? &Each : nullptr;
						break;
					}
				}
			}
			if (Joins)
			{
				Joins->Shown.Amount += Line.Amount;
				Joins->Shown.bCritical |= bCritical;
				Joins->LastAt = Arrival.ReceivedAt;
				Joins->Parts.Add(Index);
				continue;
			}
			FRunning& Started = Running.AddDefaulted_GetRef();
			Started.Shown.Unit = Line.Unit.Get();
			Started.Shown.Kind = Line.Kind;
			Started.Shown.DamageType = Line.DamageType;
			Started.Shown.bCritical = bCritical;
			Started.Shown.Amount = Line.Amount;
			Started.Shown.bFixed = Line.bFixed;
			Started.Shown.Where = Line.Where;
			Started.Other = Line.Other.Get();
			Started.LastAt = Arrival.ReceivedAt;
			Started.Parts.Add(Index);
		}
		return Running;
	}

	/** How long ago a total's latest part arrived, at Now. */
	double AgeOf(const FRunning& Running, double Now)
	{
		return FMath::Max(0.0, Now - Running.LastAt);
	}
}

void Forget(TArray<FVeyraCombatTextArrival>& Arrivals, double Now, const FVeyraCombatTextOptions& Options)
{
	// An arrival goes with the total it is part of, which shows from its latest part.
	TBitArray<> Showing(false, Arrivals.Num());
	for (const FRunning& Each : RunningTotals(Arrivals, Options))
	{
		if (AgeOf(Each, Now) < Options.ShowSeconds)
		{
			for (const int32 Part : Each.Parts)
			{
				Showing[Part] = true;
			}
		}
	}
	TArray<FVeyraCombatTextArrival> Kept;
	for (int32 Index = 0; Index < Arrivals.Num(); ++Index)
	{
		if (Showing[Index])
		{
			Kept.Add(MoveTemp(Arrivals[Index]));
		}
	}
	Arrivals = MoveTemp(Kept);
}

TArray<FVeyraCombatTextShown> Describe(TConstArrayView<FVeyraCombatTextArrival> Arrivals, double Now, const FVeyraCombatTextOptions& Options)
{
	TArray<FVeyraCombatTextShown> Shown;
	for (FRunning& Each : RunningTotals(Arrivals, Options))
	{
		const double Age = AgeOf(Each, Now);
		if (Options.ShowSeconds > 0.0 && Age < Options.ShowSeconds && Each.Shown.Amount >= LeastShownAmount)
		{
			Each.Shown.Progress = Age / Options.ShowSeconds;
			Shown.Add(Each.Shown);
		}
	}
	return Shown;
}
}
