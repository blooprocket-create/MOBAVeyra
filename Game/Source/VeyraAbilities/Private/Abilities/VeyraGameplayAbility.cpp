// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "VeyraAbilitiesLog.h"
#include "VeyraCombatVerbs.h"

namespace
{
	template <typename ComponentType>
	ComponentType* FindBesideAbilitySystem(const FGameplayAbilityActorInfo* ActorInfo)
	{
		const AActor* Owner = ActorInfo ? ActorInfo->OwnerActor.Get() : nullptr;
		return Owner ? Owner->FindComponentByClass<ComponentType>() : nullptr;
	}

	template <typename ComponentType>
	ComponentType* FindBesideAbilitySystem(const UAbilitySystemComponent& AbilitySystem)
	{
		const AActor* Owner = AbilitySystem.GetOwner();
		return Owner ? Owner->FindComponentByClass<ComponentType>() : nullptr;
	}
}

UVeyraGameplayAbility::UVeyraGameplayAbility()
{
	// Server-only abilities (ADR-006 §7): the client sends a cast intent and the server runs it.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

EVeyraCastRejection UVeyraGameplayAbility::CheckCast(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const
{
	const AActor* Avatar = Caster.GetAvatarActor();
	if (!Defines(Ability))
	{
		return EVeyraCastRejection::UnknownAbility;
	}
	if (!Avatar || !VeyraTargeting::IsAlive(Avatar))
	{
		return EVeyraCastRejection::CasterDead;
	}
	const UVeyraCooldownComponent* Cooldowns = FindBesideAbilitySystem<UVeyraCooldownComponent>(Caster);
	if (!Cooldowns || Cooldowns->GetRemainingSecondsNow(Ability) > 0.0)
	{
		return EVeyraCastRejection::OnCooldown;
	}
	if (!VeyraCombat::CanAffordResource(Caster, GetResourceCost(Ability)))
	{
		return EVeyraCastRejection::InsufficientResource;
	}
	return CheckTarget(*Avatar, Ability, Target);
}

EVeyraCastRejection UVeyraGameplayAbility::CheckTarget(const AActor& /*Caster*/, const FVeyraContentId& /*Ability*/, const FVeyraCastTarget& /*Target*/) const
{
	return EVeyraCastRejection::None;
}

FVeyraContentId UVeyraGameplayAbility::GetContentId(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UVeyraAbilityLoadoutComponent* Loadout = FindBesideAbilitySystem<UVeyraAbilityLoadoutComponent>(ActorInfo);
	const FVeyraLoadoutEntry* Entry = Loadout ? Loadout->FindHandle(Handle) : nullptr;
	return Entry ? Entry->Ability : FVeyraContentId();
}

bool UVeyraGameplayAbility::CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* /*OptionalRelevantTags*/) const
{
	const UVeyraCooldownComponent* Cooldowns = FindBesideAbilitySystem<UVeyraCooldownComponent>(ActorInfo);
	return Cooldowns && Cooldowns->GetRemainingSecondsNow(GetContentId(Handle, ActorInfo)) <= 0.0;
}

void UVeyraGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo /*ActivationInfo*/) const
{
	UVeyraCooldownComponent* Cooldowns = FindBesideAbilitySystem<UVeyraCooldownComponent>(ActorInfo);
	const FVeyraContentId Ability = GetContentId(Handle, ActorInfo);
	if (Cooldowns && Ability.IsValid())
	{
		Cooldowns->StartCooldown(Ability, GetCooldownSeconds(Ability));
	}
}

void UVeyraGameplayAbility::GetCooldownTimeRemainingAndDuration(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	float& TimeRemaining, float& CooldownDuration) const
{
	const UVeyraCooldownComponent* Cooldowns = FindBesideAbilitySystem<UVeyraCooldownComponent>(ActorInfo);
	const FVeyraContentId Ability = GetContentId(Handle, ActorInfo);
	TimeRemaining = Cooldowns ? static_cast<float>(Cooldowns->GetRemainingSecondsNow(Ability)) : 0.0f;
	CooldownDuration = Cooldowns ? static_cast<float>(Cooldowns->GetDurationSeconds(Ability)) : 0.0f;
}

bool UVeyraGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* /*OptionalRelevantTags*/) const
{
	const UAbilitySystemComponent* AbilitySystem = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	return AbilitySystem && VeyraCombat::CanAffordResource(*AbilitySystem, GetResourceCost(GetContentId(Handle, ActorInfo)));
}

void UVeyraGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo /*ActivationInfo*/) const
{
	UAbilitySystemComponent* AbilitySystem = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (AbilitySystem && !VeyraCombat::SpendResource(*AbilitySystem, GetResourceCost(GetContentId(Handle, ActorInfo))))
	{
		// CommitAbility checked the cost a moment ago, so this means the rules changed underneath it.
		UE_LOG(LogVeyraAbilities, Error, TEXT("%s committed but could not pay its cost."), *GetNameSafe(this));
	}
}
