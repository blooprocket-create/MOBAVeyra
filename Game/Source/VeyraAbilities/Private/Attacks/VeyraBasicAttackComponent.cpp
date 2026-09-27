// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraBasicAttackComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Casting/VeyraCastStateComponent.h"
#include "CombatState/VeyraCombatStateComponent.h"
#include "Delivery/VeyraProjectile.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Movement/VeyraMovementComponent.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "TimerManager.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraAbilitiesLog.h"

namespace
{
	/** Damage scaled by Fraction, keeping its penetration: a cleave's share of the attack. */
	FVeyraRawDamageEvent ScaledDamage(const FVeyraRawDamageEvent& Damage, double Fraction)
	{
		FVeyraRawDamageEvent Scaled = Damage;
		for (FVeyraDamageComponent& Component : Scaled.Components)
		{
			Component.Amount *= Fraction;
		}
		return Scaled;
	}

	/** On the ground, from the attacker toward the target; the attacker's facing when they coincide. */
	FVector AttackDirection(const FVector& From, const AActor& Target, const FVector& Facing)
	{
		const FVector Direction = (Target.GetActorLocation() - From).GetSafeNormal2D();
		return Direction.IsNearlyZero() ? Facing.GetSafeNormal2D() : Direction;
	}
}

UVeyraBasicAttackComponent::UVeyraBasicAttackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	// It follows its owner's combat state, statuses and death from initialization, before play begins.
	bWantsInitializeComponent = true;
}

void UVeyraBasicAttackComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Every machine sees every unit's attacks until Vision gates them (ADR-009 §7).
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraBasicAttackComponent, State, Params);
}

void UVeyraBasicAttackComponent::InitializeComponent()
{
	Super::InitializeComponent();
	// On every machine: the events these follow are raised on the server, and a client's copy of the
	// chain is never read.
	const AActor* Owner = GetOwner();
	if (UVeyraCombatStateComponent* CombatState = Owner ? Owner->FindComponentByClass<UVeyraCombatStateComponent>() : nullptr)
	{
		CombatStateHandle = CombatState->OnCombatStateChanged.AddUObject(this, &UVeyraBasicAttackComponent::OnCombatStateChanged);
	}
	if (UVeyraStatusComponent* Statuses = Owner ? Owner->FindComponentByClass<UVeyraStatusComponent>() : nullptr)
	{
		InterruptedHandle = Statuses->OnInterrupted.AddUObject(this, &UVeyraBasicAttackComponent::CancelAttack);
	}
	UWorld* World = GetWorld();
	if (UVeyraCombatEventSubsystem* Events = World ? World->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		DeathHandle = Events->OnDeath.AddUObject(this, &UVeyraBasicAttackComponent::OnDeath);
	}
}

void UVeyraBasicAttackComponent::UninitializeComponent()
{
	UWorld* World = GetWorld();
	if (UVeyraCombatEventSubsystem* Events = World ? World->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnDeath.Remove(DeathHandle);
	}
	const AActor* Owner = GetOwner();
	if (UVeyraCombatStateComponent* CombatState = Owner ? Owner->FindComponentByClass<UVeyraCombatStateComponent>() : nullptr)
	{
		CombatState->OnCombatStateChanged.Remove(CombatStateHandle);
	}
	if (UVeyraStatusComponent* Statuses = Owner ? Owner->FindComponentByClass<UVeyraStatusComponent>() : nullptr)
	{
		Statuses->OnInterrupted.Remove(InterruptedHandle);
	}
	Super::UninitializeComponent();
}

void UVeyraBasicAttackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhaseTimer);
	}
	Super::EndPlay(EndPlayReason);
}

bool UVeyraBasicAttackComponent::SetProfile(const FVeyraBasicAttackProfile& InProfile)
{
	const TArray<FString> Problems = VeyraBasicAttacks::Validate(InProfile);
	if (!Problems.IsEmpty())
	{
		UE_LOG(LogVeyraAbilities, Error, TEXT("Refused a basic attack profile for %s: %s."), *GetNameSafe(GetOwner()), *FString::Join(Problems, TEXT("; ")));
		return false;
	}
	Profile = InProfile;
	bHasProfile = true;
	return true;
}

