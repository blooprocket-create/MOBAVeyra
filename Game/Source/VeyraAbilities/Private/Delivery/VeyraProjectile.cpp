// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Delivery/VeyraProjectile.h"

#include "AbilitySystemComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Shapes/VeyraShapes.h"
#include "Targeting/VeyraTargeting.h"
#include "Units/VeyraUnit.h"
#include "VeyraAbilitiesLog.h"

AVeyraProjectile::AVeyraProjectile()
{
	// Only the server flies it, from its launch on. Clients get its launch data, not its movement.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	// Every machine sees it until Vision decides who sees what (ADR-009 §7).
	bAlwaysRelevant = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AVeyraProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraProjectile, Flight, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraProjectile, LaunchedFrom, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraProjectile, LaunchedAt, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraProjectile, Direction, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraProjectile, Speed, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraProjectile, Radius, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraProjectile, Range, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraProjectile, HomingTarget, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraProjectile, Team, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraProjectile, Ability, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraProjectile, CastId, Params);
}

void AVeyraProjectile::LaunchLine(UAbilitySystemComponent& InCaster, const FVector& InDirection, const FVeyraProjectileTuning& Tuning,
	EVeyraSkillshotCollision InCollision, FVeyraPreparedEffects InEffects, FVeyraPreparedEffects InPassThroughEffects, const FVeyraContentId& InAbility,
	int32 InCastId)
{
	Flight = EVeyraProjectileFlight::Line;
	Direction = InDirection.GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = GetActorForwardVector().GetSafeNormal2D();
	}
	Speed = Tuning.Speed;
	Radius = Tuning.Radius;
	Range = Tuning.Range;
	Collision = InCollision;
	Effects = MoveTemp(InEffects);
	PassThroughEffects = MoveTemp(InPassThroughEffects);
	Launch(InCaster, InAbility, InCastId);
}

void AVeyraProjectile::LaunchHoming(UAbilitySystemComponent& InCaster, AActor& Target, double InSpeed, double InRadius, FVeyraPreparedEffects InEffects,
	const FVeyraContentId& InAbility, int32 InCastId, TFunction<void(AActor&)> InOnLanded)
{
	Flight = EVeyraProjectileFlight::Homing;
	HomingTarget = &Target;
	Direction = (Target.GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	Speed = InSpeed;
	Radius = InRadius;
	Range = 0.0;
	Effects = MoveTemp(InEffects);
	OnLanded = MoveTemp(InOnLanded);
	Launch(InCaster, InAbility, InCastId);
}

void AVeyraProjectile::Launch(UAbilitySystemComponent& InCaster, const FVeyraContentId& InAbility, int32 InCastId)
{
	Caster = &InCaster;
	LaunchedFrom = GetActorLocation();
	LaunchedAt = GetWorld()->GetTimeSeconds();
	Team = VeyraTeams::TeamOf(InCaster.GetOwner());
	Ability = InAbility;
	CastId = InCastId;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraProjectile, Flight, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraProjectile, LaunchedFrom, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraProjectile, LaunchedAt, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraProjectile, Direction, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraProjectile, Speed, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraProjectile, Radius, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraProjectile, Range, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraProjectile, HomingTarget, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraProjectile, Team, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraProjectile, Ability, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraProjectile, CastId, this);
	SetActorTickEnabled(true);
}

void AVeyraProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AdvanceBy(DeltaSeconds);
}

void AVeyraProjectile::AdvanceBy(double Seconds)
{
	if (!HasAuthority() || IsActorBeingDestroyed() || !(Seconds > 0.0))
	{
		return;
	}
	// Without its caster's participant there is nobody to deal its effects for.
	UAbilitySystemComponent* Source = Caster.Get();
	if (!Source)
	{
		End();
		return;
	}
	if (Flight == EVeyraProjectileFlight::Homing)
	{
		AdvanceHoming(*Source, Speed * Seconds);
	}
	else
	{
		AdvanceLine(*Source, Speed * Seconds);
	}
}

