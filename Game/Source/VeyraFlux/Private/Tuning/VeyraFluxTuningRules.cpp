// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraFluxTuning.h"

#include "Slots/VeyraAbilitySlot.h"

namespace VeyraFlux
{
TArray<FString> Validate(const FVeyraFluxTuning& Tuning)
{
	TArray<FString> Problems;
	const TPair<const TCHAR*, const FVeyraFluxGrantTuning*> Grants[] = {
		{ TEXT("/grants/laneSpire"), &Tuning.Grants.LaneSpire },
		{ TEXT("/grants/baseTower"), &Tuning.Grants.BaseTower },
		{ TEXT("/grants/inhibitor"), &Tuning.Grants.Inhibitor },
		{ TEXT("/grants/fluxWell"), &Tuning.Grants.FluxWell },
	};
	for (const TPair<const TCHAR*, const FVeyraFluxGrantTuning*>& Grant : Grants)
	{
		const bool bPermanent = Grant.Value->Duration == EVeyraFluxDuration::Permanent;
		if (bPermanent != (Grant.Value->DurationSeconds == 0.0))
		{
			Problems.Add(FString::Printf(TEXT("%s/durationSeconds: 0 for a permanent grant, and above 0 for a temporary one"), Grant.Key));
		}
	}
	// One threshold for each spell slot, rising (§14).
	const TArray<double>& Thresholds = Tuning.SpellSlots.Thresholds;
	if (Thresholds.Num() != static_cast<int32>(UE_ARRAY_COUNT(VeyraAbilitySlots::Spells)))
	{
		Problems.Add(FString::Printf(TEXT("/spellSlots/thresholds: needs one threshold for each of the %d Flux Spell slots"),
			static_cast<int32>(UE_ARRAY_COUNT(VeyraAbilitySlots::Spells))));
	}
	for (int32 Index = 1; Index < Thresholds.Num(); ++Index)
	{
		if (!(Thresholds[Index] > Thresholds[Index - 1]))
		{
			Problems.Add(FString::Printf(TEXT("/spellSlots/thresholds/%d: must be above the slot before it"), Index));
		}
	}
	return Problems;
}

int32 UnlockedSpellSlots(double Permanent, TConstArrayView<double> Thresholds)
{
	int32 Unlocked = 0;
	while (Unlocked < Thresholds.Num() && Permanent >= Thresholds[Unlocked])
	{
		++Unlocked;
	}
	return Unlocked;
}
}
