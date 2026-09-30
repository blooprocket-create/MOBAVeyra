// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "InputMappingContext.h"
#include "Input/VeyraInputSettings.h"
#include "UObject/Package.h"
#include "VeyraSettingsStore.h"
#include "VeyraSettingsSubsystem.h"

namespace VeyraKeyBindingTests
{
	FVeyraContentId BindingId(const TCHAR* Text)
	{
		return FVeyraContentId::FromText(Text).GetValue();
	}

	FVeyraSettingsRegistry BindingRegistry()
	{
		FVeyraSettingsRegistry Registry;
		UVeyraSettingsSubsystem::LoadRegistry(Registry);
		return Registry;
	}

	/** The key an action is mapped to in Context; invalid when it has none. */
	FKey MappedKey(const UInputMappingContext& Context, const UInputAction* Action)
	{
		const FEnhancedActionKeyMapping* Mapping = Context.GetMappings().FindByPredicate([Action](const FEnhancedActionKeyMapping& Each) { return Each.Action == Action; });
		return Mapping ? Mapping->Key : EKeys::Invalid;
	}

	// Veyra.Match.KeyBindings.*: the player's keys over the developer's (Settings Bible §1.1; ADR-024 §6).
	TEST_CLASS(KeyBindings, "Veyra.Match")
	{
		const FVeyraSettingsRegistry Registry = BindingRegistry();

		TEST_METHOD(ABindingTakesAKeyNoneOrTheDefault)
		{
			FVeyraSettingsStore Store(Registry);
			const FVeyraContentId Q = BindingId(TEXT("controls_bind_ability_q"));
			ASSERT_THAT(IsTrue(Store.Set(Q, TEXT("t")) == EVeyraSettingChange::Changed && Store.Get(Q) == TEXT("T"), TEXT("a key, as the engine spells it")));
			ASSERT_THAT(IsTrue(Store.Set(Q, TEXT("NotAKey")) == EVeyraSettingChange::InvalidValue));
			ASSERT_THAT(IsTrue(Store.Set(Q, TEXT("MouseX")) == EVeyraSettingChange::InvalidValue, TEXT("an axis is not a key")));
			ASSERT_THAT(IsTrue(Store.Set(Q, VeyraSettings::Unbound()) == EVeyraSettingChange::Changed && Store.Get(Q) == VeyraSettings::Unbound()));
			ASSERT_THAT(IsTrue(Store.Set(Q, TEXT("")) == EVeyraSettingChange::Changed && !Store.IsChanged(Q), TEXT("empty is the developer's key again")));
		}

		TEST_METHOD(ThePlayersKeysGoOverTheDevelopersAndMapTheActions)
		{
			FVeyraSettingsStore Store(Registry);
			Store.Set(BindingId(TEXT("controls_bind_ability_q")), TEXT("T"));
			Store.Set(BindingId(TEXT("controls_bind_recall")), VeyraSettings::Unbound());
			UVeyraInputSettings* Keys = NewObject<UVeyraInputSettings>(GetTransientPackage());
			const UVeyraInputSettings& Developer = *GetDefault<UVeyraInputSettings>();
			ASSERT_THAT(IsTrue(VeyraSettings::ApplyBindings(*Keys, Store) == 2));
			ASSERT_THAT(IsTrue(Keys->AbilityQKey == EKeys::T && !Keys->RecallKey.IsValid() && Keys->AbilityWKey == Developer.AbilityWKey));
			ASSERT_THAT(IsTrue(VeyraSettings::BindingKey(Developer, Store, BindingId(TEXT("controls_bind_ability_w"))).Get(EKeys::Invalid) == Developer.AbilityWKey));

			const FVeyraInputObjects Input = VeyraInput::Build(*Keys, *GetTransientPackage());
			ASSERT_THAT(IsTrue(MappedKey(*Input.MappingContext, Input.AbilityQ) == EKeys::T));
			ASSERT_THAT(IsFalse(MappedKey(*Input.MappingContext, Input.Recall).IsValid(), TEXT("a binding without a key maps nothing")));

			// A rebinding maps the same actions anew.
			Keys->AbilityQKey = EKeys::G;
			const UInputMappingContext* Remapped = VeyraInput::MapKeys(*Keys, Input, *GetTransientPackage());
			ASSERT_THAT(IsTrue(Remapped && MappedKey(*Remapped, Input.AbilityQ) == EKeys::G));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
