// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"

namespace VeyraEffects
{
	/**
	 * The index of the entry of DisplayNames whose name, as authored, is Name; INDEX_NONE if none has it. An enum's
	 * display names are localized text, so an editor running in another culture shows them translated. The effects
	 * spec names an entry as it was authored (its source text), which is the same under every culture, never by a
	 * translation.
	 */
	int32 FindByAuthoredName(TConstArrayView<FText> DisplayNames, const FString& Name);
}
