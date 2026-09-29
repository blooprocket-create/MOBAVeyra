// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraAbilitiesVerbs.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/VeyraGameplayAbility.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "VeyraAbilitiesLog.h"

namespace VeyraAbilities
{
EVeyraCastRejection TryCast(UAbilitySystemComponent& Caster, EVeyraAbilitySlot Slot, const FVeyraCastTarget& Target)
{
	const AActor* Owner = Caster.GetOwner();
	const UVeyraAbilityLoadoutComponent* Loadout = Owner ? Owner->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
	// A locked spell slot refuses before anything else, whatever it holds (ADR-015 §4).
	if (Loadout && Loadout->IsLocked(Slot))
	{
		return EVeyraCastRejection::Locked;
	}
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
	// data is involved (ADR-006 §7). A ground point goes as a literal location.
	FGameplayEventData Payload;
	Payload.Instigator = Caster.GetAvatarActor();
	Payload.Target = Target.Actor;
	if (Target.bHasLocation)
	{
		FGameplayAbilityTargetData_LocationInfo* Point = new FGameplayAbilityTargetData_LocationInfo();
		Point->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
		Point->TargetLocation.LiteralTransform = FTransform(Target.Location);
		Payload.TargetData.Add(Point);
	}
	// Copied: a used-once override leaves the loadout as it commits, inside the activation, and the
	// entry's place in it with it.
	const FVeyraContentId Cast = Entry->Ability;
	const bool bActivated = Caster.TriggerAbilityFromGameplayEvent(Entry->Handle, Caster.AbilityActorInfo.Get(), FGameplayTag(), &Payload, Caster);
	UE_CLOG(!bActivated, LogVeyraAbilities, Warning, TEXT("%s passed validation but the ability system did not activate %s."),
		*GetNameSafe(Caster.GetAvatarActor()), *Cast.ToString());
	UE_CLOG(bActivated, LogVeyraAbilities, Verbose, TEXT("%s cast %s at %s."),
		*GetNameSafe(Caster.GetAvatarActor()), *Cast.ToString(), *GetNameSafe(Target.Actor));
	return bActivated ? EVeyraCastRejection::None : EVeyraCastRejection::ActivationFailed;
}
}
