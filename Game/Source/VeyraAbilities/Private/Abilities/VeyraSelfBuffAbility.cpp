// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraSelfBuffAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Engine/World.h"
#include "Shapes/VeyraShapes.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Teams/VeyraTeam.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraCombatVerbs.h"

bool UVeyraSelfBuffAbility::Defines(const FVeyraContentId& Ability) const
{
	return UVeyraAbilitiesTuningSubsystem::FindSelfBuff(Ability) != nullptr;
}

double UVeyraSelfBuffAbility::GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraSelfBuffAbilityTuning* Buff = UVeyraAbilitiesTuningSubsystem::FindSelfBuff(Ability);
	return Buff ? VeyraAbilityRules::ValueAtRank(Buff->Cast.ResourceCostByRank, Rank) : 0.0;
}

double UVeyraSelfBuffAbility::GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraSelfBuffAbilityTuning* Buff = UVeyraAbilitiesTuningSubsystem::FindSelfBuff(Ability);
	return Buff ? VeyraAbilityRules::ValueAtRank(Buff->Cast.CooldownSecondsByRank, Rank) : 0.0;
}

const FVeyraCastTuning* UVeyraSelfBuffAbility::GetCastTuning(const FVeyraContentId& Ability) const
{
	const FVeyraSelfBuffAbilityTuning* Buff = UVeyraAbilitiesTuningSubsystem::FindSelfBuff(Ability);
	return Buff ? &Buff->Cast : nullptr;
}

bool UVeyraSelfBuffAbility::EndsEarlyOnRecast(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability) const
{
	const FVeyraSelfBuffAbilityTuning* Buff = UVeyraAbilitiesTuningSubsystem::FindSelfBuff(Ability);
	if (!Buff || Buff->Recast != EVeyraRecast::EndsEarly)
	{
		return false;
	}
	if (IsAuraRunning() && AuraCaster.Get() == &Caster)
	{
		return true;
	}
	const AActor* Owner = Caster.GetOwner();
	const UVeyraStatusComponent* Statuses = Owner ? Owner->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
	return Statuses && Statuses->GetLedger().Entries.ContainsByPredicate([Buff](const FVeyraStatusEntry& Entry) { return Buff->Statuses.Contains(Entry.Id); });
}

void UVeyraSelfBuffAbility::EndEarly(UAbilitySystemComponent& Caster, const FVeyraContentId& Ability)
{
	if (const FVeyraSelfBuffAbilityTuning* Buff = UVeyraAbilitiesTuningSubsystem::FindSelfBuff(Ability))
	{
		for (const FVeyraContentId& Status : Buff->Statuses)
		{
			VeyraCombat::RemoveStatus(Caster, Status);
		}
	}
	StopAura();
}

FVeyraChannelPlan UVeyraSelfBuffAbility::Deliver(const FVeyraCast& Cast)
{
	const FVeyraSelfBuffAbilityTuning* Buff = UVeyraAbilitiesTuningSubsystem::FindSelfBuff(Cast.Ability);
	UAbilitySystemComponent* Caster = Cast.Caster.Get();
	UWorld* World = GetWorld();
	if (!Buff || !Caster || !World)
	{
		return FVeyraChannelPlan();
	}
	for (const FVeyraContentId& StatusId : Buff->Statuses)
	{
		if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId))
		{
			VeyraCombat::ApplyStatus(*Caster, *Caster, Status.GetValue());
		}
	}
	for (const FVeyraShieldTuning& Shield : Buff->Shields)
	{
		// §51: the amount is built from the rank and the caster's stats, then the shield is created.
		VeyraCombat::GrantShield(*Caster, *Caster, VeyraEffectDelivery::ShieldGrant(*Caster, Shield, Cast.Rank));
	}
	for (const FVeyraAuraTuning& Aura : Buff->Aura)
	{
		AuraCaster = Caster;
		AuraAbility = Cast.Ability;
		AuraEndsAt = World->GetTimeSeconds() + Aura.DurationSeconds;
		World->GetTimerManager().SetTimer(AuraTimer, FTimerDelegate::CreateUObject(this, &UVeyraSelfBuffAbility::RefreshAura),
			static_cast<float>(Aura.RefreshSeconds), /*bLoop*/ true);
		RefreshAura();
	}
	return FVeyraChannelPlan();
}

void UVeyraSelfBuffAbility::RefreshAura()
{
	UWorld* World = GetWorld();
	UAbilitySystemComponent* Caster = AuraCaster.Get();
	const AActor* Body = Caster ? Caster->GetAvatarActor() : nullptr;
	const FVeyraSelfBuffAbilityTuning* Buff = UVeyraAbilitiesTuningSubsystem::FindSelfBuff(AuraAbility);
	// The aura ends with its time, or with its caster.
	if (!World || !Body || !Buff || Buff->Aura.IsEmpty() || World->GetTimeSeconds() >= AuraEndsAt || !VeyraTargeting::IsAlive(Body))
	{
		StopAura();
		return;
	}
	const FVeyraAuraTuning& Aura = Buff->Aura[0];
	FVeyraShape Circle;
	Circle.Kind = EVeyraShapeKind::Circle;
	Circle.Radius = Aura.Radius;
	const EVeyraTeam Side = VeyraTeams::TeamOf(Caster->GetOwner());
	const TArray<AActor*> Allies = VeyraShapes::GatherUnits(*World, FVeyraPlacedShape{ Circle, Body->GetActorLocation(), Body->GetActorForwardVector() },
		[Body, Side](const AActor& Unit) { return &Unit != Body && Side != EVeyraTeam::None && VeyraTeams::TeamOf(&Unit) == Side && VeyraUnits::IsVanguard(&Unit); });
	for (AActor* Ally : Allies)
	{
		UAbilitySystemComponent* Target = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Ally);
		for (const FVeyraContentId& StatusId : Aura.AllyStatuses)
		{
			const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId);
			if (Target && Status.IsSet())
			{
				VeyraCombat::ApplyStatus(*Caster, *Target, Status.GetValue());
			}
		}
	}
}

void UVeyraSelfBuffAbility::StopAura()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AuraTimer);
	}
	AuraCaster = nullptr;
	AuraAbility = FVeyraContentId();
}

bool UVeyraSelfBuffAbility::IsAuraRunning() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(AuraTimer);
}