EVeyraAttackRejection UVeyraBasicAttackComponent::CheckAttack(const AActor* Target) const
{
	if (!bHasProfile)
	{
		return EVeyraAttackRejection::NoProfile;
	}
	const UAbilitySystemComponent* Attacker = GetAbilitySystem();
	const APawn* Body = Attacker ? Cast<APawn>(Attacker->GetAvatarActor()) : nullptr;
	if (!Body || !VeyraTargeting::IsAlive(Body))
	{
		return EVeyraAttackRejection::AttackerDead;
	}
	if (EnumHasAnyFlags(VeyraCombat::GetActionBlocks(*Attacker), EVeyraActionBlocks::Attack))
	{
		return EVeyraAttackRejection::CrowdControlled;
	}
	const UVeyraCastStateComponent* CastState = GetOwner()->FindComponentByClass<UVeyraCastStateComponent>();
	const UVeyraMovementComponent* Movement = Body->FindComponentByClass<UVeyraMovementComponent>();
	const bool bForcedMove = Movement && (Movement->IsDisplaced() || Movement->IsDashing());
	if (State.Phase == EVeyraAttackPhase::Windup || (CastState && CastState->IsBusy()) || bForcedMove)
	{
		return EVeyraAttackRejection::Busy;
	}
	// The target before the interval, so an order chases a target out of range while the interval runs.
	switch (VeyraTargeting::CheckEnemyTarget(*Body, Target, Profile.Range))
	{
	case EVeyraTargetValidity::Valid:
		break;
	case EVeyraTargetValidity::OutOfRange:
		return EVeyraAttackRejection::OutOfRange;
	case EVeyraTargetValidity::NotACombatant:
	case EVeyraTargetValidity::Caster:
	case EVeyraTargetValidity::Dead:
	case EVeyraTargetValidity::NotHostile:
		return EVeyraAttackRejection::InvalidTarget;
	}
	return GetServerNow() < NextAttackAt ? EVeyraAttackRejection::OnCooldown : EVeyraAttackRejection::None;
}

EVeyraAttackRejection UVeyraBasicAttackComponent::StartAttack(AActor& Target)
{
	const EVeyraAttackRejection Rejection = CheckAttack(&Target);
	if (Rejection != EVeyraAttackRejection::None)
	{
		return Rejection;
	}
	// A backswing still running ends as the next attack begins.
	GetWorld()->GetTimerManager().ClearTimer(PhaseTimer);

	const double Now = GetServerNow();
	FRunningAttack& Attack = Running.Emplace();
	Attack.Target = &Target;
	Attack.StartedAt = Now;
	Attack.Timing = GetTiming();
	const double WindupSeconds = Attack.Timing.IntervalSeconds * Profile.WindupFraction;

	// The attacker turns to face its target.
	if (AActor* Body = GetAbilitySystem()->GetAvatarActor())
	{
		const FVector Facing = AttackDirection(Body->GetActorLocation(), Target, Body->GetActorForwardVector());
		Body->SetActorRotation(FRotator(0.0, Facing.Rotation().Yaw, 0.0));
	}
	EnterPhase(EVeyraAttackPhase::Windup, &Target, Now + WindupSeconds);
	GetWorld()->GetTimerManager().SetTimer(PhaseTimer, FTimerDelegate::CreateUObject(this, &UVeyraBasicAttackComponent::Commit),
		static_cast<float>(WindupSeconds), /*bLoop*/ false);
	return EVeyraAttackRejection::None;
}

void UVeyraBasicAttackComponent::CancelAttack()
{
	if (!Running.IsSet())
	{
		return;
	}
	// Before Commit nothing was spent; after it, NextAttackAt keeps the attack's interval (§48).
	EndAttack();
}

