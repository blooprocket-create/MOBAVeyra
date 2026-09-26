// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "NativeGameplayTags.h"

// Health vocabulary. Naming rules: PROJECT_STRUCTURE.md §5, "Gameplay Tag vocabulary".
namespace VeyraTags
{
	/** Marks a grant of Temporary Health (Combat Bible §7) and keys its amount. */
	VEYRACORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TemporaryHealth);
}
