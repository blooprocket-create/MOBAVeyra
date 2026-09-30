// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Passives/VeyraQuarryPassive.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraAbilitiesTuning.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraAbilitiesVerbs.h"
#include "VeyraCombatVerbs.h"

void UVeyraQuarryPassive::Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId)
{
	Super::Start(Owner, InPassiveId);
	AActor* Participant = Owner.GetOwner();
	if (UVeyraBasicAttackComponent* Component = Participant ? Participant->FindComponentByClass<UVeyraBasicAttackComponent>() : nullptr)
	{
		Attacks = Component;
		ModifyHandle = Component->OnModifyAttack.AddUObject(this, &UVeyraQuarryPassive::OnModifyAttack);
		LandingHandle = Component->OnLanding.AddUObject(this, &UVeyraQuarryPassive::OnAttackLands);
	}
	UWorld* World = GetWorld();
	if (UVeyraCombatEventSubsystem* Events = World ? World->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		StatusHandle = Events->OnStatusApplied.AddUObject(this, &UVeyraQuarryPassive::OnStatusApplied);
		DeathHandle = Events->OnDeath.AddUObject(this, &UVeyraQuarryPassive::OnDeath);
	}
	const FVeyraQuarryTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindQuarry(PassiveId);
	if (World && Tuning)
	{
		World->GetTimerManager().SetTimer(SampleTimer, this, &UVeyraQuarryPassive::Sample, static_cast<float>(Tuning->SampleSeconds), /*bLoop*/ true);
	}
}

void UVeyraQuarryPassive::Stop()
{
	if (UVeyraBasicAttackComponent* Component = Attacks.Get())
	{
		Component->OnModifyAttack.Remove(ModifyHandle);
		Component->OnLanding.Remove(LandingHandle);
	}
	UWorld* World = GetWorld();
	if (UVeyraCombatEventSubsystem* Events = World ? World->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnStatusApplied.Remove(StatusHandle);
		Events->OnDeath.Remove(DeathHandle);
	}
	if (World)
	{
		World->GetTimerManager().ClearTimer(SampleTimer);
	}
	Attacks.Reset();
	Quarry.Reset();
	Super::Stop();
}

AActor* UVeyraQuarryPassive::GetQuarry() const
{
	const UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraQuarryTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindQuarry(PassiveId);
	UAbilitySystemComponent* Held = Quarry.Get();
	AActor* Body = Held ? Held->GetAvatarActor() : nullptr;
	// Its mark may have run out since it was placed.
	return Owner && Tuning && Body && VeyraCombat::HasStatusFrom(Body, Tuning->Mark, *Owner) ? Body : nullptr;
}

int32 UVeyraQuarryPassive::OwnerLevel() const
{
	const UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const UVeyraProgressionComponent* Progression = Owner && Owner->GetOwner() ? Owner->GetOwner()->FindComponentByClass<UVeyraProgressionComponent>() : nullptr;
	return Progression && Progression->IsInitialized() ? Progression->GetLevel() : 1;
}

void UVeyraQuarryPassive::OnStatusApplied(const FVeyraStatusApplied& Event)
{
	const FVeyraQuarryTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindQuarry(PassiveId);
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	UAbilitySystemComponent* Marked = Event.Target.Get();
	if (!Tuning || !Owner || !Marked || Event.Source.Get() != Owner || Event.Id != Tuning->Mark)
	{
		return;
	}
	// One at a time: marking another takes it off the last.
	if (UAbilitySystemComponent* Last = Quarry.Get(); Last && Last != Marked)
	{
		VeyraCombat::RemoveStatus(*Last, Tuning->Mark);
	}
	Quarry = Marked;
}

