// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Passives/VeyraCampRewardPassive.h"

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Targeting/VeyraTargeting.h"
#include "Teams/VeyraTeam.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraCombatVerbs.h"
#include "Wildlife/VeyraJungleSubsystem.h"

void UVeyraCampRewardPassive::Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId)
{
	Super::Start(Owner, InPassiveId);
	UWorld* World = GetWorld();
	if (UVeyraJungleSubsystem* Jungle = World ? World->GetSubsystem<UVeyraJungleSubsystem>() : nullptr)
	{
		CampHandle = Jungle->OnCampCleared.AddUObject(this, &UVeyraCampRewardPassive::OnCampCleared);
	}
	if (UVeyraCombatEventSubsystem* Events = World ? World->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		DeathHandle = Events->OnDeath.AddUObject(this, &UVeyraCampRewardPassive::OnDeath);
	}
}

void UVeyraCampRewardPassive::Stop()
{
	UWorld* World = GetWorld();
	if (UVeyraJungleSubsystem* Jungle = World ? World->GetSubsystem<UVeyraJungleSubsystem>() : nullptr)
	{
		Jungle->OnCampCleared.Remove(CampHandle);
	}
	if (UVeyraCombatEventSubsystem* Events = World ? World->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnDeath.Remove(DeathHandle);
	}
	CampHandle.Reset();
	DeathHandle.Reset();
	Super::Stop();
}

void UVeyraCampRewardPassive::OnCampCleared(const FVeyraCampCleared& Cleared)
{
	// Once per cleared camp he helped finish (§23), not per creature or last hit.
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	if (Owner && Cleared.Contributors.Contains(Owner))
	{
		Reward();
	}
}

void UVeyraCampRewardPassive::OnDeath(const FVeyraDeathEvent& Death)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraCampRewardTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindCampReward(PassiveId);
	const UAbilitySystemComponent* Victim = Death.Victim.Get();
	const UWorld* World = GetWorld();
	if (!Owner || !Tuning || !Victim || !World || !VeyraUnits::IsVanguard(Victim->GetOwner())
		|| VeyraTeams::TeamOf(Victim->GetOwner()) == VeyraTeams::TeamOf(Owner->GetOwner()))
	{
		return;
	}
	// A takedown: his kill, or his assist.
	const bool bTakedown = Death.CreditedKiller.Get() == Owner || Death.Assisters.Contains(Owner);
	const double Now = World->GetTimeSeconds();
	if (bTakedown && Now >= NextTakedownAt)
	{
		NextTakedownAt = Now + Tuning->TakedownCooldownSeconds;
		Reward();
	}
}

void UVeyraCampRewardPassive::Reward()
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraCampRewardTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindCampReward(PassiveId);
	if (!Owner || !Tuning || !VeyraTargeting::IsAlive(Owner->GetAvatarActor()))
	{
		return;
	}
	const double MaxHealth = Owner->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
	VeyraCombat::RestoreHealthFrom(*Owner, *Owner, MaxHealth * Tuning->HealthRatio + Tuning->HealthAmount);
	for (const FVeyraContentId& StatusId : Tuning->Statuses)
	{
		if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId))
		{
			VeyraCombat::ApplyStatus(*Owner, *Owner, Status.GetValue());
		}
	}
}
