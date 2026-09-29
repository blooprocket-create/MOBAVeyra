// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraVisionTuning.h"

namespace VeyraVision
{
TArray<FString> Validate(const FVeyraVisionTuning& Tuning)
{
	TArray<FString> Problems;
	// A capsule is never shorter than it is wide.
	if (Tuning.PersistentWard.BodyHalfHeight < Tuning.PersistentWard.BodyRadius)
	{
		Problems.Add(TEXT("/persistentWard/bodyHalfHeight: at least bodyRadius"));
	}
	return Problems;
}
}
