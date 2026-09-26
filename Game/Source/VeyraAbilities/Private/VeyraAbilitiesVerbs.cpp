// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraAbilitiesVerbs.h"

#include "AbilitySystemComponent.h"
#include "Abilities/VeyraGameplayAbility.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "VeyraAbilitiesLog.h"

namespace VeyraAbilities
{
EVeyraCastRejection TryCast(UAbilitySystemComponent& Caster, EVeyraAbilitySlot Slot, const FVeyraCastTarget& Target)
{
	const AActor* Owner = Caster.GetOwner();
	const UVeyraAbilityLoadoutComponent* Loadout = Owner ? Owner->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
	const FVeyraLoadoutEntry* Entry = Loadout ? Loadout->FindSlot(Slot) : nullptr;
	const FGameplayAbilitySpec* Spec = Entry ? Caster.FindAbilitySpecFromHandle(Entry->Handle) : nullptr;
	const UVeyraGameplayAbility* Ability = Spec ? Cast<UVeyraGameplayAbility>(Spec->Ability) : nullptr;
	if (!Ability)
	{
		return EVeyraCastRejection::UnknownAbility;
	}

	const EVeyraCastRejection Rejection = Ability->CheckCast(Caster, Entry->Ability, Target);
	if (Rejection != EVeyraCastRejection::None)
	{
		return Rejection;
	}

	// The target travels in the activation's event data, which the server fills; no client target
	// data is involved (ADR-006 §7).
	FGameplayEventData Payload;
	Payload.Instigator = Caster.GetAvatarActor();
	Payload.Target = Target.Actor;
	const bool bActivated = Caster.TriggerAbilityFromGameplayEvent(Entry->Handle, Caster.AbilityActorInfo.Get(), FGameplayTag(), &Payload, Caster);
	UE_CLOG(!bActivated, LogVeyraAbilities, Warning, TEXT("%s passed validation but the ability system did not activate %s."),
		*GetNameSafe(Caster.GetAvatarActor()), *Entry->Ability.ToString());
	UE_CLOG(bActivated, LogVeyraAbilities, Verbose, TEXT("%s cast %s at %s."),
		*GetNameSafe(Caster.GetAvatarActor()), *Entry->Ability.ToString(), *GetNameSafe(Target.Actor));
	return bActivated ? EVeyraCastRejection::None : EVeyraCastRejection::ActivationFailed;
}
}
