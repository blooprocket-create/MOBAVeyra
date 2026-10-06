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
#include "Terrain/VeyraGround.h"
#include "Units/VeyraUnit.h"
#include "VeyraAbilitiesLog.h"

namespace
{
	/** The ability hit a projectile launched at From carries to the unit it strikes. */
	FVeyraAbilityHitSource ProjectileHit(const FVeyraContentId& Ability, int32 CastId, const FVector& From)
	{
		FVeyraAbilityHitSource Hit{ Ability, CastId };
		Hit.ProjectileFrom = From;
		return Hit;
	}
}

AVeyraProjectile::AVeyraProjectile()
{
	// Only the server flies it, from its launch on. Clients get its launch data, not its movement.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	// Always relevant, so replays record it; Vision's fog gate decides which clients receive it (ADR-016 §3).
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
	int32 InCastId, TFunction<void(AActor&)> InBeforeStrike, TSharedPtr<FVeyraSharedStrikes> InShared)
{
	BeforeStrike = MoveTemp(InBeforeStrike);
	Shared = MoveTemp(InShared);
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

	// Terrain stops it where its body meets terrain, as its body meets units (ADR-008 §9, ADR-009 §4).
	// Terrain its body already overlaps where the step starts, as a wall its caster stood against,
	// stops it only where its centre meets it, so it can still be cast along the wall; any other
	// terrain its body meets further on still stops it.
	// The shot keeps its height above the ground along its path (ADR-040 §4): slopes neither stop it nor strand it in
	// the air; the ground is not terrain that stops it, walls are. Distances along it are the ground's plan.
	const auto Along = [&World, &From, this](double Distance) {
		return VeyraGround::Carried(World, From, FVector2D(From + Direction * Distance));
	};
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(VeyraProjectile), /*bTraceComplex*/ false, this);
	const FCollisionObjectQueryParams TerrainObjects(ECC_WorldStatic);
	const FVector To = Along(Step);
	double TerrainDistance = TNumericLimits<double>::Max();
	TArray<FHitResult> BodyHits;
	World.SweepMultiByObjectType(BodyHits, From, To, FQuat::Identity, TerrainObjects, FCollisionShape::MakeSphere(static_cast<float>(Radius)), Params);
	for (const FHitResult& Hit : BodyHits)
	{
		if (!Hit.bStartPenetrating)
		{
			TerrainDistance = FMath::Min(TerrainDistance, Hit.Time * Step);
		}
	}
	FHitResult CentreHit;
	if (World.LineTraceSingleByObjectType(CentreHit, From, To, TerrainObjects, Params))
	{
		TerrainDistance = FMath::Min(TerrainDistance, CentreHit.Time * Step);
	}
	if (TerrainDistance <= Step)
	{
		Step = TerrainDistance;
		bEnds = true;
	}

	const TArray<FVeyraPathHit> Hits = VeyraShapes::GatherUnitsAlong(World, From, From + Direction * Step, Radius, [this](const AActor& Unit) {
		return VeyraTargeting::CanHitEnemy(this, Unit) && !Met.ContainsByPredicate([&Unit](const TWeakObjectPtr<AActor>& Earlier) { return Earlier.Get() == &Unit; });
	});
	for (const FVeyraPathHit& Hit : Hits)
	{
		AActor& Unit = *Hit.Unit;
		Met.Add(&Unit);
		if (Collision == EVeyraSkillshotCollision::FirstEnemyVanguard && !VeyraUnits::IsVanguard(&Unit))
		{
			VeyraEffectDelivery::Apply(Source, Unit, PassThroughEffects, PathFrame(), ProjectileHit(Ability, CastId, LaunchedFrom));
			continue;
		}
		// Before the hit lands, so what it reads of the unit is as the shot found it (ADR-030 §8).
		if (BeforeStrike)
		{
			BeforeStrike(Unit);
		}
		// Another projectile of the cast struck it already: it takes the repeat instead (ADR-031 §6).
		const FVeyraPreparedEffects* Landing = &Effects;
		if (Shared.IsValid())
		{
			if (Shared->Struck.ContainsByPredicate([&Unit](const TWeakObjectPtr<AActor>& Earlier) { return Earlier.Get() == &Unit; }))
			{
				Landing = &Shared->RepeatEffects;
			}
			else
			{
				Shared->Struck.Add(&Unit);
			}
		}
		VeyraEffectDelivery::Apply(Source, Unit, *Landing, CasterFrame(), ProjectileHit(Ability, CastId, LaunchedFrom));
		if (Collision != EVeyraSkillshotCollision::Pierce)
		{
			Travelled += Hit.Distance;
			SetActorLocation(Along(Hit.Distance));
			End();
			return;
		}
	}
	Travelled += Step;
	SetActorLocation(Along(Step));
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
	// It closes on its target over the ground's plan, rising or falling toward the target's height as it goes, so a
	// shot up a slope or down into the river meets its target's body (ADR-040 §4).
	const FVector Here = GetActorLocation();
	const FVector ToTarget = Target->GetActorLocation() - Here;
	const FVector Heading = ToTarget.GetSafeNormal2D();
	const double Apart = ToTarget.Size2D();
	const double Gap = FMath::Max(0.0, Apart - Target->GetSimpleCollisionRadius() - Radius);
	const auto Toward = [&](double Run) {
		return Here + Heading * Run + FVector::UpVector * (Apart > 0.0 ? ToTarget.Z * Run / Apart : 0.0);
	};
	if (Distance < Gap)
	{
		SetActorLocation(Toward(Distance));
		return;
	}
	SetActorLocation(Toward(Gap));
	// A targeted shot at an enemy that is Untargetable as it would land fails (Combat Bible §10).
	if (VeyraTargeting::AreHostile(this, Target) && VeyraTargeting::IsUntargetable(*Target))
	{
		End();
		return;
	}
	Met.Add(Target);
	VeyraEffectDelivery::Apply(Source, *Target, Effects, CasterFrame(), ProjectileHit(Ability, CastId, LaunchedFrom));
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
	// As the server flies it: at its launch height above the ground along its path.
	const FVector2D Place(LaunchedFrom + Direction * FMath::Clamp(Speed * (ServerTime - LaunchedAt), 0.0, Range));
	const UWorld* World = GetWorld();
	return World ? VeyraGround::Carried(*World, LaunchedFrom, Place) : FVector(Place, LaunchedFrom.Z);
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
	if (Flight == EVeyraProjectileFlight::Line && OnLineEnded && Caster.IsValid())
	{
		const TFunction<void(const FVector&)> Ended = MoveTemp(OnLineEnded);
		OnLineEnded = nullptr;
		Ended(GetActorLocation());
	}
	SetActorTickEnabled(false);
	Destroy();
}
