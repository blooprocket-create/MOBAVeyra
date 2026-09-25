// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "NativeGameplayTags.h"

// The three primary damage types (Combat Bible §2). Naming and placement rules for every native
// tag: PROJECT_STRUCTURE.md §5, "Gameplay Tag vocabulary".
namespace VeyraTags
{
	VEYRACORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Type_Physical);
	VEYRACORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Type_Magic);
	VEYRACORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Type_True);
}
