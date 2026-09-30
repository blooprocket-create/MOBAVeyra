// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraAmbushAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attribution/VeyraAttributionComponent.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Engine/World.h"
#include "Targeting/VeyraTargeting.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraCombatVerbs.h"

namespace
{
	/** What the strike needs once the caster has been gone a while. */
	struct FPendingAmbush
	{
		TWeakObjectPtr<UAbilitySystemComponent> Caster;
		TWeakObjectPtr<AActor> Target;
		FVeyraPreparedEffects Effects;
		TArray<FVeyraContentId> VanishStatuses;
		double BesideDistance = 0.0;
		FVeyraAbilityHitSource Source;
	};

	/** The caster comes back: beside its target on the side it came from, and strikes, if both still live. */
	void Strike(const FPendingAmbush& Pending)
	{
		UAbilitySystemComponent* Caster = Pending.Caster.Get();
		if (!Caster)
		{
			return;
		}
		for (const FVeyraContentId& Status : Pending.VanishStatuses)
		{
			VeyraCombat::RemoveStatus(*Caster, Status);
		}
		const AActor* Body = Caster->GetAvatarActor();
		AActor* Target = Pending.Target.Get();
		if (!Body || !Target || !VeyraTargeting::IsAlive(Body) || !VeyraTargeting::IsAlive(Target))
		{
			return;
		}
		FVector Away = (Body->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
		if (Away.IsNearlyZero())
		{
			Away = -Target->GetActorForwardVector().GetSafeNormal2D();
		}
		const double Gap = Target->GetSimpleCollisionRadius() + Body->GetSimpleCollisionRadius() + Pending.BesideDistance;
		const FVector Beside = Target->GetActorLocation() + Away * Gap;
		if (!VeyraCombat::Blink(*Caster, Beside, -Away))
		{
			return;
		}
		FVeyraEffectFrame Frame;
		Frame.Origin = Beside;
		Frame.Direction = -Away;
		Frame.bOriginIsCaster = true;
		VeyraEffectDelivery::Apply(*Caster, *Target, Pending.Effects, Frame, Pending.Source);
	}
}

bool UVeyraAmbushAbility::HurtLately(const UAbilitySystemComponent& Caster, const AActor& Target, double Seconds)
{
	const UAbilitySystemComponent* Held = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Target);
	const AActor* Owner = Held ? Held->GetOwner() : nullptr;
	const UVeyraAttributionComponent* Attribution = Owner ? Owner->FindComponentByClass<UVeyraAttributionComponent>() : nullptr;
	const UWorld* World = Target.GetWorld();
	if (!Attribution || !World)
	{
		return false;
	}
	const double Now = World->GetTimeSeconds();
	return Attribution->GetContributions().ContainsByPredicate([&Caster, Now, Seconds](const FVeyraContribution& Each) {
		return Each.Contributor.Get() == &Caster && Now - Each.AtSeconds <= Seconds;
	});
}

bool UVeyraAmbushAbility::Defines(const FVeyraContentId& Ability) const
{
	return UVeyraAbilitiesTuningSubsystem::FindAmbush(Ability) != nullptr;
}

double UVeyraAmbushAbility::GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraAmbushAbilityTuning* Ambush = UVeyraAbilitiesTuningSubsystem::FindAmbush(Ability);
	return Ambush ? VeyraAbilityRules::ValueAtRank(Ambush->Cast.ResourceCostByRank, Rank) : 0.0;
}

double UVeyraAmbushAbility::GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraAmbushAbilityTuning* Ambush = UVeyraAbilitiesTuningSubsystem::FindAmbush(Ability);
	return Ambush ? VeyraAbilityRules::ValueAtRank(Ambush->Cast.CooldownSecondsByRank, Rank) : 0.0;
}

const FVeyraCastTuning* UVeyraAmbushAbility::GetCastTuning(const FVeyraContentId& Ability) const
{
	const FVeyraAmbushAbilityTuning* Ambush = UVeyraAbilitiesTuningSubsystem::FindAmbush(Ability);
	return Ambush ? &Ambush->Cast : nullptr;
}

EVeyraCastRejection UVeyraAmbushAbility::CheckTarget(const AActor& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const
{
	const FVeyraAmbushAbilityTuning* Ambush = UVeyraAbilitiesTuningSubsystem::FindAmbush(Ability);
	if (!Ambush)
	{
		return EVeyraCastRejection::UnknownAbility;
	}
	const EVeyraUnitKind Vanguards[] = { EVeyraUnitKind::Vanguard };
	const EVeyraCastRejection Unit = CheckEnemyUnit(Caster, Target.Actor.Get(), Ambush->Cast.CastRange, Vanguards);
	if (Unit != EVeyraCastRejection::None)
	{
		return Unit;
	}
	// Only a Vanguard it hurt lately: one of its playmates (ADR-030 §9).
	const UAbilitySystemComponent* Own = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Caster);
	return Own && HurtLately(*Own, *Target.Actor, Ambush->RecentSeconds) ? EVeyraCastRejection::None : EVeyraCastRejection::InvalidTarget;
}

FVeyraChannelPlan UVeyraAmbushAbility::Deliver(const FVeyraCast& Cast)
{
	const FVeyraAmbushAbilityTuning* Ambush = UVeyraAbilitiesTuningSubsystem::FindAmbush(Cast.Ability);
	UAbilitySystemComponent* Caster = Cast.Caster.Get();
	AActor* Target = Cast.TargetActor.Get();
	UWorld* World = GetWorld();
	if (!Ambush || !Caster || !Target || !World)
	{
		return FVeyraChannelPlan();
	}
	// Gone, Invisible and Untargetable, for as long as it vanishes.
	for (const FVeyraContentId& StatusId : Ambush->VanishStatuses)
	{
		if (TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId, GetCasterLevel(*Caster)))
		{
			Status->DurationSeconds = FMath::Max(Status->DurationSeconds, Ambush->VanishSeconds);
			VeyraCombat::ApplyStatus(*Caster, *Caster, Status.GetValue());
		}
	}
	// The strike is fixed now, at the cast's rank and the caster's power (Combat Bible §50).
	const TSharedRef<FPendingAmbush> Pending = MakeShared<FPendingAmbush>();
	Pending->Caster = Caster;
	Pending->Target = Target;
	Pending->Effects = VeyraEffectDelivery::Prepare(*Caster, Ambush->Effects, Cast.Rank);
	Pending->VanishStatuses = Ambush->VanishStatuses;
	Pending->BesideDistance = Ambush->BesideDistance;
	Pending->Source = FVeyraAbilityHitSource{ Cast.Ability, Cast.CastId };
	// Bound to the caster, not to this ability, whose own timers end with it.
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Caster, [Pending] { Strike(*Pending); }),
		static_cast<float>(Ambush->VanishSeconds), /*bLoop*/ false);
	return FVeyraChannelPlan();
}
