// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraSkillshotAbility.h"

#include "VeyraCombatVerbs.h"
#include "Entities/VeyraPlacedMarker.h"
#include "Delivery/VeyraVolleySubsystem.h"
#include "AbilitySystemComponent.h"
#include "Delivery/VeyraProjectile.h"
#include "Engine/World.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraAbilitiesVerbs.h"

bool UVeyraSkillshotAbility::Defines(const FVeyraContentId& Ability) const
{
	return UVeyraAbilitiesTuningSubsystem::FindSkillshot(Ability) != nullptr;
}

double UVeyraSkillshotAbility::GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraSkillshotAbilityTuning* Skillshot = UVeyraAbilitiesTuningSubsystem::FindSkillshot(Ability);
	return Skillshot ? VeyraAbilityRules::ValueAtRank(Skillshot->Cast.ResourceCostByRank, Rank) : 0.0;
}

double UVeyraSkillshotAbility::GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraSkillshotAbilityTuning* Skillshot = UVeyraAbilitiesTuningSubsystem::FindSkillshot(Ability);
	return Skillshot ? VeyraAbilityRules::ValueAtRank(Skillshot->Cast.CooldownSecondsByRank, Rank) : 0.0;
}

EVeyraCastRejection UVeyraSkillshotAbility::CheckTarget(const AActor& /*Caster*/, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const
{
	if (!UVeyraAbilitiesTuningSubsystem::FindSkillshot(Ability))
	{
		return EVeyraCastRejection::UnknownAbility;
	}
	// The point aims it.
	return HasUsablePoint(Target) ? EVeyraCastRejection::None : EVeyraCastRejection::InvalidLocation;
}

const FVeyraCastTuning* UVeyraSkillshotAbility::GetCastTuning(const FVeyraContentId& Ability) const
{
	const FVeyraSkillshotAbilityTuning* Skillshot = UVeyraAbilitiesTuningSubsystem::FindSkillshot(Ability);
	return Skillshot ? &Skillshot->Cast : nullptr;
}

bool UVeyraSkillshotAbility::MovesCaster(const FVeyraContentId& Ability) const
{
	const FVeyraSkillshotAbilityTuning* Skillshot = UVeyraAbilitiesTuningSubsystem::FindSkillshot(Ability);
	return Skillshot && !Skillshot->CasterDash.IsEmpty();
}

TFunction<void(AActor&)> UVeyraSkillshotAbility::ReturnIfHeld(UAbilitySystemComponent& Caster, const FVeyraSkillshotAbilityTuning& Skillshot, const FVeyraContentId& Ability)
{
	if (Skillshot.ReturnIfHeld.IsEmpty())
	{
		return nullptr;
	}
	const FVeyraReturnShotTuning Return = Skillshot.ReturnIfHeld[0];
	const double Radius = Skillshot.Projectile.Radius;
	// Striking a unit that already held the caster's mark, it flies back; reaching the caster refunds
	// part of what remains of its cooldown (ADR-030 §8). It carries nothing on the way back.
	return [WeakCaster = TWeakObjectPtr<UAbilitySystemComponent>(&Caster), Return, Radius, Ability](AActor& Struck) {
		UAbilitySystemComponent* Source = WeakCaster.Get();
		AActor* Home = Source ? Source->GetAvatarActor() : nullptr;
		UWorld* World = Struck.GetWorld();
		if (!Source || !Home || !World || !VeyraCombat::HasStatusFrom(&Struck, Return.Status, *Source))
		{
			return;
		}
		if (AVeyraProjectile* Back = World->SpawnActor<AVeyraProjectile>(AVeyraProjectile::StaticClass(), FTransform(Struck.GetActorLocation())))
		{
			Back->LaunchHoming(*Source, *Home, Return.Speed, Radius, FVeyraPreparedEffects(), FVeyraContentId(), 0, [WeakCaster, Ability, Return](AActor&) {
				if (UAbilitySystemComponent* Caught = WeakCaster.Get())
				{
					VeyraAbilities::RefundCooldown(*Caught, Ability, Return.CooldownRefund);
				}
			});
		}
	};
}

FVeyraChannelPlan UVeyraSkillshotAbility::Deliver(const FVeyraCast& Cast)
{
	const FVeyraSkillshotAbilityTuning* Skillshot = UVeyraAbilitiesTuningSubsystem::FindSkillshot(Cast.Ability);
	UAbilitySystemComponent* Caster = Cast.Caster.Get();
	const AActor* Body = Caster ? Caster->GetAvatarActor() : nullptr;
	UWorld* World = GetWorld();
	if (!Skillshot || !Caster || !Body || !World)
	{
		return FVeyraChannelPlan();
	}

	// A volley's shot keeps to its lane (ADR-018 §6), and counts against it.
	UVeyraVolleySubsystem* Volleys = World->GetSubsystem<UVeyraVolleySubsystem>();
	const FVector Direction = Volleys ? Volleys->AimWithin(*Caster, Cast.Ability, Cast.Direction) : Cast.Direction;

	// Its caster's marker throws it too, toward the same point; the two share what they strike (ADR-031 §6).
	const AVeyraPlacedMarker* Mimic = Skillshot->Mimic.IsEmpty() ? nullptr : AVeyraPlacedMarker::FindStanding(*Caster, Skillshot->Mimic[0].MarkerAbility);
	TSharedPtr<FVeyraSharedStrikes> Shared;
	if (Mimic)
	{
		Shared = MakeShared<FVeyraSharedStrikes>();
		Shared->RepeatEffects = VeyraEffectDelivery::Prepare(*Caster, Skillshot->Mimic[0].RepeatEffects, Cast.Rank);
	}
	// It sets off from where the caster is at Commit, which a free windup may have moved.
	const FTransform Launch(Direction.Rotation(), Body->GetActorLocation());
	if (AVeyraProjectile* Projectile = World->SpawnActor<AVeyraProjectile>(AVeyraProjectile::StaticClass(), Launch))
	{
		Projectile->LaunchLine(*Caster, Direction, Skillshot->Projectile, Skillshot->Collision,
			VeyraEffectDelivery::Prepare(*Caster, Skillshot->Effects, Cast.Rank), VeyraEffectDelivery::Prepare(*Caster, Skillshot->PassThroughEffects, Cast.Rank),
			Cast.Ability, Cast.CastId, ReturnIfHeld(*Caster, *Skillshot, Cast.Ability), Shared);
	}
	if (Mimic)
	{
		const FVector Toward = (Cast.Point - Mimic->GetActorLocation()).GetSafeNormal2D();
		const FVector Aim = Toward.IsNearlyZero() ? Direction : Toward;
		const FTransform From(Aim.Rotation(), Mimic->GetActorLocation());
		if (AVeyraProjectile* Echo = World->SpawnActor<AVeyraProjectile>(AVeyraProjectile::StaticClass(), From))
		{
			Echo->LaunchLine(*Caster, Aim, Skillshot->Projectile, Skillshot->Collision, VeyraEffectDelivery::Prepare(*Caster, Skillshot->Effects, Cast.Rank),
				VeyraEffectDelivery::Prepare(*Caster, Skillshot->PassThroughEffects, Cast.Rank), Cast.Ability, Cast.CastId, nullptr, Shared);
		}
	}
	if (Volleys)
	{
		Volleys->NoteShot(*Caster, Cast.Ability);
	}
	// A recoil away from the aim, as the shot leaves (Kade's Reposition).
	if (!Skillshot->CasterDash.IsEmpty())
	{
		const FVeyraCasterDashTuning& Recoil = Skillshot->CasterDash[0];
		VeyraCombat::Dash(*Caster, FVeyraDash{ -Direction.GetSafeNormal2D(), Recoil.Distance, Recoil.Speed, EVeyraDashContact::None });
	}
	return FVeyraChannelPlan();
}
