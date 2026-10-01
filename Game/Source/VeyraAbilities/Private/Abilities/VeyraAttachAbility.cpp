// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraAttachAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "Movement/VeyraMovementComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraAbilitiesLog.h"
#include "VeyraCombatVerbs.h"

bool UVeyraAttachAbility::Defines(const FVeyraContentId& Ability) const
{
	return UVeyraAbilitiesTuningSubsystem::FindAttach(Ability) != nullptr;
}

double UVeyraAttachAbility::GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraAttachAbilityTuning* Attach = UVeyraAbilitiesTuningSubsystem::FindAttach(Ability);
	return Attach ? VeyraAbilityRules::ValueAtRank(Attach->Cast.ResourceCostByRank, Rank) : 0.0;
}

double UVeyraAttachAbility::GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraAttachAbilityTuning* Attach = UVeyraAbilitiesTuningSubsystem::FindAttach(Ability);
	return Attach ? VeyraAbilityRules::ValueAtRank(Attach->Cast.CooldownSecondsByRank, Rank) : 0.0;
}

EVeyraCastRejection UVeyraAttachAbility::CheckTarget(const AActor& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const
{
	const FVeyraAttachAbilityTuning* Attach = UVeyraAbilitiesTuningSubsystem::FindAttach(Ability);
	return Attach ? CheckEnemyUnit(Caster, Target.Actor, Attach->Cast.CastRange, Attach->TargetKinds) : EVeyraCastRejection::UnknownAbility;
}

const FVeyraCastTuning* UVeyraAttachAbility::GetCastTuning(const FVeyraContentId& Ability) const
{
	const FVeyraAttachAbilityTuning* Attach = UVeyraAbilitiesTuningSubsystem::FindAttach(Ability);
	return Attach ? &Attach->Cast : nullptr;
}

FVeyraChannelPlan UVeyraAttachAbility::Deliver(const FVeyraCast& Cast)
{
	const FVeyraAttachAbilityTuning* Tuning = UVeyraAbilitiesTuningSubsystem::FindAttach(Cast.Ability);
	UAbilitySystemComponent* Caster = Cast.Caster.Get();
	const AActor* Body = Caster ? Caster->GetAvatarActor() : nullptr;
	AActor* Target = Cast.TargetActor.Get();
	UVeyraMovementComponent* Movement = Body ? Body->FindComponentByClass<UVeyraMovementComponent>() : nullptr;
	StopWatching();
	if (!Tuning || !Caster || !Body || !Target || !Movement)
	{
		return FVeyraChannelPlan();
	}
	FGrab& Pending = Grab.Emplace();
	Pending.Caster = Caster;
	Pending.Target = Target;
	Pending.Ability = Cast.Ability;
	Pending.Source = FVeyraAbilityHitSource{ Cast.Ability, Cast.CastId };
	Pending.Effects = VeyraEffectDelivery::Prepare(*Caster, Tuning->HostEffects, Cast.Rank);
	const int32 Level = GetCasterLevel(*Caster);
	for (const FVeyraContentId& StatusId : Tuning->HostStatuses)
	{
		if (TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId, Level))
		{
			// Held while the caster holds on, and removed as it lets go.
			Status->DurationSeconds = Tuning->AttachSeconds;
			Pending.HostStatuses.Add(Status.GetValue());
		}
	}
	Watched = Movement;

	// It leaps to touch the target where it stands now; already touching, it takes hold at once.
	const double Gap = VeyraTargeting::EdgeToEdgeDistance(*Body, *Target);
	const FVector Toward = (Target->GetActorLocation() - Body->GetActorLocation()).GetSafeNormal2D();
	if (Gap <= 0.0 || Toward.IsNearlyZero())
	{
		TakeHold();
		return FVeyraChannelPlan();
	}
	LeapEndedHandle = Movement->OnDashEnded.AddUObject(this, &UVeyraAttachAbility::OnLeapEnded);
	if (!VeyraCombat::Dash(*Caster, FVeyraDash{ Toward, Gap, Tuning->LeapSpeed, EVeyraDashContact::None }))
	{
		UE_LOG(LogVeyraAbilities, Verbose, TEXT("%s could not leap for %s (cast %d)."), *GetNameSafe(Body), *Cast.Ability.ToString(), Cast.CastId);
		Miss();
	}
	return FVeyraChannelPlan();
}

