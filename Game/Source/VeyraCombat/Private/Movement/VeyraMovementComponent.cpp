// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Movement/VeyraMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraMobilitySet.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Movement/VeyraMovementRules.h"
#include "NavigationSystem.h"
#include "Shapes/VeyraShapes.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "Units/VeyraUnit.h"

namespace
{
	bool IsForcedMoveValid(const FVector& Direction, double Distance, double Speed)
	{
		return !Direction.GetSafeNormal2D().IsNearlyZero() && FMath::IsFinite(Distance) && Distance > 0.0 && FMath::IsFinite(Speed) && Speed > 0.0;
	}
}

void UVeyraMovementComponent::BindCombatant(UAbilitySystemComponent* Combatant)
{
	if (UVeyraStatusComponent* Statuses = FollowedStatuses.Get())
	{
		Statuses->OnStatusesChanged.Remove(StatusesChangedHandle);
	}
	StatusesChangedHandle.Reset();
	FollowedCombatant = Combatant;
	FollowedStatuses = nullptr;

	AActor* Participant = Combatant ? Combatant->GetOwner() : nullptr;
	if (UVeyraStatusComponent* Statuses = Participant ? Participant->FindComponentByClass<UVeyraStatusComponent>() : nullptr)
	{
		FollowedStatuses = Statuses;
		StatusesChangedHandle = Statuses->OnStatusesChanged.AddUObject(this, &UVeyraMovementComponent::OnFollowedStatusesChanged);
	}
	OnFollowedStatusesChanged();
}

void UVeyraMovementComponent::OnFollowedStatusesChanged()
{
	// A Fear cleansed or ended early ends its flight with it.
	const UVeyraStatusComponent* Statuses = FollowedStatuses.Get();
	if (IsFleeing() && !(Statuses && Statuses->Has(EVeyraStatusKind::Fear)))
	{
		EndForcedMove();
	}
	RefreshMovementLock();
	RefreshBody();
}

void UVeyraMovementComponent::RefreshBody()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Capsule)
	{
		return;
	}
	if (!BaseCapsuleRadius.IsSet())
	{
		BaseCapsuleRadius = Capsule->GetUnscaledCapsuleRadius();
		BasePawnResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
	}
	const UVeyraStatusComponent* Statuses = FollowedStatuses.Get();
	// Ghosted, it passes through units, never terrain (Combat Bible §24).
	const bool bGhosted = Statuses && Statuses->Has(EVeyraStatusKind::Ghosted);
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, bGhosted ? ECR_Ignore : BasePawnResponse.GetValue());
	// Its body is wider or narrower for its hits (§13).
	const double Scale = Statuses && Statuses->Has(EVeyraStatusKind::BodyScale) ? Statuses->GetStrongest(EVeyraStatusKind::BodyScale) : 1.0;
	Capsule->SetCapsuleRadius(static_cast<float>(BaseCapsuleRadius.GetValue() * Scale), /*bUpdateOverlaps*/ true);
}

float UVeyraMovementComponent::GetMaxSpeed() const
{
	const UAbilitySystemComponent* Combatant = FollowedCombatant.Get();
	const bool bWalks = MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking || MovementMode == MOVE_Falling;
	if (!Combatant || !bWalks)
	{
		return Super::GetMaxSpeed();
	}

	const UVeyraStatusComponent* Statuses = FollowedStatuses.Get();
	FVeyraSpeedInputs Inputs;
	Inputs.MoveSpeed = Combatant->GetNumericAttribute(UVeyraMobilitySet::GetMoveSpeedAttribute());
	Inputs.BaseMoveSpeed = Combatant->GetNumericAttributeBase(UVeyraMobilitySet::GetMoveSpeedAttribute());
	// Slow Resistance weakens the Slow that controls the unit's speed (ADR-018 §2).
	Inputs.StrongestSlow = Statuses ? Statuses->GetStrongestSlow() * Statuses->GetRetained(EVeyraStatusKind::SlowResistance) : 0.0;
	Inputs.bStunned = Statuses && EnumHasAnyFlags(Statuses->GetActionBlocks(), EVeyraActionBlocks::Move);
	// A bonus toward enemy Vanguards holds only while the body heads for one (§23 conditional bonus).
	const double Pursuit = Statuses ? Statuses->GetStrongest(EVeyraStatusKind::MoveSpeedTowardEnemyVanguards) : 0.0;
	if (Pursuit > 0.0 && IsMovingTowardEnemyVanguard())
	{
		Inputs.ConditionalBonus = Pursuit;
	}
	return static_cast<float>(VeyraMovementRules::EffectiveSpeed(Inputs, UVeyraCombatTuningSubsystem::Get().Movement));
}