void UVeyraBasicAttackComponent::Commit()
{
	if (!Running.IsSet() || State.Phase != EVeyraAttackPhase::Windup)
	{
		return;
	}
	GetWorld()->GetTimerManager().ClearTimer(PhaseTimer);
	UAbilitySystemComponent* Attacker = GetAbilitySystem();
	const AActor* Body = Attacker ? Attacker->GetAvatarActor() : nullptr;
	AActor* Target = Running->Target.Get();

	// The target must still be valid and in range, and the attacker free to attack (Combat Bible §4).
	const bool bBlocked = Attacker && EnumHasAnyFlags(VeyraCombat::GetActionBlocks(*Attacker), EVeyraActionBlocks::Attack);
	if (!Body || !Target || bBlocked || VeyraTargeting::CheckEnemyTarget(*Body, Target, Profile.Range) != EVeyraTargetValidity::Valid)
	{
		EndAttack();
		return;
	}

	NextAttackAt = Running->StartedAt + Running->Timing.IntervalSeconds;
	const FVeyraAttackPlan Plan = BuildPlan(*Attacker, *Target, Running->Timing);
	const FLandingAttack Landing = Prepare(*Attacker, *Body, Plan);
	OnAttack.Broadcast(Landing.Event);

	if (Profile.Projectile.IsEmpty())
	{
		Land(Landing);
	}
	else if (AVeyraProjectile* Projectile = GetWorld()->SpawnActor<AVeyraProjectile>(AVeyraProjectile::StaticClass(), FTransform(Body->GetActorLocation())))
	{
		// Once launched it cannot be escaped by leaving range (§4); it lands even if the attacker has died.
		const FVeyraAttackProjectileTuning& Flight = Profile.Projectile[0];
		Projectile->LaunchHoming(*Attacker, *Target, Flight.Speed, Flight.Radius, FVeyraPreparedEffects(), FVeyraContentId(), 0,
			[WeakThis = TWeakObjectPtr<UVeyraBasicAttackComponent>(this), Landing](AActor&) {
				if (UVeyraBasicAttackComponent* Attacks = WeakThis.Get())
				{
					Attacks->Land(Landing);
				}
			});
	}

	const double Now = GetServerNow();
	EnterPhase(EVeyraAttackPhase::Backswing, Target, NextAttackAt);
	if (NextAttackAt > Now)
	{
		GetWorld()->GetTimerManager().SetTimer(PhaseTimer, FTimerDelegate::CreateUObject(this, &UVeyraBasicAttackComponent::EndBackswing),
			static_cast<float>(NextAttackAt - Now), /*bLoop*/ false);
	}
	else
	{
		EndBackswing();
	}
}

FVeyraAttackPlan UVeyraBasicAttackComponent::BuildPlan(UAbilitySystemComponent& Attacker, AActor& Target, const FVeyraAttackTiming& Timing)
{
	FVeyraAttackPlan Plan;
	Plan.Target = &Target;

	// The hit chain counts consecutive attacks on one enemy Vanguard; any other target starts it again.
	if (VeyraUnits::IsVanguard(&Target))
	{
		Chain = ChainTarget.Get() == &Target ? Chain + 1 : 1;
		ChainTarget = &Target;
	}
	else
	{
		ResetChain();
	}
	Plan.Chain = Chain;

	// Overflow amplifies the base damage only, not what empowerments and modifiers add (§22).
	const double Base = Attacker.GetNumericAttribute(UVeyraOffenceSet::GetPhysicalPowerAttribute()) * Profile.PhysicalPowerRatio
		+ Attacker.GetNumericAttribute(UVeyraOffenceSet::GetMagicPowerAttribute()) * Profile.MagicPowerRatio;
	Plan.AddDamage(Profile.DamageType, Base * Timing.OverflowDamageMultiplier);

	if (Empowerment.IsSet() && GetServerNow() <= EmpowermentExpiresAt)
	{
		Plan.bEmpowered = true;
		if (Empowerment->Apply)
		{
			Empowerment->Apply(Plan);
		}
	}
	Empowerment.Reset();

	const UVeyraStatusComponent* Statuses = GetOwner()->FindComponentByClass<UVeyraStatusComponent>();
	const double CleaveFraction = Statuses ? Statuses->GetStrongest(EVeyraStatusKind::AttackCleave) : 0.0;
	if (CleaveFraction > 0.0 && !Plan.Cleave.IsSet())
	{
		Plan.Cleave = FVeyraAttackCleave{ CleaveFraction, {} };
	}
	OnModifyAttack.Broadcast(Plan);
	return Plan;
}

