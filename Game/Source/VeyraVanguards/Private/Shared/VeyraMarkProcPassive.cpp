// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shared/VeyraMarkProcPassive.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Delivery/VeyraProjectile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Teams/VeyraTeam.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraCombatVerbs.h"
#include "VeyraVanguardsLog.h"

namespace VeyraMarkProc
{
	// A passive has no ranks; its one amounts apply at every level.
	constexpr int32 PassiveRank = 1;

	/** Damage with Type and a set amount, as proc damage carries once worked out. */
	FVeyraEffectBundleTuning ProcEffects(EVeyraDamageType Type, double Amount)
	{
		FVeyraEffectBundleTuning Effects;
		FVeyraDamageTuning& Damage = Effects.Damage.AddDefaulted_GetRef();
		Damage.Type = Type;
		Damage.AmountByRank = { Amount };
		return Effects;
	}
}

void UVeyraMarkProcPassive::Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId)
{
	Super::Start(Owner, InPassiveId);
	AActor* Participant = Owner.GetOwner();
	if (UVeyraBasicAttackComponent* Component = Participant ? Participant->FindComponentByClass<UVeyraBasicAttackComponent>() : nullptr)
	{
		Attacks = Component;
		ModifyHandle = Component->OnModifyAttack.AddUObject(this, &UVeyraMarkProcPassive::OnModifyAttack);
	}
	if (UVeyraCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		StatusHandle = Events->OnStatusApplied.AddUObject(this, &UVeyraMarkProcPassive::OnStatusApplied);
		CastHandle = Events->OnCastCommitted.AddUObject(this, &UVeyraMarkProcPassive::OnCastCommitted);
	}
}

void UVeyraMarkProcPassive::Stop()
{
	if (UVeyraBasicAttackComponent* Component = Attacks.Get())
	{
		Component->OnModifyAttack.Remove(ModifyHandle);
	}
	if (UVeyraCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnStatusApplied.Remove(StatusHandle);
		Events->OnCastCommitted.Remove(CastHandle);
	}
	ModifyHandle.Reset();
	StatusHandle.Reset();
	CastHandle.Reset();
	Attacks.Reset();
	Super::Stop();
}

void UVeyraMarkProcPassive::OnStatusApplied(const FVeyraStatusApplied& Event)
{
	const FVeyraMarkProcTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindMarkProc(PassiveId);
	if (Tuning && !Tuning->Emergence.IsEmpty() && Event.Target.Get() == OwnerAbilitySystem.Get() && Event.Id == Tuning->Emergence[0].Status)
	{
		// A new Camouflage: the first attack out of it will be empowered.
		CamouflagedUntil = Event.EndsAt;
		bEmerged = false;
	}
}

void UVeyraMarkProcPassive::OnCastCommitted(const FVeyraCastEvent& Event)
{
	const FVeyraMarkProcTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindMarkProc(PassiveId);
	const UWorld* World = GetWorld();
	if (Tuning && World && !Tuning->ProcBolts.IsEmpty() && Event.Caster.Get() == OwnerAbilitySystem.Get() && Event.Ability == Tuning->ProcBolts[0].Ability)
	{
		BoltsUntil = World->GetTimeSeconds() + Tuning->ProcBolts[0].WindowSeconds;
	}
}

