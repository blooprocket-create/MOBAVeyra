// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "NativeGameplayTags.h"

// Combat states a unit can be in. Naming rules: PROJECT_STRUCTURE.md §5, "Gameplay Tag vocabulary".
namespace VeyraTags
{
	/** Damage of every type reduces Health by 0, and nothing absorbs it (Combat Bible §10, §25 step 7). */
	VEYRACORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Invulnerable);

	/**
	 * The unit's Health is a meter its owner sets (ADR-050 §3), as an Echo's Integrity: damage reduces it by 0, as
	 * Invulnerability does, and no heal or shield reaches it.
	 */
	VEYRACORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_SealedHealth);
}