UVeyraBasicAttackComponent::FLandingAttack UVeyraBasicAttackComponent::Prepare(UAbilitySystemComponent& Attacker, const AActor& Body,
	const FVeyraAttackPlan& Plan) const
{
	// Everything the attacker contributes is fixed now; each target's defences are read on landing (§50).
	FLandingAttack Landing;
	Landing.Event.Attacker = &Attacker;
	Landing.Event.Target = Plan.Target;
	Landing.Event.Chain = Plan.Chain;
	Landing.Event.bEmpowered = Plan.bEmpowered;
	Landing.AttackerLocation = Body.GetActorLocation();
	Landing.Damage = VeyraCombat::PrepareDamage(Attacker, Plan.Damage);
	Landing.TargetStatuses = Plan.TargetStatuses;
	if (Plan.Cleave.IsSet() && !Profile.Cleave.IsEmpty())
	{
		Landing.CleaveShape = Profile.Cleave[0];
		Landing.CleaveDamage = VeyraCombat::PrepareDamage(Attacker, ScaledDamage(Plan.Damage, Plan.Cleave->DamageFraction));
		Landing.CleaveStatuses = Plan.Cleave->Statuses;
	}
	if (Plan.SecondaryImpact.IsSet())
	{
		Landing.ImpactShape = Plan.SecondaryImpact->Shape;
		if (!Plan.SecondaryImpact->Damage.Components.IsEmpty())
		{
			Landing.ImpactDamage = VeyraCombat::PrepareDamage(Attacker, Plan.SecondaryImpact->Damage);
		}
		Landing.ImpactStatuses = Plan.SecondaryImpact->Statuses;
	}
	return Landing;
}

void UVeyraBasicAttackComponent::Land(const FLandingAttack& Landing)
{
	AActor* Target = Landing.Event.Target.Get();
	UAbilitySystemComponent* Attacker = Landing.Event.Attacker.Get();
	UAbilitySystemComponent* Struck = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	// A target that died before the attack landed is not hit, and nothing is refunded (Combat Bible §54).
	if (!Target || !Attacker || !Struck || !VeyraTargeting::IsAlive(Target))
	{
		return;
	}
	if (Landing.Damage.IsValid())
	{
		VeyraCombat::DealPreparedDamage(Landing.Damage, *Struck);
	}
	for (const FVeyraStatusSpec& Status : Landing.TargetStatuses)
	{
		VeyraCombat::ApplyStatus(*Attacker, *Struck, Status);
	}
	OnHit.Broadcast(Landing.Event);

	// The cleave sweeps out from the attacker, where it stands now if it still lives.
	const AActor* Body = Attacker->GetAvatarActor();
	const FVector From = Body && VeyraTargeting::IsAlive(Body) ? Body->GetActorLocation() : Landing.AttackerLocation;
	const FVector Direction = AttackDirection(From, *Target, Body ? Body->GetActorForwardVector() : FVector::ForwardVector);
	if (Landing.CleaveShape.IsSet())
	{
		HitAround(Landing.Event, FVeyraPlacedShape{ Landing.CleaveShape.GetValue(), From, Direction }, Landing.CleaveDamage, Landing.CleaveStatuses);
	}
	if (Landing.ImpactShape.IsSet())
	{
		HitAround(Landing.Event, FVeyraPlacedShape{ Landing.ImpactShape.GetValue(), Target->GetActorLocation(), Direction }, Landing.ImpactDamage,
			Landing.ImpactStatuses);
	}
}

