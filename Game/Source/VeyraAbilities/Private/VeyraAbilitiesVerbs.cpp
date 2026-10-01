// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraAbilitiesVerbs.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/VeyraGameplayAbility.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
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

void RefundCooldown(UAbilitySystemComponent& Caster, const FVeyraContentId& Ability, double Fraction)
{
	const AActor* Owner = Caster.GetOwner();
	const UVeyraAbilityLoadoutComponent* Loadout = Owner ? Owner->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
	UVeyraCooldownComponent* Cooldowns = Owner ? Owner->FindComponentByClass<UVeyraCooldownComponent>() : nullptr;
	if (Loadout && Cooldowns && Ability.IsValid())
	{
		// Keyed as its cooldown started: a variant shares its base ability's.
		Cooldowns->ReduceCooldown(Loadout->CooldownIdOf(Ability), Fraction);
	}
}

void ShortenCooldown(UAbilitySystemComponent& Caster, EVeyraAbilitySlot Slot, double Seconds)
{
	const AActor* Owner = Caster.GetOwner();
	const UVeyraAbilityLoadoutComponent* Loadout = Owner ? Owner->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
	UVeyraCooldownComponent* Cooldowns = Owner ? Owner->FindComponentByClass<UVeyraCooldownComponent>() : nullptr;
	const FVeyraLoadoutEntry* Entry = Loadout ? Loadout->FindSlot(Slot) : nullptr;
	if (Cooldowns && Entry && Seconds > 0.0)
	{
		Cooldowns->ReduceCooldown(Loadout->CooldownIdOf(Entry->Ability), 0.0, Seconds);
	}
}

int32 RankOf(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability)
{
	const AActor* Owner = Caster.GetOwner();
	const UVeyraAbilityLoadoutComponent* Loadout = Owner ? Owner->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
	const FVeyraLoadoutEntry* Entry = Loadout ? Loadout->FindAbility(Ability) : nullptr;
	// An item's Active and a Flux Spell have no ranks: each works at its one rank (ADR-012 §1, ADR-015 §1).
	if (Entry && (VeyraAbilitySlots::IsItemSlot(Entry->Slot) || VeyraAbilitySlots::IsSpellSlot(Entry->Slot)))
	{
		return 1;
	}
	const UVeyraProgressionComponent* Progression = Owner ? Owner->FindComponentByClass<UVeyraProgressionComponent>() : nullptr;
	return Entry && Progression ? Progression->GetRank(Entry->Slot) : 0;
}

double ResourceCostOf(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability)
{
	return VeyraAbilityRules::ResourceCost(UVeyraAbilitiesTuningSubsystem::Get(), Ability, RankOf(Caster, Ability));
}

void ShortenSoonestCooldown(UAbilitySystemComponent& Caster, TConstArrayView<EVeyraAbilitySlot> Slots, double Seconds)
{
	const AActor* Owner = Caster.GetOwner();
	const UVeyraAbilityLoadoutComponent* Loadout = Owner ? Owner->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
	const UVeyraCooldownComponent* Cooldowns = Owner ? Owner->FindComponentByClass<UVeyraCooldownComponent>() : nullptr;
	if (!Loadout || !Cooldowns || !(Seconds > 0.0))
	{
		return;
	}
	TOptional<EVeyraAbilitySlot> Soonest;
	double SoonestLeft = 0.0;
	for (const EVeyraAbilitySlot Slot : Slots)
	{
		const FVeyraLoadoutEntry* Entry = Loadout->FindSlot(Slot);
		const double Left = Entry ? Cooldowns->GetRemainingSecondsNow(Loadout->CooldownIdOf(Entry->Ability)) : 0.0;
		if (Left > 0.0 && (!Soonest.IsSet() || Left < SoonestLeft))
		{
			Soonest = Slot;
			SoonestLeft = Left;
		}
	}
	if (Soonest.IsSet())
	{
		ShortenCooldown(Caster, Soonest.GetValue(), Seconds);
	}
}

void EndFollowUp(UAbilitySystemComponent& Caster, const FVeyraContentId& OpenedBy)
{
	const FVeyraCastTuning* Cast = VeyraAbilityRules::FindCast(UVeyraAbilitiesTuningSubsystem::Get(), OpenedBy);
	const AActor* Owner = Caster.GetOwner();
	UVeyraAbilityLoadoutComponent* Loadout = Owner ? Owner->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
	if (!Cast || Cast->RecastWindow.IsEmpty() || !Loadout)
	{
		return;
	}
	const FVeyraContentId& FollowUp = Cast->RecastWindow[0].Ability;
	const FVeyraLoadoutEntry* Entry = Loadout->FindAbility(FollowUp);
	// Whether it shows now or waits in another stance.
	const FVeyraSlotOverride* Current = Entry ? Loadout->FindOverride(Entry->Slot) : nullptr;
	if (Current && Current->Entry.Ability == FollowUp)
	{
		Loadout->EndOverride(Caster, Entry->Slot);
	}
}

void RefundCooldowns(UAbilitySystemComponent& Caster, TConstArrayView<EVeyraAbilitySlot> Slots, double Fraction)
{
	const AActor* Owner = Caster.GetOwner();
	const UVeyraAbilityLoadoutComponent* Loadout = Owner ? Owner->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
	for (const EVeyraAbilitySlot Slot : Slots)
	{
		if (const FVeyraLoadoutEntry* Entry = Loadout ? Loadout->FindSlot(Slot) : nullptr)
		{
			RefundCooldown(Caster, Entry->Ability, Fraction);
		}
	}
}
}