void UVeyraMarkProcPassive::OnModifyAttack(FVeyraAttackPlan& Plan)
{
	const UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraMarkProcTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindMarkProc(PassiveId);
	AActor* Target = Plan.Target.Get();
	UAbilitySystemComponent* TargetAbilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	const UVeyraStatusComponent* Marks = TargetAbilities && TargetAbilities->GetOwner() ? TargetAbilities->GetOwner()->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
	const TOptional<FVeyraStatusSpec> Mark = Tuning ? UVeyraAbilitiesTuningSubsystem::FindStatus(Tuning->Mark) : TOptional<FVeyraStatusSpec>();
	const UWorld* World = GetWorld();
	// Only an attack on an enemy Vanguard marks or procs (§21).
	if (!Owner || !Tuning || !Mark.IsSet() || !Marks || !World || !VeyraUnits::IsVanguard(Target)
		|| VeyraTeams::TeamOf(Target) == VeyraTeams::TeamOf(Owner->GetOwner()))
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	// The first attack out of a Camouflage, while its window lasts.
	const bool bEmergence = !Tuning->Emergence.IsEmpty() && !bEmerged && Now <= CamouflagedUntil + Tuning->Emergence[0].WindowSeconds;
	if (bEmergence)
	{
		bEmerged = true;
	}
	if (Marks->GetStacksFrom(Tuning->Mark, *Owner) >= Mark->MaxStacks)
	{
		// Primed: the mark is spent for the proc, and this attack adds no stack.
		const UVeyraProgressionComponent* Progression = Owner->GetOwner()->FindComponentByClass<UVeyraProgressionComponent>();
		const int32 Level = Progression && Progression->IsInitialized() ? Progression->GetLevel() : 1;
		const double Amount = VeyraEffectDelivery::DamageAmount(*Owner, Tuning->ProcDamage, VeyraMarkProc::PassiveRank) + Tuning->ProcDamagePerLevel * (Level - 1);
		Plan.AddDamage(Tuning->ProcDamage.Type, Amount);
		VeyraCombat::RemoveStatus(*TargetAbilities, Tuning->Mark);
		if (Now <= BoltsUntil)
		{
			SendBolt(*Target);
		}
		return;
	}
	// Out of a Camouflage on an unprimed target: bonus damage, and the mark at its most (§21).
	int32 Added = 1;
	if (bEmergence)
	{
		Plan.AddDamage(Tuning->Emergence[0].BonusDamage.Type,
			VeyraEffectDelivery::DamageAmount(*Owner, Tuning->Emergence[0].BonusDamage, VeyraMarkProc::PassiveRank));
		Added = Mark->MaxStacks;
	}
	for (int32 Stack = 0; Stack < Added; ++Stack)
	{
		Plan.TargetStatuses.Add(Mark.GetValue());
	}
}

void UVeyraMarkProcPassive::SendBolt(AActor& ProcTarget)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraMarkProcTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindMarkProc(PassiveId);
	AActor* Body = Owner ? Owner->GetAvatarActor() : nullptr;
	UWorld* World = GetWorld();
	if (!Tuning || !Body || !World)
	{
		return;
	}
	const FVeyraProcBoltTuning& Bolt = Tuning->ProcBolts[0];
	// The nearest other enemy Vanguard near the proc's target that the owner could acquire; the target
	// itself only when none is. Never one it cannot see (§21).
	AActor* Chosen = nullptr;
	double Nearest = FMath::Square(Bolt.Radius);
	const EVeyraTeam Side = VeyraTeams::TeamOf(Owner->GetOwner());
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Candidate = *It;
		const double Distance = FVector::DistSquared2D(Candidate->GetActorLocation(), ProcTarget.GetActorLocation());
		if (Candidate != &ProcTarget && Distance <= Nearest && VeyraUnits::IsVanguard(Candidate) && VeyraTeams::TeamOf(Candidate) != Side
			&& VeyraTargeting::IsAlive(Candidate) && VeyraTargeting::CanAcquire(Body, *Candidate))
		{
			Chosen = Candidate;
			Nearest = Distance;
		}
	}
	AActor* Struck = Chosen ? Chosen : &ProcTarget;
	AVeyraProjectile* Shot = World->SpawnActor<AVeyraProjectile>(AVeyraProjectile::StaticClass(), FTransform(Body->GetActorLocation()));
	if (!Shot)
	{
		UE_LOG(LogVeyraVanguards, Warning, TEXT("%s could not send its bolt at %s."), *PassiveId.ToString(), *GetNameSafe(Struck));
		return;
	}
	// Proc damage: it marks nothing and sends nothing further.
	const double Amount = VeyraEffectDelivery::DamageAmount(*Owner, Bolt.Damage, VeyraMarkProc::PassiveRank);
	Shot->LaunchHoming(*Owner, *Struck, Bolt.Projectile.Speed, Bolt.Projectile.Radius,
		VeyraEffectDelivery::Prepare(*Owner, VeyraMarkProc::ProcEffects(Bolt.Damage.Type, Amount), VeyraMarkProc::PassiveRank), PassiveId, 0);
}