void UVeyraBasicAttackComponent::HitAround(const FVeyraAttackEvent& Event, const FVeyraPlacedShape& Placed, const FVeyraPreparedDamage& Damage,
	TConstArrayView<FVeyraStatusSpec> Statuses) const
{
	UAbilitySystemComponent* Attacker = Event.Attacker.Get();
	const AActor* Target = Event.Target.Get();
	if (!Attacker)
	{
		return;
	}
	// Sides belong to the participant, which outlives its body.
	const AActor* Side = Attacker->GetOwner();
	const TArray<AActor*> Units = VeyraShapes::GatherUnits(*GetWorld(), Placed, [Side, Target](const AActor& Unit) {
		return &Unit != Target && VeyraTargeting::AreHostile(Side, &Unit);
	});
	for (AActor* Unit : Units)
	{
		UAbilitySystemComponent* Struck = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Unit);
		if (!Struck)
		{
			continue;
		}
		if (Damage.IsValid())
		{
			VeyraCombat::DealPreparedDamage(Damage, *Struck);
		}
		for (const FVeyraStatusSpec& Status : Statuses)
		{
			VeyraCombat::ApplyStatus(*Attacker, *Struck, Status);
		}
	}
}

void UVeyraBasicAttackComponent::Empower(FVeyraAttackEmpowerment InEmpowerment)
{
	EmpowermentExpiresAt = GetServerNow() + InEmpowerment.DurationSeconds;
	Empowerment = MoveTemp(InEmpowerment);
}

bool UVeyraBasicAttackComponent::IsEmpowered() const
{
	return Empowerment.IsSet() && GetServerNow() <= EmpowermentExpiresAt;
}

FVeyraAttackTiming UVeyraBasicAttackComponent::GetTiming() const
{
	const UAbilitySystemComponent* Attacker = GetAbilitySystem();
	const double AttackSpeed = Attacker ? Attacker->GetNumericAttribute(UVeyraOffenceSet::GetAttackSpeedAttribute()) : 0.0;
	return VeyraAttackSpeed::Resolve(AttackSpeed, UVeyraCombatTuningSubsystem::Get().AttackSpeed, Profile.MinimumIntervalSeconds);
}

void UVeyraBasicAttackComponent::EndBackswing()
{
	EndAttack();
}

void UVeyraBasicAttackComponent::EndAttack()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhaseTimer);
	}
	Running.Reset();
	EnterPhase(EVeyraAttackPhase::None, nullptr, 0.0);
	OnAttackEnded.Broadcast();
}

void UVeyraBasicAttackComponent::EnterPhase(EVeyraAttackPhase Phase, AActor* Target, double EndsAt)
{
	State.Phase = Phase;
	State.Target = Target;
	State.PhaseEndsAt = EndsAt;
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraBasicAttackComponent, State, this);
}

void UVeyraBasicAttackComponent::ResetChain()
{
	ChainTarget.Reset();
	Chain = 0;
}

void UVeyraBasicAttackComponent::OnCombatStateChanged(bool bInCombat)
{
	// Leaving combat drops the chain (ADR-009 §5).
	if (!bInCombat)
	{
		ResetChain();
	}
}

void UVeyraBasicAttackComponent::OnDeath(const FVeyraDeathEvent& Death)
{
	// Death ends the attack, the chain and a waiting empowerment, as it ends temporary effects (§44).
	if (Death.Victim.Get() == GetAbilitySystem())
	{
		CancelAttack();
		ResetChain();
		Empowerment.Reset();
	}
}

UAbilitySystemComponent* UVeyraBasicAttackComponent::GetAbilitySystem() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
}

double UVeyraBasicAttackComponent::GetServerNow() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}
