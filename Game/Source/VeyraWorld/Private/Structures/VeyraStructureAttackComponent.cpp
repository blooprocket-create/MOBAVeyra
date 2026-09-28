// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Structures/VeyraStructureAttackComponent.h"

#include "AbilitySystemComponent.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Delivery/VeyraProjectile.h"
#include "Engine/World.h"
#include "Shapes/VeyraShapes.h"
#include "Structures/VeyraStructure.h"
#include "Targeting/VeyraTargeting.h"
#include "TimerManager.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraCombatVerbs.h"

UVeyraStructureAttackComponent::UVeyraStructureAttackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVeyraStructureAttackComponent::StartAttacking()
{
	UWorld* World = GetWorld();
	if (!World || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	const float Cadence = static_cast<float>(UVeyraWorldTuningSubsystem::Get().TowerAttack.ThinkSeconds);
	World->GetTimerManager().SetTimer(ThinkTimer, FTimerDelegate::CreateUObject(this, &UVeyraStructureAttackComponent::Think), Cadence, /*bLoop*/ true);
}

void UVeyraStructureAttackComponent::StopAttacking()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ThinkTimer);
	}
	Target.Reset();
	bPriority = false;
	Claimant.Reset();
	RampTarget.Reset();
	RampStacks = 0;
}

void UVeyraStructureAttackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAttacking();
	Super::EndPlay(EndPlayReason);
}

void UVeyraStructureAttackComponent::NoteAggression(AActor& Attacker)
{
	Claimant = &Attacker;
}

void UVeyraStructureAttackComponent::Think()
{
	const AVeyraStructure* Structure = GetStructure();
	if (!Structure || Structure->IsDestroyed())
	{
		StopAttacking();
		return;
	}
	const TArray<FVeyraTowerCandidate> Candidates = GatherCandidates();
	const FVeyraTowerChoice Choice = VeyraTowerRules::Choose(Target.Get(), bPriority, Claimant.Get(), Candidates);
	Claimant.Reset();
	// Losing or switching the target resets the ramp; nothing brings old stacks back (§33).
	if (Choice.Target != Target.Get())
	{
		RampTarget.Reset();
		RampStacks = 0;
	}
	Target = const_cast<AActor*>(Choice.Target);
	bPriority = Choice.bPriority;

	AActor* Shot = Target.Get();
	if (!Shot || Now() < NextShotAt)
	{
		return;
	}
	const FVeyraTowerCandidate* Candidate = Candidates.FindByPredicate([Shot](const FVeyraTowerCandidate& Each) { return Each.Unit == Shot; });
	Fire(*Shot, Candidate && Candidate->bVanguard);
}

bool UVeyraStructureAttackComponent::IsInRange(const AActor& Unit) const
{
	const AActor* Owner = GetOwner();
	return Owner && VeyraTargeting::EdgeToEdgeDistance(*Owner, Unit) <= UVeyraWorldTuningSubsystem::Get().TowerAttack.Range;
}

TArray<FVeyraTowerCandidate> UVeyraStructureAttackComponent::GatherCandidates() const
{
	TArray<FVeyraTowerCandidate> Candidates;
	const AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return Candidates;
	}
	// A circle this wide around the tower's centre touches every body within range of its edge.
	FVeyraShape Reach;
	Reach.Kind = EVeyraShapeKind::Circle;
	Reach.Radius = UVeyraWorldTuningSubsystem::Get().TowerAttack.Range + Owner->GetSimpleCollisionRadius();
	const TArray<AActor*> Units = VeyraShapes::GatherUnits(*World, FVeyraPlacedShape{ Reach, Owner->GetActorLocation(), Owner->GetActorForwardVector() },
		[Owner](const AActor& Unit) { return VeyraTargeting::AreHostile(Owner, &Unit) && VeyraTargeting::CanAcquire(Owner, Unit); });
	for (const AActor* Unit : Units)
	{
		Candidates.Add({ Unit, VeyraUnits::IsVanguard(Unit), VeyraTargeting::EdgeToEdgeDistance(*Owner, *Unit), Unit->GetUniqueID() });
	}
	return Candidates;
}

void UVeyraStructureAttackComponent::Fire(AActor& Shot, bool bVanguard)
{
	AVeyraStructure* Structure = GetStructure();
	UAbilitySystemComponent* Tower = Structure ? Structure->GetAbilitySystemComponent() : nullptr;
	UWorld* World = GetWorld();
	if (!Tower || !World)
	{
		return;
	}
	const FVeyraWorldTuning& Tuning = UVeyraWorldTuningSubsystem::Get();
	RampStacks = VeyraTowerRules::NextRampStacks(RampTarget.Get(), RampStacks, &Shot, bVanguard, Tuning.TowerRamp.MaxStacks);
	RampTarget = bVanguard ? &Shot : nullptr;
	NextShotAt = Now() + Tuning.TowerAttack.IntervalSeconds;

	// A structure attack, not a basic attack: no crit and no on-hit (Combat Bible §55).
	FVeyraRawDamageEvent Damage;
	Damage.Components.Add({ Tuning.TowerAttack.DamageType, Tuning.TowerAttack.Damage * VeyraTowerRules::RampMultiplier(RampStacks, Tuning.TowerRamp.PerShot) });
	Damage.Delivery = EVeyraDamageDelivery::StructureAttack;
	FVeyraPreparedEffects Effects;
	Effects.Damage = VeyraCombat::PrepareDamage(*Tower, Damage);
	if (!Effects.Damage.IsValid())
	{
		return;
	}
	// A committed shot follows its target out of range and through projectile blockers (§55).
	if (AVeyraProjectile* Projectile = World->SpawnActor<AVeyraProjectile>(AVeyraProjectile::StaticClass(), FTransform(Structure->GetActorLocation())))
	{
		Projectile->LaunchHoming(*Tower, Shot, Tuning.TowerAttack.ProjectileSpeed, Tuning.TowerAttack.ProjectileRadius, MoveTemp(Effects),
			FVeyraContentId(), /*CastId*/ 0);
	}
}

AVeyraStructure* UVeyraStructureAttackComponent::GetStructure() const
{
	return Cast<AVeyraStructure>(GetOwner());
}

double UVeyraStructureAttackComponent::Now() const
{
	// World time, which a pause holds (ADR-006 §8).
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}
