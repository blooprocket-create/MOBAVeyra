// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraCommandAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Companions/VeyraCompanion.h"
#include "Companions/VeyraCompanionSubsystem.h"
#include "Delivery/VeyraAreaDelivery.h"
#include "Engine/World.h"
#include "Targeting/VeyraTargeting.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraAbilitiesLog.h"
#include "VeyraCombatVerbs.h"

namespace
{
	/** Caster's companion while it is on the battleground, or null. */
	AVeyraCompanion* LivingCompanionOf(const UAbilitySystemComponent& Caster)
	{
		const UWorld* World = Caster.GetWorld();
		const UVeyraCompanionSubsystem* Keeper = World ? World->GetSubsystem<UVeyraCompanionSubsystem>() : nullptr;
		return Keeper ? Keeper->FindLiving(Caster) : nullptr;
	}
}

bool UVeyraCommandAbility::Defines(const FVeyraContentId& Ability) const
{
	return UVeyraAbilitiesTuningSubsystem::FindCommand(Ability) != nullptr;
}

double UVeyraCommandAbility::GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraCommandAbilityTuning* Command = UVeyraAbilitiesTuningSubsystem::FindCommand(Ability);
	return Command ? VeyraAbilityRules::ValueAtRank(Command->Cast.ResourceCostByRank, Rank) : 0.0;
}

double UVeyraCommandAbility::GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraCommandAbilityTuning* Command = UVeyraAbilitiesTuningSubsystem::FindCommand(Ability);
	return Command ? VeyraAbilityRules::ValueAtRank(Command->Cast.CooldownSecondsByRank, Rank) : 0.0;
}

EVeyraCastRejection UVeyraCommandAbility::CheckTarget(const AActor& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const
{
	const FVeyraCommandAbilityTuning* Command = UVeyraAbilitiesTuningSubsystem::FindCommand(Ability);
	const UAbilitySystemComponent* Abilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Caster);
	if (!Command || !Abilities)
	{
		return EVeyraCastRejection::UnknownAbility;
	}
	// A summon or redirect binds to the unit the cast names: an allied Vanguard, its caster among them, or an
	// enemy unit its caster can see (ADR-035 §5). A summon forms its companion; a redirect needs it living.
	if (Command->Order == EVeyraCompanionOrder::Summon || Command->Order == EVeyraCompanionOrder::Redirect)
	{
		if (Command->Order == EVeyraCompanionOrder::Redirect && !LivingCompanionOf(*Abilities))
		{
			return EVeyraCastRejection::NoCompanion;
		}
		const AActor* Unit = Target.Actor.Get();
		if (Command->BindTo == EVeyraCompanionBind::Enemy)
		{
			return CheckEnemyUnit(Caster, Unit, Command->Cast.CastRange, {});
		}
		if (Unit == &Caster)
		{
			return EVeyraCastRejection::None;
		}
		switch (VeyraTargeting::CheckAllyTarget(Caster, Unit, Command->Cast.CastRange))
		{
		case EVeyraTargetValidity::Valid:
			return EVeyraCastRejection::None;
		case EVeyraTargetValidity::OutOfRange:
			return EVeyraCastRejection::OutOfRange;
		case EVeyraTargetValidity::Dead:
			return EVeyraCastRejection::TargetDead;
		default:
			return EVeyraCastRejection::InvalidTarget;
		}
	}
	if (!LivingCompanionOf(*Abilities))
	{
		return EVeyraCastRejection::NoCompanion;
	}
	// A hold needs its point; a recall needs nothing.
	return Command->Order == EVeyraCompanionOrder::Hold && !Target.bHasLocation ? EVeyraCastRejection::InvalidTarget : EVeyraCastRejection::None;
}

const FVeyraCastTuning* UVeyraCommandAbility::GetCastTuning(const FVeyraContentId& Ability) const
{
	const FVeyraCommandAbilityTuning* Command = UVeyraAbilitiesTuningSubsystem::FindCommand(Ability);
	return Command ? &Command->Cast : nullptr;
}

