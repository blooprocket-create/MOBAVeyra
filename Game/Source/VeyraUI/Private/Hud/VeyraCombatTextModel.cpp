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
		}
		return false;
	}

	/** A number drawn whole: under half a point rounds to nothing. */
	constexpr double LeastShownAmount = 0.5;

	struct FRunning
	{
		FVeyraCombatTextShown Shown;
		TWeakObjectPtr<AActor> Other;
		double LastAt = 0.0;
	};
}

void Forget(TArray<FVeyraCombatTextArrival>& Arrivals, double Now, double ShowSeconds)
{
	Arrivals.RemoveAll([Now, ShowSeconds](const FVeyraCombatTextArrival& Arrival) { return Now - Arrival.ReceivedAt >= ShowSeconds; });
}

TArray<FVeyraCombatTextShown> Describe(TConstArrayView<FVeyraCombatTextArrival> Arrivals, double Now, const FVeyraCombatTextOptions& Options)
{
	TArray<FRunning> Running;
	for (const FVeyraCombatTextArrival& Arrival : Arrivals)
	{
		const FVeyraCombatTextLine& Line = Arrival.Line;
		if (!IsShown(Line.Kind, Options))
		{
			continue;
		}
		const bool bCritical = Line.bCritical && Options.bCritEmphasis;
		// The latest total between the same two units, of the same kind and type, takes a number arriving soon enough.
		FRunning* Joins = nullptr;
		if (Options.bReduced)
		{
			for (int32 Index = Running.Num() - 1; Index >= 0 && !Joins; --Index)
			{
				FRunning& Each = Running[Index];
				if (Each.Shown.Unit.Get() == Line.Unit.Get() && Each.Other.Get() == Line.Other.Get() && Each.Shown.Kind == Line.Kind && Each.Shown.DamageType == Line.DamageType)
				{
					Joins = Arrival.ReceivedAt - Each.LastAt <= Options.MergeSeconds ? &Each : nullptr;
					break;
				}
			}
		}
		if (Joins)
		{
			Joins->Shown.Amount += Line.Amount;
			Joins->Shown.bCritical |= bCritical;
			Joins->LastAt = Arrival.ReceivedAt;
			continue;
		}
		FRunning& Started = Running.AddDefaulted_GetRef();
		Started.Shown.Unit = Line.Unit.Get();
		Started.Shown.Kind = Line.Kind;
		Started.Shown.DamageType = Line.DamageType;
		Started.Shown.bCritical = bCritical;
		Started.Shown.Amount = Line.Amount;
		Started.Other = Line.Other.Get();
		Started.LastAt = Arrival.ReceivedAt;
	}
	TArray<FVeyraCombatTextShown> Shown;
	for (FRunning& Each : Running)
	{
		const double Age = FMath::Max(0.0, Now - Each.LastAt);
		if (Options.ShowSeconds > 0.0 && Age < Options.ShowSeconds && Each.Shown.Amount >= LeastShownAmount)
		{
			Each.Shown.Progress = Age / Options.ShowSeconds;
			Shown.Add(Each.Shown);
		}
	}
	return Shown;
}
}