void AVeyraProjectile::AdvanceLine(UAbilitySystemComponent& Source, double Distance)
{
	UWorld& World = *GetWorld();
	const FVector From = GetActorLocation();
	double Step = FMath::Min(Distance, Range - Travelled);
	bool bEnds = Step >= Range - Travelled;

	// Terrain stops it where its centre line meets terrain (ADR-008 §9).
	FHitResult Terrain;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(VeyraProjectile), /*bTraceComplex*/ false, this);
	if (World.LineTraceSingleByObjectType(Terrain, From, From + Direction * Step, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		Step = Terrain.Distance;
		bEnds = true;
	}

	const TArray<FVeyraPathHit> Hits = VeyraShapes::GatherUnitsAlong(World, From, From + Direction * Step, Radius, [this](const AActor& Unit) {
		return VeyraTargeting::AreHostile(this, &Unit) && !Met.ContainsByPredicate([&Unit](const TWeakObjectPtr<AActor>& Earlier) { return Earlier.Get() == &Unit; });
	});
	for (const FVeyraPathHit& Hit : Hits)
	{
		AActor& Unit = *Hit.Unit;
		Met.Add(&Unit);
		if (Collision == EVeyraSkillshotCollision::FirstEnemyVanguard && !VeyraUnits::IsVanguard(&Unit))
		{
			VeyraEffectDelivery::Apply(Source, Unit, PassThroughEffects, PathFrame());
			continue;
		}
		VeyraEffectDelivery::Apply(Source, Unit, Effects, CasterFrame());
		if (Collision != EVeyraSkillshotCollision::Pierce)
		{
			Travelled += Hit.Distance;
			SetActorLocation(From + Direction * Hit.Distance);
			End();
			return;
		}
	}
	Travelled += Step;
	SetActorLocation(From + Direction * Step);
	if (bEnds)
	{
		End();
	}
}

void AVeyraProjectile::AdvanceHoming(UAbilitySystemComponent& Source, double Distance)
{
	AActor* Target = HomingTarget;
	// It fizzles if its target dies or leaves before it lands.
	if (!IsValid(Target) || !VeyraTargeting::IsAlive(Target))
	{
		End();
		return;
	}
	const FVector Here = GetActorLocation();
	FVector ToTarget = Target->GetActorLocation() - Here;
	ToTarget.Z = 0.0;
	const FVector Heading = ToTarget.GetSafeNormal();
	const double Gap = FMath::Max(0.0, ToTarget.Size() - Target->GetSimpleCollisionRadius() - Radius);
	if (Distance < Gap)
	{
		SetActorLocation(Here + Heading * Distance);
		return;
	}
	SetActorLocation(Here + Heading * Gap);
	Met.Add(Target);
	VeyraEffectDelivery::Apply(Source, *Target, Effects, CasterFrame());
	if (OnLanded)
	{
		OnLanded(*Target);
	}
	End();
}

FVector AVeyraProjectile::GetLineLocationAt(double ServerTime) const
{
	if (Flight != EVeyraProjectileFlight::Line)
	{
		return LaunchedFrom;
	}
	return LaunchedFrom + Direction * FMath::Clamp(Speed * (ServerTime - LaunchedAt), 0.0, Range);
}

FVeyraEffectFrame AVeyraProjectile::CasterFrame() const
{
	FVeyraEffectFrame Frame;
	Frame.Direction = Direction;
	const UAbilitySystemComponent* Source = Caster.Get();
	const APawn* Body = Source ? Cast<APawn>(Source->GetAvatarActor()) : nullptr;
	Frame.bOriginIsCaster = Body && VeyraTargeting::IsAlive(Body);
	Frame.Origin = Frame.bOriginIsCaster ? Body->GetActorLocation() : LaunchedFrom;
	return Frame;
}

FVeyraEffectFrame AVeyraProjectile::PathFrame() const
{
	FVeyraEffectFrame Frame;
	Frame.Origin = GetActorLocation();
	Frame.Direction = Direction;
	return Frame;
}

void AVeyraProjectile::End()
{
	UE_LOG(LogVeyraAbilities, Verbose, TEXT("A projectile of %s (cast %d) ended after meeting %d unit(s)."), *Ability.ToString(), CastId, Met.Num());
	SetActorTickEnabled(false);
	Destroy();
}
