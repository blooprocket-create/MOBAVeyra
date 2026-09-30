// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Passives/VeyraReclaimPassive.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Engine/World.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Tuning/VeyraAbilitiesTuning.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraCombatVerbs.h"

void UVeyraReclaimPassive::Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId)
{
	Super::Start(Owner, InPassiveId);
	AActor* Participant = Owner.GetOwner();
	if (UVeyraBasicAttackComponent* Component = Participant ? Participant->FindComponentByClass<UVeyraBasicAttackComponent>() : nullptr)
	{
		Attacks = Component;
		HitHandle = Component->OnHit.AddUObject(this, &UVeyraReclaimPassive::OnHit);
	}
}

void UVeyraReclaimPassive::Stop()
{
	if (UVeyraBasicAttackComponent* Component = Attacks.Get())
	{
		Component->OnHit.Remove(HitHandle);
	}
	Attacks.Reset();
	HitHandle.Reset();
	ReclaimedAt.Reset();
	Super::Stop();
}

void UVeyraReclaimPassive::OnHit(const FVeyraAttackEvent& Event)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraReclaimTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindReclaim(PassiveId);
	AActor* Target = Event.Target.Get();
	UAbilitySystemComponent* Held = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	const UVeyraStatusComponent* Statuses = Held && Held->GetOwner() ? Held->GetOwner()->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
	const UWorld* World = GetWorld();
	// Only a landed attack on an enemy Vanguard that holds his mark (§3).
	if (!Owner || !Tuning || !World || !VeyraUnits::IsVanguard(Target) || !Statuses || !Statuses->HasFrom(Tuning->Mark, *Owner))
	{
		return;
	}
	// Once per lockout per target: until then the mark stays for a later attack.
	const double Now = World->GetTimeSeconds();
	if (const double* Last = ReclaimedAt.Find(Target); Last && Now - *Last < Tuning->LockoutSeconds)
	{
		return;
	}
	ReclaimedAt.Add(Target, Now);
	VeyraCombat::RemoveStatus(*Held, Tuning->Mark);
	const UVeyraProgressionComponent* Progression = Owner->GetOwner()->FindComponentByClass<UVeyraProgressionComponent>();
	const int32 Level = Progression && Progression->IsInitialized() ? Progression->GetLevel() : 1;
	const double Heal = VeyraAbilityRules::AtLevel(Tuning->HealAmount, Tuning->HealPerLevel, Level)
		+ Owner->GetNumericAttribute(UVeyraOffenceSet::GetMagicPowerAttribute()) * Tuning->MagicPowerRatio;
	VeyraCombat::RestoreHealthFrom(*Owner, *Owner, Heal);
}
