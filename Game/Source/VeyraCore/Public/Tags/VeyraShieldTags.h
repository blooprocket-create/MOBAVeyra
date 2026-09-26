// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "NativeGameplayTags.h"

// The three shield categories (Combat Bible §7). A shield effect carries its category's tag, which
// also keys the shield's amount. Naming rules: PROJECT_STRUCTURE.md §5, "Gameplay Tag vocabulary".
namespace VeyraTags
{
	VEYRACORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Shield_Type_Physical);
	VEYRACORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Shield_Type_Magic);
	VEYRACORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Shield_Type_Universal);
}
