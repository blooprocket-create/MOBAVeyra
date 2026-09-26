// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tags/VeyraShieldTags.h"

namespace VeyraTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Shield_Type_Physical, "Shield.Type.Physical", "Physical Shield: absorbs Physical Damage only (Combat Bible §7).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Shield_Type_Magic, "Shield.Type.Magic", "Magic Shield: absorbs Magic Damage only (Combat Bible §7).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Shield_Type_Universal, "Shield.Type.Universal", "Universal Shield: absorbs Physical, Magic and True Damage (Combat Bible §7).");
}