void UVeyraQuarryPassive::Sample()
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraQuarryTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindQuarry(PassiveId);
	const AActor* Body = Owner ? Owner->GetAvatarActor() : nullptr;
	if (!Owner || !Tuning || !Body)
	{
		return;
	}
	// Closing on its quarry: within reach, and moving within the angle toward it.
	bool bClosing = false;
	if (const AActor* Target = GetQuarry(); Target && VeyraTargeting::IsAlive(Body))
	{
		const FVector Toward = (Target->GetActorLocation() - Body->GetActorLocation()) * FVector(1.0, 1.0, 0.0);
		const FVector Moving = Body->GetVelocity() * FVector(1.0, 1.0, 0.0);
		const double MinCos = FMath::Cos(FMath::DegreesToRadians(Tuning->ChaseAngleDegrees));
		bClosing = Toward.Size() <= Tuning->ChaseRange && !Moving.IsNearlyZero() && !Toward.IsNearlyZero()
			&& FVector::DotProduct(Moving.GetSafeNormal(), Toward.GetSafeNormal()) >= MinCos;
	}
	if (bClosing)
	{
		if (const TOptional<FVeyraStatusSpec> Chase = UVeyraAbilitiesTuningSubsystem::FindStatus(Tuning->ChaseStatus, OwnerLevel()))
		{
			VeyraCombat::ApplyStatus(*Owner, *Owner, Chase.GetValue());
		}
	}
	else
	{
		VeyraCombat::RemoveStatus(*Owner, Tuning->ChaseStatus);
	}
}

void UVeyraQuarryPassive::OnModifyAttack(FVeyraAttackPlan& Plan)
{
	const UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraQuarryTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindQuarry(PassiveId);
	// An attack on It carries the mark's bonus magic damage; the mark goes as it lands.
	if (Owner && Tuning && VeyraCombat::HasStatusFrom(Plan.Target.Get(), Tuning->Mark, *Owner))
	{
		const double Bonus = VeyraAbilityRules::AtLevel(Tuning->DamageAmount, Tuning->DamagePerLevel, OwnerLevel())
			+ Owner->GetNumericAttribute(UVeyraOffenceSet::GetMagicPowerAttribute()) * Tuning->MagicPowerRatio;
		Plan.AddDamage(EVeyraDamageType::Magic, Bonus);
	}
}

void UVeyraQuarryPassive::OnAttackLands(const FVeyraAttackEvent& Event)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraQuarryTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindQuarry(PassiveId);
	AActor* Target = Event.Target.Get();
	UAbilitySystemComponent* Held = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!Owner || !Tuning || !Held || !VeyraCombat::HasStatusFrom(Target, Tuning->Mark, *Owner))
	{
		return;
	}
	// Spent: the mark goes, and her basic abilities come back sooner.
	VeyraCombat::RemoveStatus(*Held, Tuning->Mark);
	Quarry.Reset();
	const EVeyraAbilitySlot Basic[] = { EVeyraAbilitySlot::Q, EVeyraAbilitySlot::W, EVeyraAbilitySlot::E };
	VeyraAbilities::RefundCooldowns(*Owner, Basic, Tuning->CooldownRefund);
}

void UVeyraQuarryPassive::OnDeath(const FVeyraDeathEvent& Death)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraQuarryTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindQuarry(PassiveId);
	UAbilitySystemComponent* Fallen = Death.Victim.Get();
	UWorld* World = GetWorld();
	if (!Owner || !Tuning || !World || !Fallen || Fallen != Quarry.Get())
	{
		return;
	}
	Quarry.Reset();
	const AActor* Where = Fallen->GetAvatarActor();
	const AActor* Body = Owner->GetAvatarActor();
	if (Death.CreditedKiller.Get() != Owner || !Where || !Body)
	{
		return;
	}
	// The game goes on: It jumps to the nearest enemy Vanguard in reach.
	AActor* Next = nullptr;
	double Nearest = FMath::Square(Tuning->JumpRadius);
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Candidate = *It;
		if (Candidate == Where || !VeyraUnits::IsVanguard(Candidate) || !VeyraTargeting::IsAlive(Candidate) || !VeyraTargeting::AreHostile(Body, Candidate))
		{
			continue;
		}
		const double Apart = FVector::DistSquared2D(Candidate->GetActorLocation(), Where->GetActorLocation());
		if (Apart <= Nearest)
		{
			Nearest = Apart;
			Next = Candidate;
		}
	}
	UAbilitySystemComponent* NextAbilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Next);
	const TOptional<FVeyraStatusSpec> Mark = UVeyraAbilitiesTuningSubsystem::FindStatus(Tuning->Mark, OwnerLevel());
	if (NextAbilities && Mark.IsSet())
	{
		VeyraCombat::ApplyStatus(*Owner, *NextAbilities, Mark.GetValue());
	}
}
