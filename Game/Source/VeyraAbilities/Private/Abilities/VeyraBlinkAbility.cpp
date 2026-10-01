// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraBlinkAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Entities/VeyraPlacedMarker.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraAbilitiesLog.h"
#include "VeyraCombatVerbs.h"

bool UVeyraBlinkAbility::Defines(const FVeyraContentId& Ability) const
{
	return UVeyraAbilitiesTuningSubsystem::FindBlink(Ability) != nullptr;
}

double UVeyraBlinkAbility::GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraBlinkAbilityTuning* Blink = UVeyraAbilitiesTuningSubsystem::FindBlink(Ability);
	return Blink ? VeyraAbilityRules::ValueAtRank(Blink->Cast.ResourceCostByRank, Rank) : 0.0;
}

double UVeyraBlinkAbility::GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraBlinkAbilityTuning* Blink = UVeyraAbilitiesTuningSubsystem::FindBlink(Ability);
	return Blink ? VeyraAbilityRules::ValueAtRank(Blink->Cast.CooldownSecondsByRank, Rank) : 0.0;
}

AVeyraPlacedMarker* UVeyraBlinkAbility::OwnMarkerOf(const UAbilitySystemComponent& Caster, const FVeyraBlinkAbilityTuning& Blink)
{
	return Blink.MarkerAbility.IsEmpty() ? nullptr : AVeyraPlacedMarker::FindStanding(Caster, Blink.MarkerAbility[0]);
}

EVeyraCastRejection UVeyraBlinkAbility::CheckTarget(const AActor& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const
{
	const FVeyraBlinkAbilityTuning* Blink = UVeyraAbilitiesTuningSubsystem::FindBlink(Ability);
	const UAbilitySystemComponent* Abilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Caster);
	if (!Blink || !Abilities)
	{
		return EVeyraCastRejection::UnknownAbility;
	}
	// Its own marker, wherever it stands within reach; no reach for a blink with no cast range (ADR-031 §5).
	const AVeyraPlacedMarker* Marker = OwnMarkerOf(*Abilities, *Blink);
	const bool bInReach = Marker && (!(Blink->Cast.CastRange > 0.0) || FVector::Dist2D(Caster.GetActorLocation(), Marker->GetActorLocation()) <= Blink->Cast.CastRange);
	switch (Blink->To)
	{
	case EVeyraBlinkTo::OwnMarker:
		return bInReach ? EVeyraCastRejection::None : (Marker ? EVeyraCastRejection::OutOfRange : EVeyraCastRejection::InvalidTarget);
	case EVeyraBlinkTo::EnemyUnitOrOwnMarker:
		if (Marker && Target.Actor.Get() == Marker)
		{
			return bInReach ? EVeyraCastRejection::None : EVeyraCastRejection::OutOfRange;
		}
		return CheckEnemyUnit(Caster, Target.Actor.Get(), Blink->Cast.CastRange, Blink->TargetKinds);
	case EVeyraBlinkTo::EnemyUnit:
		return CheckEnemyUnit(Caster, Target.Actor.Get(), Blink->Cast.CastRange, Blink->TargetKinds);
	}
	return EVeyraCastRejection::InvalidTarget;
}

const FVeyraCastTuning* UVeyraBlinkAbility::GetCastTuning(const FVeyraContentId& Ability) const
{
	const FVeyraBlinkAbilityTuning* Blink = UVeyraAbilitiesTuningSubsystem::FindBlink(Ability);
	return Blink ? &Blink->Cast : nullptr;
}

FVeyraChannelPlan UVeyraBlinkAbility::Deliver(const FVeyraCast& Cast)
{
	const FVeyraBlinkAbilityTuning* Blink = UVeyraAbilitiesTuningSubsystem::FindBlink(Cast.Ability);
	UAbilitySystemComponent* Caster = Cast.Caster.Get();
	const AActor* Body = Caster ? Caster->GetAvatarActor() : nullptr;
	if (!Blink || !Caster || !Body)
	{
		return FVeyraChannelPlan();
	}
	AVeyraPlacedMarker* Marker = OwnMarkerOf(*Caster, *Blink);
	AActor* Target = Cast.TargetActor.Get();
	const bool bToMarker = Blink->To == EVeyraBlinkTo::OwnMarker || (Blink->To == EVeyraBlinkTo::EnemyUnitOrOwnMarker && Marker && Target == Marker);
	if (bToMarker)
	{
		// To where its marker stands; with a swap, the marker takes the caster's old place.
		if (!Marker)
		{
			return FVeyraChannelPlan();
		}
		const FVector From = Body->GetActorLocation();
		if (VeyraCombat::Blink(*Caster, Marker->GetActorLocation(), Body->GetActorForwardVector()) && Blink->Swap == EVeyraBlinkSwap::Swap)
		{
			Marker->Relocate(From);
		}
		return FVeyraChannelPlan();
	}
	// Beside the enemy it names, still standing, which then takes its effects.
	if (!Target || !VeyraTargeting::IsAlive(Target))
	{
		return FVeyraChannelPlan();
	}
	FVector Landing;
	FVector Facing;
	if (!VeyraCombat::BlinkBeside(*Caster, *Target, Blink->BesideDistance, Landing, Facing))
	{
		UE_LOG(LogVeyraAbilities, Verbose, TEXT("%s could not blink beside %s for %s (cast %d)."), *GetNameSafe(Body), *GetNameSafe(Target), *Cast.Ability.ToString(),
			Cast.CastId);
		return FVeyraChannelPlan();
	}
	FVeyraEffectFrame Frame;
	Frame.Origin = Landing;
	Frame.Direction = Facing;
	Frame.bOriginIsCaster = true;
	VeyraEffectDelivery::Apply(*Caster, *Target, VeyraEffectDelivery::Prepare(*Caster, Blink->Effects, Cast.Rank), Frame,
		FVeyraAbilityHitSource{ Cast.Ability, Cast.CastId });
	return FVeyraChannelPlan();
}

bool UVeyraBlinkAbility::IsOffensive(const FVeyraContentId& Ability) const
{
	// One that may name an enemy threatens; one only to its own marker does not (ADR-031 §5).
	const FVeyraBlinkAbilityTuning* Blink = UVeyraAbilitiesTuningSubsystem::FindBlink(Ability);
	return Blink && Blink->To != EVeyraBlinkTo::OwnMarker;
}
