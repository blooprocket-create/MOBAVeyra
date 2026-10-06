// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Effects/VeyraEffectsEnum.h"

int32 VeyraEffects::FindByAuthoredName(TConstArrayView<FText> DisplayNames, const FString& Name)
{
	return DisplayNames.IndexOfByPredicate([&Name](const FText& Display) { return Display.BuildSourceString() == Name; });
}
