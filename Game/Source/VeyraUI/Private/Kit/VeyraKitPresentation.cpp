// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Kit/VeyraKitPresentation.h"

#include "Statuses/VeyraStatusTypes.h"
#include "Tuning/VeyraAbilitiesTuning.h"

FVeyraKitPresentationIndex FVeyraKitPresentationIndex::Build(const FVeyraAbilitiesTuning& Tuning)
{
	FVeyraKitPresentationIndex Index;
	for (const TPair<FVeyraContentId, FVeyraSelfBuffAbilityTuning>& Each : Tuning.SelfBuff)
	{
		const FVeyraSelfBuffAbilityTuning& Buff = Each.Value;
		if (Buff.Statuses.IsEmpty() || (Buff.Aura.IsEmpty() && Buff.EndPayload.IsEmpty()))
		{
			continue;
		}
		FBuffShape& Shape = Index.ByStatus.FindOrAdd(Buff.Statuses[0]);
		for (const FVeyraAuraTuning& Aura : Buff.Aura)
		{
			Shape.Auras.Emplace(Aura.Radius, Aura.DurationSeconds);
		}
		// The buff fires its first payload alone. One that needs hits its caster took cannot be told from what a client
		// receives, so it shows nothing rather than a burst that may never come (ADR-071 §3).
		if (!Buff.EndPayload.IsEmpty() && Buff.EndPayload[0].MinHits == 0)
		{
			Shape.Bursts.Emplace(Buff.EndPayload[0].AfterSeconds, Buff.EndPayload[0].Radius);
		}
	}
	return Index;
}

TArray<FVeyraShownAura> FVeyraKitPresentationIndex::AurasOf(const FVeyraStatusEntry& Entry, double Now) const
{
	TArray<FVeyraShownAura> Shown;
	if (const FBuffShape* Shape = ByStatus.Find(Entry.Id))
	{
		for (const TPair<double, double>& Aura : Shape->Auras)
		{
			const double EndsAt = Entry.StartedAt + Aura.Value;
			if (Now < EndsAt)
			{
				Shown.Add(FVeyraShownAura{ Aura.Key, EndsAt });
			}
		}
	}
	return Shown;
}

TArray<FVeyraShownBurst> FVeyraKitPresentationIndex::BurstsOf(const FVeyraStatusEntry& Entry) const
{
	TArray<FVeyraShownBurst> Shown;
	if (const FBuffShape* Shape = ByStatus.Find(Entry.Id))
	{
		for (const TPair<double, double>& Burst : Shape->Bursts)
		{
			Shown.Add(FVeyraShownBurst{ Entry.StartedAt + Burst.Key, Burst.Value });
		}
	}
	return Shown;
}

TOptional<double> VeyraKitPresentation::BurstShareAt(const FVeyraShownBurst& Burst, double Now, double Seconds)
{
	if (!(Seconds > 0.0) || Now < Burst.At || Now > Burst.At + Seconds)
	{
		return {};
	}
	return (Now - Burst.At) / Seconds;
}

double VeyraKitPresentation::BeadShareAt(int32 Bead, int32 Count, double Now, double FlowSeconds)
{
	if (Count <= 0 || !(FlowSeconds > 0.0))
	{
		return 0.0;
	}
	const double Offset = static_cast<double>(Bead) / Count;
	return FMath::Frac(Now / FlowSeconds + Offset);
}