bool UVeyraMovementComponent::IsMovingTowardEnemyVanguard() const
{
	const UAbilitySystemComponent* Combatant = FollowedCombatant.Get();
	const AActor* Body = GetOwner();
	const UWorld* World = GetWorld();
	const FVector Heading = Velocity.IsNearlyZero() ? FVector(GetCurrentAcceleration()) : FVector(Velocity);
	if (!Combatant || !Body || !World || Heading.IsNearlyZero())
	{
		return false;
	}
	// Enemy Vanguards within range, edge to edge: a circle that far past the body's own edge touches them.
	const FVeyraPursuitTuning& Pursuit = UVeyraCombatTuningSubsystem::Get().Pursuit;
	FVeyraPlacedShape Near;
	Near.Shape.Kind = EVeyraShapeKind::Circle;
	Near.Shape.Radius = Pursuit.Range + Body->GetSimpleCollisionRadius();
	Near.Origin = Body->GetActorLocation();
	const AActor* Side = Combatant->GetOwner();
	const FVector From = Body->GetActorLocation();
	const TArray<AActor*> Ahead = VeyraShapes::GatherUnits(*World, Near, [Side, Body, &From, &Heading, &Pursuit](const AActor& Unit) {
		return &Unit != Body && VeyraUnits::IsVanguard(&Unit) && VeyraTargeting::AreHostile(Side, &Unit) && VeyraTargeting::CanAcquire(Side, Unit)
			&& VeyraMovementRules::IsHeadingToward(From, Heading, Unit.GetActorLocation(), Pursuit.MaxAngleDegrees);
	});
	return !Ahead.IsEmpty();
}

bool UVeyraMovementComponent::StartDisplacement(const FVector& Direction, double Distance, double Speed)
{
	if (!IsForcedMoveValid(Direction, Distance, Speed) || !UpdatedComponent)
	{
		return false;
	}
	const bool bInterruptsDash = IsDashing();
	FForcedMove Move;
	Move.Mode = EVeyraCustomMovementMode::Displaced;
	Move.Destination = ResolveForcedMoveEnd(Direction, Distance);
	Move.Speed = Speed;
	BeginForcedMove(Move);
	if (bInterruptsDash)
	{
		OnDashEnded.Broadcast(FVeyraDashEnd{ EVeyraDashEndReason::Interrupted, nullptr });
	}
	return true;
}

bool UVeyraMovementComponent::StartFleeing(const FVector& Direction, double Distance, double Speed)
{
	if (IsDisplaced() || !IsForcedMoveValid(Direction, Distance, Speed) || !UpdatedComponent)
	{
		return false;
	}
	const bool bInterruptsDash = IsDashing();
	FForcedMove Move;
	Move.Mode = EVeyraCustomMovementMode::Fleeing;
	Move.Destination = ResolveForcedMoveEnd(Direction, Distance);
	Move.Speed = Speed;
	BeginForcedMove(Move);
	if (bInterruptsDash)
	{
		OnDashEnded.Broadcast(FVeyraDashEnd{ EVeyraDashEndReason::Interrupted, nullptr });
	}
	return true;
}

