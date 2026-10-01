// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Passives/VeyraUnreturnedPassive.h"

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "CombatState/VeyraCombatStateComponent.h"
#include "Engine/World.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraCombatVerbs.h"

void UVeyraUnreturnedPassive::Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId)
{
	Super::Start(Owner, InPassiveId);
	UWorld* World = GetWorld();
	const FVeyraUnreturnedTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindUnreturned(PassiveId);
	if (World && Tuning)
	{
		// World time, so a pause holds it.
		World->GetTimerManager().SetTimer(CheckTimer, FTimerDelegate::CreateUObject(this, &UVeyraUnreturnedPassive::Check), static_cast<float>(Tuning->CheckSeconds),
			/*bLoop*/ true);
	}
}

void UVeyraUnreturnedPassive::Stop()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CheckTimer);
	}
	Super::Stop();
}

void UVeyraUnreturnedPassive::Check()
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraUnreturnedTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindUnreturned(PassiveId);
	const AActor* Body = Owner ? Owner->GetAvatarActor() : nullptr;
	if (!Owner || !Tuning || !Body || !VeyraTargeting::IsAlive(Body))
	{
		return;
	}
	const double MaxHealth = Owner->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
	const double Health = Owner->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
	if (!(MaxHealth > 0.0))
	{
		return;
	}
	// Out of Vanguard combat, his core rebuilds: a share of what he lacks, each second (§9).
	const UVeyraCombatStateComponent* CombatState = Owner->GetOwner()->FindComponentByClass<UVeyraCombatStateComponent>();
	const double Missing = MaxHealth - Health;
	if (CombatState && !CombatState->IsInCombat() && Missing > 0.0)
	{
		VeyraCombat::RestoreHealthFrom(*Owner, *Owner, Missing * Tuning->RestoreFractionPerSecond * Tuning->CheckSeconds);
	}
	// Below each threshold, the exposed core's statuses, given again at each check so they lapse soon after.
	const UVeyraProgressionComponent* Progression = Owner->GetOwner()->FindComponentByClass<UVeyraProgressionComponent>();
	const int32 Level = Progression && Progression->IsInitialized() ? Progression->GetLevel() : 1;
	for (const FVeyraUnreturnedThresholdTuning& Threshold : Tuning->Thresholds)
	{
		if (Health / MaxHealth >= Threshold.HealthFraction)
		{
			continue;
		}
		for (const FVeyraContentId& StatusId : Threshold.Statuses)
		{
			if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId, Level))
			{
				VeyraCombat::ApplyStatus(*Owner, *Owner, Status.GetValue());
			}
		}
	}
}
