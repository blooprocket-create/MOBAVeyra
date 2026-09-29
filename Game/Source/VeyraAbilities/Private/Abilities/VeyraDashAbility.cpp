// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraDashAbility.h"

#include "AbilitySystemComponent.h"
#include "Delivery/VeyraAreaDelivery.h"
#include "Engine/World.h"
#include "Movement/VeyraMovementComponent.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraAbilitiesLog.h"
#include "VeyraCombatVerbs.h"

bool UVeyraDashAbility::Defines(const FVeyraContentId& Ability) const
{
	return UVeyraAbilitiesTuningSubsystem::FindDash(Ability) != nullptr;
}

double UVeyraDashAbility::GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraDashAbilityTuning* Dash = UVeyraAbilitiesTuningSubsystem::FindDash(Ability);
	return Dash ? VeyraAbilityRules::ValueAtRank(Dash->Cast.ResourceCostByRank, Rank) : 0.0;
}

double UVeyraDashAbility::GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraDashAbilityTuning* Dash = UVeyraAbilitiesTuningSubsystem::FindDash(Ability);
	return Dash ? VeyraAbilityRules::ValueAtRank(Dash->Cast.CooldownSecondsByRank, Rank) : 0.0;
}

EVeyraCastRejection UVeyraDashAbility::CheckTarget(const AActor& /*Caster*/, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const
{
	if (!UVeyraAbilitiesTuningSubsystem::FindDash(Ability))
	{
		return EVeyraCastRejection::UnknownAbility;
	}
	// The point gives the dash its direction.
	return HasUsablePoint(Target) ? EVeyraCastRejection::None : EVeyraCastRejection::InvalidLocation;
}

const FVeyraCastTuning* UVeyraDashAbility::GetCastTuning(const FVeyraContentId& Ability) const
{
	const FVeyraDashAbilityTuning* Dash = UVeyraAbilitiesTuningSubsystem::FindDash(Ability);
	return Dash ? &Dash->Cast : nullptr;
}

FVeyraChannelPlan UVeyraDashAbility::Deliver(const FVeyraCast& Cast)
{
	const FVeyraDashAbilityTuning* Dash = UVeyraAbilitiesTuningSubsystem::FindDash(Cast.Ability);
	UAbilitySystemComponent* Caster = Cast.Caster.Get();
	const AActor* Body = Caster ? Caster->GetAvatarActor() : nullptr;
	UWorld* World = GetWorld();
	if (!Dash || !Caster || !Body || !World)
	{
		return FVeyraChannelPlan();
	}

	// The start zones hit where the caster sets off, facing the point.
	if (!Dash->StartZones.IsEmpty())
	{
		FVeyraEffectFrame Placement;
		Placement.Origin = Body->GetActorLocation();
		Placement.Direction = Cast.Direction;
		Placement.bOriginIsCaster = true;
		VeyraAreaDelivery::Resolve(*World, *Caster, Placement, VeyraAreaDelivery::PrepareZones(*Caster, Dash->StartZones, Cast.Rank),
			FVeyraAbilityHitSource{ Cast.Ability, Cast.CastId });
	}

	const FVector Heading = VeyraAbilityRules::DashHeading(*Dash, Cast.Direction);
	StopWatching();
	UVeyraMovementComponent* Movement = Body->FindComponentByClass<UVeyraMovementComponent>();
	if (Dash->Contact == EVeyraDashContact::StopAtFirstEnemy && Movement)
	{
		FPendingContact& Pending = Contact.Emplace();
		Pending.Caster = Caster;
		Pending.Source = FVeyraAbilityHitSource{ Cast.Ability, Cast.CastId };
		Pending.Direction = Heading;
		Pending.Effects = VeyraEffectDelivery::Prepare(*Caster, Dash->ContactEffects, Cast.Rank);
		for (const FVeyraContentId& StatusId : Dash->ContactSelfStatuses)
		{
			if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId))
			{
				Pending.SelfStatuses.Add(Status.GetValue());
			}
		}
		Watched = Movement;
		DashEndedHandle = Movement->OnDashEnded.AddUObject(this, &UVeyraDashAbility::OnDashEnded);
	}
	if (!VeyraCombat::Dash(*Caster, FVeyraDash{ Heading, Dash->Distance, Dash->Speed, Dash->Contact }))
	{
		UE_LOG(LogVeyraAbilities, Verbose, TEXT("%s could not dash for %s (cast %d)."), *GetNameSafe(Body), *Cast.Ability.ToString(), Cast.CastId);
		StopWatching();
	}
	return FVeyraChannelPlan();
}

void UVeyraDashAbility::OnDashEnded(const FVeyraDashEnd& End)
{
	const TOptional<FPendingContact> Pending = Contact;
	StopWatching();
	UAbilitySystemComponent* Caster = Pending.IsSet() ? Pending->Caster.Get() : nullptr;
	AActor* Enemy = End.Contact.Get();
	if (End.Reason != EVeyraDashEndReason::EnemyContact || !Caster || !Enemy)
	{
		return;
	}
	const AActor* Body = Caster->GetAvatarActor();
	FVeyraEffectFrame Frame;
	Frame.Origin = Body ? Body->GetActorLocation() : Enemy->GetActorLocation();
	Frame.Direction = Pending->Direction;
	Frame.bOriginIsCaster = Body != nullptr;
	VeyraEffectDelivery::Apply(*Caster, *Enemy, Pending->Effects, Frame, Pending->Source);
	for (const FVeyraStatusSpec& Status : Pending->SelfStatuses)
	{
		VeyraCombat::ApplyStatus(*Caster, *Caster, Status);
	}
}

void UVeyraDashAbility::StopWatching()
{
	if (UVeyraMovementComponent* Movement = Watched.Get())
	{
		Movement->OnDashEnded.Remove(DashEndedHandle);
	}
	Watched.Reset();
	DashEndedHandle.Reset();
	Contact.Reset();
}

bool UVeyraDashAbility::IsOffensive(const FVeyraContentId& Ability) const
{
	// A dash is offensive only if what it passes through or lands on takes its effects.
	const FVeyraDashAbilityTuning* Tuning = UVeyraAbilitiesTuningSubsystem::FindDash(Ability);
	return Tuning && (!Tuning->StartZones.IsEmpty() || !Tuning->ContactEffects.Damage.IsEmpty() || !Tuning->ContactEffects.Statuses.IsEmpty()
		|| !Tuning->ContactEffects.Displacement.IsEmpty());
}
