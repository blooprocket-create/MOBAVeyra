// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "NativeGameplayTags.h"

// Combat states a unit can be in. Naming rules: PROJECT_STRUCTURE.md §5, "Gameplay Tag vocabulary".
namespace VeyraTags
{
	/** Damage of every type reduces Health by 0, and nothing absorbs it (Combat Bible §10, §25 step 7). */
	VEYRACORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Invulnerable);
}
