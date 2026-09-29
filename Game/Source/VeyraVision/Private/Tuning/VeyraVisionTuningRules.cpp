// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraVisionTuning.h"

namespace VeyraVision
{
TArray<FString> Validate(const FVeyraVisionTuning& /*Tuning*/)
{
	// The schema bounds every value; the vision tools bring the domain's cross-checks (ADR-016 §6).
	return TArray<FString>();
}
}
