// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraCombatTuning.h"

namespace VeyraCombatTuningRules
{
TArray<FString> Validate(const FVeyraCombatTuning& Tuning)
{
	TArray<FString> Problems;
	double PreviousFrom = 0.0;
	for (int32 Index = 0; Index < Tuning.Movement.SoftCaps.Num(); ++Index)
	{
		const double From = Tuning.Movement.SoftCaps[Index].From;
		if (Index > 0 && From <= PreviousFrom)
		{
			Problems.Add(FString::Printf(TEXT("/movement/softCaps/%d/from: must be above the previous cap's"), Index));
		}
		PreviousFrom = From;
	}
	return Problems;
}
}
