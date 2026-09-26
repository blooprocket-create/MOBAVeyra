// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraCombatTagMapping.h"

#include "Tags/VeyraDamageTags.h"
#include "Tags/VeyraShieldTags.h"

namespace VeyraCombatTagMapping
{
FGameplayTag DamageTypeTag(EVeyraDamageType Type)
{
	switch (Type)
	{
	case EVeyraDamageType::Physical:
		return VeyraTags::Damage_Type_Physical;
	case EVeyraDamageType::Magic:
		return VeyraTags::Damage_Type_Magic;
	case EVeyraDamageType::TrueDamage:
		return VeyraTags::Damage_Type_True;
	}
	return FGameplayTag();
}

TOptional<EVeyraDamageType> DamageTypeFromTag(const FGameplayTag& Tag)
{
	for (const EVeyraDamageType Type : { EVeyraDamageType::Physical, EVeyraDamageType::Magic, EVeyraDamageType::TrueDamage })
	{
		if (Tag.MatchesTagExact(DamageTypeTag(Type)))
		{
			return Type;
		}
	}
	return {};
}

FGameplayTag ShieldCategoryTag(EVeyraShieldCategory Category)
{
	switch (Category)
	{
	case EVeyraShieldCategory::Physical:
		return VeyraTags::Shield_Type_Physical;
	case EVeyraShieldCategory::Magic:
		return VeyraTags::Shield_Type_Magic;
	case EVeyraShieldCategory::Universal:
		return VeyraTags::Shield_Type_Universal;
	}
	return FGameplayTag();
}

TOptional<EVeyraShieldCategory> ShieldCategoryFromTag(const FGameplayTag& Tag)
{
	for (const EVeyraShieldCategory Category : { EVeyraShieldCategory::Physical, EVeyraShieldCategory::Magic, EVeyraShieldCategory::Universal })
	{
		if (Tag.MatchesTagExact(ShieldCategoryTag(Category)))
		{
			return Category;
		}
	}
	return {};
}
}