FVeyraChannelPlan UVeyraCommandAbility::Deliver(const FVeyraCast& Cast)
{
	const FVeyraCommandAbilityTuning* Command = UVeyraAbilitiesTuningSubsystem::FindCommand(Cast.Ability);
	UAbilitySystemComponent* Caster = Cast.Caster.Get();
	UWorld* World = Caster ? Caster->GetWorld() : nullptr;
	// A summon forms its companion bound to the unit, or redirects the one living; a redirect binds it anew (ADR-035 §5).
	if (Command && World && (Command->Order == EVeyraCompanionOrder::Summon || Command->Order == EVeyraCompanionOrder::Redirect))
	{
		UVeyraCompanionSubsystem* Keeper = World->GetSubsystem<UVeyraCompanionSubsystem>();
		AActor* Unit = Cast.TargetActor.Get();
		const EVeyraCompanionMode Mode = Command->BindTo == EVeyraCompanionBind::Enemy ? EVeyraCompanionMode::Hunt : EVeyraCompanionMode::Escort;
		if (Keeper && Unit && Command->Order == EVeyraCompanionOrder::Summon)
		{
			Keeper->SummonFor(*Caster, Command->Companion[0], Mode, *Unit, Command->LifetimeSeconds);
		}
		else if (Keeper && Unit)
		{
			Keeper->Redirect(*Caster, Mode, *Unit);
		}
		return FVeyraChannelPlan();
	}
	AVeyraCompanion* Companion = Caster ? LivingCompanionOf(*Caster) : nullptr;
	if (!Command || !Companion || !World)
	{
		return FVeyraChannelPlan();
	}
	if (Command->Order == EVeyraCompanionOrder::Recall)
	{
		// Back to its owner's side: its hold ends, and with it the hold's recall (ADR-034 §5).
		Companion->EndHold();
		return FVeyraChannelPlan();
	}
	// It leaps to the point, on the nearest ground there, and holds it from its landing.
	UAbilitySystemComponent& Leaper = *Companion->GetAbilitySystemComponent();
	const FVector Ground = VeyraCombat::NearestGround(*World, Cast.Point);
	const FVector From = Companion->GetActorLocation();
	const double Distance = FVector::Dist2D(From, Ground);
	FVeyraDash Leap;
	Leap.Direction = (Ground - From).GetSafeNormal2D();
	Leap.Distance = Distance;
	Leap.Speed = Command->LeapSpeed;
	const bool bLeapt = Distance > 0.0 && VeyraCombat::Dash(Leaper, Leap);
	const double LeapSeconds = bLeapt ? Distance / Command->LeapSpeed : 0.0;
	Companion->HoldAt(Ground, World->GetTimeSeconds() + LeapSeconds + Command->HoldSeconds, Cast.Ability);

	// What it lands with is its own hit, from its own power now (Combat Bible §50; ADR-034 §5).
	const TArray<FVeyraPreparedZone> Zones = VeyraAreaDelivery::PrepareZones(Leaper, Command->LandingZones, Cast.Rank);
	if (Zones.IsEmpty())
	{
		return FVeyraChannelPlan();
	}
	const TWeakObjectPtr<AVeyraCompanion> WeakCompanion = Companion;
	const FVeyraAbilityHitSource Source{ Cast.Ability, Cast.CastId };
	const FVector Facing = Leap.Direction.IsNearlyZero() ? Companion->GetActorForwardVector() : Leap.Direction;
	const auto Land = [WeakCompanion, Zones, Source, Facing] {
		AVeyraCompanion* Lander = WeakCompanion.Get();
		UWorld* Here = Lander ? Lander->GetWorld() : nullptr;
		if (!Here || !Lander->IsAlive() || Lander->IsBanished())
		{
			return;
		}
		FVeyraEffectFrame Frame;
		Frame.Origin = Lander->GetActorLocation();
		Frame.Direction = Facing;
		VeyraAreaDelivery::Resolve(*Here, *Lander->GetAbilitySystemComponent(), Frame, Zones, Source);
	};
	if (LeapSeconds > 0.0)
	{
		// Bound to the caster, as an aura is: GAS clears an ability's own timers as its cast ends.
		World->GetTimerManager().SetTimer(LandingTimer, FTimerDelegate::CreateWeakLambda(Caster, Land), static_cast<float>(LeapSeconds), /*bLoop*/ false);
	}
	else
	{
		Land();
	}
	return FVeyraChannelPlan();
}

bool UVeyraCommandAbility::IsOffensive(const FVeyraContentId& Ability) const
{
	// A hold that lands zones threatens, and so does sending a companion to hunt; a recall or an escort does not.
	const FVeyraCommandAbilityTuning* Command = UVeyraAbilitiesTuningSubsystem::FindCommand(Ability);
	return Command && ((Command->Order == EVeyraCompanionOrder::Hold && !Command->LandingZones.IsEmpty()) || Command->BindTo == EVeyraCompanionBind::Enemy);
}
