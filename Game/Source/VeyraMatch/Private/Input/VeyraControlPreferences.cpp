// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Input/VeyraControlPreferences.h"

#include "VeyraSettingsStore.h"

EVeyraCastMode FVeyraControlPreferences::CastModeOf(EVeyraAbilitySlot Slot) const
{
	const EVeyraCastMode* Mode = CastModes.Find(VeyraControlPreferences::CastMode(Slot));
	return Mode ? *Mode : EVeyraCastMode::Quick;
}

namespace VeyraControlPreferences
{
namespace
{
	FVeyraContentId IdOf(const TCHAR* Text)
	{
		// The registry's validation and the tests keep these real.
		return FVeyraContentId::FromText(Text).GetValue();
	}
}

const FVeyraContentId& CastMode(EVeyraAbilitySlot Slot)
{
	static const FVeyraContentId Q = IdOf(TEXT("controls_cast_mode_q"));
	static const FVeyraContentId W = IdOf(TEXT("controls_cast_mode_w"));
	static const FVeyraContentId E = IdOf(TEXT("controls_cast_mode_e"));
	static const FVeyraContentId R = IdOf(TEXT("controls_cast_mode_r"));
	static const FVeyraContentId Spell1 = IdOf(TEXT("controls_cast_mode_spell1"));
	static const FVeyraContentId Spell2 = IdOf(TEXT("controls_cast_mode_spell2"));
	static const FVeyraContentId Items = IdOf(TEXT("controls_cast_mode_items"));
	switch (Slot)
	{
	case EVeyraAbilitySlot::Q:
		return Q;
	case EVeyraAbilitySlot::W:
		return W;
	case EVeyraAbilitySlot::E:
		return E;
	case EVeyraAbilitySlot::R:
		return R;
	case EVeyraAbilitySlot::Spell1:
		return Spell1;
	case EVeyraAbilitySlot::Spell2:
		return Spell2;
	default:
		return Items;
	}
}

TArray<FVeyraContentId> CastModeSettings()
{
	return { CastMode(EVeyraAbilitySlot::Q), CastMode(EVeyraAbilitySlot::W), CastMode(EVeyraAbilitySlot::E), CastMode(EVeyraAbilitySlot::R),
		CastMode(EVeyraAbilitySlot::Spell1), CastMode(EVeyraAbilitySlot::Spell2), CastMode(EVeyraAbilitySlot::Item1) };
}

FVeyraControlPreferences Resolve(const FVeyraSettingsStore* Store)
{
	FVeyraControlPreferences Preferences;
	if (!Store)
	{
		return Preferences;
	}
	for (const FVeyraContentId& Id : CastModeSettings())
	{
		if (const TOptional<EVeyraCastMode> Mode = VeyraCastModes::Parse(Store->Get(Id)))
		{
			Preferences.CastModes.Add(Id, Mode.GetValue());
		}
	}
	return Preferences;
}
}