void UVeyraAttachAbility::OnLeapEnded(const FVeyraDashEnd& End)
{
	if (UVeyraMovementComponent* Movement = Watched.Get())
	{
		Movement->OnDashEnded.Remove(LeapEndedHandle);
	}
	LeapEndedHandle.Reset();
	// A displacement that cut the leap short leaves it nothing to hold.
	if (End.Reason == EVeyraDashEndReason::Interrupted)
	{
		Miss();
		return;
	}
	TakeHold();
}

void UVeyraAttachAbility::TakeHold()
{
	UAbilitySystemComponent* Caster = Grab.IsSet() ? Grab->Caster.Get() : nullptr;
	AActor* Target = Grab.IsSet() ? Grab->Target.Get() : nullptr;
	const FVeyraAttachAbilityTuning* Tuning = Grab.IsSet() ? UVeyraAbilitiesTuningSubsystem::FindAttach(Grab->Ability) : nullptr;
	const AActor* Body = Caster ? Caster->GetAvatarActor() : nullptr;
	UAbilitySystemComponent* Host = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	UVeyraMovementComponent* Movement = Watched.Get();
	// What became Untargetable during the leap or the windup is passed over, as any enemy effect passes it (ADR-030 §2).
	const bool bInReach = Body && Target && Tuning && VeyraTargeting::IsAlive(Target) && VeyraTargeting::CanHitEnemy(Caster->GetOwner(), *Target)
		&& VeyraTargeting::EdgeToEdgeDistance(*Body, *Target) <= Tuning->ReachOnArrival;
	// A Spell Shield blocks the whole grab before it takes hold: no hold, hit or status (Combat Bible §19; ADR-025 §4).
	if (!bInReach || !Host || !Movement || VeyraCombat::BlockAbilityHit(*Host, *Caster) || !VeyraCombat::Attach(*Caster, *Target, Tuning->AttachSeconds))
	{
		Miss();
		return;
	}
	Grab->bHolding = true;
	AttachEndedHandle = Movement->OnAttachEnded.AddUObject(this, &UVeyraAttachAbility::OnAttachEnded);
	FVeyraEffectFrame Frame;
	Frame.Origin = Body->GetActorLocation();
	Frame.Direction = (Target->GetActorLocation() - Body->GetActorLocation()).GetSafeNormal2D();
	Frame.bOriginIsCaster = true;
	FVeyraAbilityHitSource Source = Grab->Source;
	Source.bSkipSpellShield = true;
	VeyraEffectDelivery::Apply(*Caster, *Target, Grab->Effects, Frame, Source);
	for (const FVeyraStatusSpec& Status : Grab->HostStatuses)
	{
		VeyraCombat::ApplyStatus(*Caster, *Host, Status);
	}
}

void UVeyraAttachAbility::OnAttachEnded(const FVeyraAttachEnd& End)
{
	const TOptional<FGrab> Ended = Grab;
	StopWatching();
	if (!Ended.IsSet())
	{
		return;
	}
	UAbilitySystemComponent* Caster = Ended->Caster.Get();
	if (UAbilitySystemComponent* Host = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(End.Host.Get()))
	{
		for (const FVeyraStatusSpec& Status : Ended->HostStatuses)
		{
			VeyraCombat::RemoveStatus(*Host, Status.Id);
		}
	}
	// Let go by its own recast, that recast is under way; otherwise it has nothing left to throw.
	if (Caster && End.Reason != EVeyraAttachEndReason::Released)
	{
		EndRecastWindow(*Caster, Ended->Ability);
	}
}

void UVeyraAttachAbility::Miss()
{
	const TOptional<FGrab> Missed = Grab;
	StopWatching();
	if (UAbilitySystemComponent* Caster = Missed.IsSet() ? Missed->Caster.Get() : nullptr)
	{
		EndRecastWindow(*Caster, Missed->Ability);
	}
}

void UVeyraAttachAbility::StopWatching()
{
	if (UVeyraMovementComponent* Movement = Watched.Get())
	{
		Movement->OnDashEnded.Remove(LeapEndedHandle);
		Movement->OnAttachEnded.Remove(AttachEndedHandle);
	}
	Watched.Reset();
	LeapEndedHandle.Reset();
	AttachEndedHandle.Reset();
	Grab.Reset();
}
