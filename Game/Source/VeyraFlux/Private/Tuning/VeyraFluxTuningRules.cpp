// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraFluxTuning.h"

namespace VeyraFlux
{
TArray<FString> Validate(const FVeyraFluxTuning& Tuning)
{
	TArray<FString> Problems;
	const TPair<const TCHAR*, const FVeyraFluxGrantTuning*> Grants[] = {
		{ TEXT("/grants/laneSpire"), &Tuning.Grants.LaneSpire },
		{ TEXT("/grants/baseTower"), &Tuning.Grants.BaseTower },
		{ TEXT("/grants/inhibitor"), &Tuning.Grants.Inhibitor },
	};
	for (const TPair<const TCHAR*, const FVeyraFluxGrantTuning*>& Grant : Grants)
	{
		const bool bPermanent = Grant.Value->Duration == EVeyraFluxDuration::Permanent;
		if (bPermanent != (Grant.Value->DurationSeconds == 0.0))
		{
			Problems.Add(FString::Printf(TEXT("%s/durationSeconds: 0 for a permanent grant, and above 0 for a temporary one"), Grant.Key));
		}
	}
	return Problems;
}
}