bool UVeyraMovementComponent::IsFleeing() const
{
	return ForcedMove.IsSet() && ForcedMove->Mode == EVeyraCustomMovementMode::Fleeing;
}

bool UVeyraMovementComponent::StartDash(const FVeyraDash& Dash)
{
	if (!IsForcedMoveValid(Dash.Direction, Dash.Distance, Dash.Speed) || !UpdatedComponent || IsMovementLocked())
	{
		return false;
	}
	FForcedMove Move;
	Move.Mode = EVeyraCustomMovementMode::Dashing;
	Move.Destination = ResolveForcedMoveEnd(Dash.Direction, Dash.Distance);
	Move.Speed = Dash.Speed;
	Move.Contact = Dash.Contact;
	BeginForcedMove(Move);
	return true;
}

FVector UVeyraMovementComponent::ResolveForcedMoveEnd(const FVector& Direction, double Distance) const
{
	const FVector Start = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
	const FVector Heading = Direction.GetSafeNormal2D();
	const UWorld* World = GetWorld();
	const UCapsuleComponent* Capsule = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
	if (!World || !Capsule || Heading.IsNearlyZero() || !(Distance > 0.0))
	{
		return Start;
	}

	// Terrain stops the path where the body last fits (§9). The body is swept raised by what it could
	// step up, so the floor and small steps do not stop it; only static geometry does, not units.
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float Lift = FMath::Min(MaxStepHeight, 2.0f * (HalfHeight - Radius));
	const FVector Raise(0.0, 0.0, Lift / 2.0f);
	const FVector Wanted = Start + Heading * Distance;
	FVector End = Wanted;
	FHitResult Terrain;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(VeyraForcedMove), /*bTraceComplex*/ false, CharacterOwner);
	if (World->SweepSingleByObjectType(Terrain, Start + Raise, Wanted + Raise, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic),
		FCollisionShape::MakeCapsule(Radius, HalfHeight - Lift / 2.0f), Params))
	{
		End = Terrain.bStartPenetrating ? Start : Terrain.Location - Raise;
	}

	// Where the world has navigation, the path also stops where walkable ground does, as at a map's
	// edge or a cliff, and ends on the nearest walkable point.
	const UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	const ANavigationData* NavData = Navigation ? Navigation->GetDefaultNavDataInstance() : nullptr;
	if (NavData)
	{
		const FVector ToFeet(0.0, 0.0, HalfHeight);
		FVector LastWalkable;
		if (UNavigationSystemV1::NavigationRaycast(GetWorld(), Start - ToFeet, End - ToFeet, LastWalkable))
		{
			End = FVector(LastWalkable.X, LastWalkable.Y, End.Z);
		}
		const double Extent = UVeyraCombatTuningSubsystem::Get().ForcedMovement.NavigationExtent;
		FNavLocation Walkable;
		if (!Navigation->ProjectPointToNavigation(End - ToFeet, Walkable, FVector(Extent), NavData))
		{
			return Start;
		}
		End = FVector(Walkable.Location.X, Walkable.Location.Y, End.Z);
	}
	return End;
}

bool UVeyraMovementComponent::IsDisplaced() const
{
	return ForcedMove.IsSet() && ForcedMove->Mode == EVeyraCustomMovementMode::Displaced;
}

bool UVeyraMovementComponent::IsDashing() const
{
	return ForcedMove.IsSet() && ForcedMove->Mode == EVeyraCustomMovementMode::Dashing;
}

TOptional<FVector> UVeyraMovementComponent::GetForcedMoveDestination() const
{
	return ForcedMove.IsSet() ? TOptional<FVector>(ForcedMove->Destination) : TOptional<FVector>();
}

void UVeyraMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	BindCombatant(nullptr);
	Super::EndPlay(EndPlayReason);
}

void UVeyraMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	if (!ForcedMove.IsSet() || !UpdatedComponent)
	{
		SetMovementMode(MOVE_Walking);
		return;
	}
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	// The path was cleared of terrain when it was planned, and forced movement passes through units,
	// so the body moves along it without sweeping.
	const FVector Current = UpdatedComponent->GetComponentLocation();
	const FVector ToEnd = ForcedMove->Destination - Current;
	const double Step = ForcedMove->Speed * DeltaTime;
	const bool bArrives = ToEnd.Size() <= Step;
	FVector Next = bArrives ? ForcedMove->Destination : Current + ToEnd.GetSafeNormal() * Step;

	AActor* Contact = nullptr;
	if (ForcedMove->Mode == EVeyraCustomMovementMode::Dashing && ForcedMove->Contact == EVeyraDashContact::StopAtFirstEnemy)
	{
		Contact = FindEnemyContact(Current, Next, Next);
	}
	MoveUpdatedComponent(Next - Current, UpdatedComponent->GetComponentQuat(), /*bSweep*/ false);
	Velocity = (Next - Current) / DeltaTime;

	if (Contact)
	{
		EndDash(EVeyraDashEndReason::EnemyContact, Contact);
	}
	else if (bArrives && ForcedMove->Mode == EVeyraCustomMovementMode::Dashing)
	{
		EndDash(EVeyraDashEndReason::Arrived, nullptr);
	}
	else if (bArrives)
	{
		EndForcedMove();
	}
}

void UVeyraMovementComponent::BeginForcedMove(const FForcedMove& Move)
{
	ForcedMove = Move;
	SetMovementMode(MOVE_Custom, static_cast<uint8>(Move.Mode));
	RefreshMovementLock();
}

void UVeyraMovementComponent::EndForcedMove()
{
	ForcedMove.Reset();
	Velocity = FVector::ZeroVector;
	SetMovementMode(MOVE_Walking);
	RefreshMovementLock();
}

void UVeyraMovementComponent::EndDash(EVeyraDashEndReason Reason, AActor* Contact)
{
	EndForcedMove();
	OnDashEnded.Broadcast(FVeyraDashEnd{ Reason, Contact });
}

AActor* UVeyraMovementComponent::FindEnemyContact(const FVector& From, const FVector& To, FVector& OutContactLocation) const
{
	const UWorld* World = GetWorld();
	const UCapsuleComponent* Capsule = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
	if (!World || !Capsule)
	{
		return nullptr;
	}
	TArray<FHitResult> Hits;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(VeyraDashContact), /*bTraceComplex*/ false, CharacterOwner);
	World->SweepMultiByObjectType(Hits, From, To, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Params);
	Hits.Sort([](const FHitResult& A, const FHitResult& B) { return A.Time < B.Time; });
	for (const FHitResult& Hit : Hits)
	{
		AActor* Unit = Hit.GetActor();
		if (Unit && VeyraTargeting::IsAlive(Unit) && VeyraTargeting::AreHostile(CharacterOwner, Unit))
		{
			OutContactLocation = Hit.bStartPenetrating ? From : Hit.Location;
			return Unit;
		}
	}
	return nullptr;
}

void UVeyraMovementComponent::SetCastLocksMovement(bool bLocks)
{
	bCastLocksMovement = bLocks;
	RefreshMovementLock();
}

void UVeyraMovementComponent::RefreshMovementLock()
{
	const UVeyraStatusComponent* Statuses = FollowedStatuses.Get();
	const bool bStunned = Statuses && EnumHasAnyFlags(Statuses->GetActionBlocks(), EVeyraActionBlocks::Move);
	const bool bLocked = bStunned || bCastLocksMovement || ForcedMove.IsSet();
	if (bLocked == bMovementLocked)
	{
		return;
	}
	bMovementLocked = bLocked;
	if (bLocked && !ForcedMove.IsSet())
	{
		// A stunned or casting unit stops where it stands rather than braking to a halt (Combat Bible
		// §8, §48). Its path is its controller's to keep or drop, so stopping here leaves the path alone.
		StopMovementKeepPathing();
	}
	OnMovementLockChanged.Broadcast(bLocked);
}
