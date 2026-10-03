// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Movement/VeyraMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/VeyraMobilitySet.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Movement/VeyraMovementFields.h"
#include "Movement/VeyraMovementRules.h"
#include "Movement/VeyraUnitCollision.h"
#include "NavigationSystem.h"
#include "Shapes/VeyraShapes.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraCombatVerbs.h"

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
	// It changes only what it shaped itself, as that changes: whoever sized the body or set its
	// collision, such as the character from its data, keeps what they set.
	const UVeyraStatusComponent* Statuses = FollowedStatuses.Get();
	// Ghosted, it passes through units, never terrain (Combat Bible §24); so does a body holding on to
	// another, and a rider (§56).
	const bool bPassesThrough = (Statuses && Statuses->Has(EVeyraStatusKind::Ghosted)) || IsAttached() || IsRiding();
	if (bPassesThrough != PassThroughFrom.IsSet())
	{
		// Units sit on a side's channel or the pawn channel (ADR-062 §1): it passes through all of them.
		if (bPassesThrough)
		{
			TArray<ECollisionResponse, TInlineAllocator<3>> Before;
			for (const ECollisionChannel Channel : VeyraUnitCollision::Channels)
			{
				Before.Add(Capsule->GetCollisionResponseToChannel(Channel));
			}
			PassThroughFrom = MoveTemp(Before);
			VeyraUnitCollision::SetResponseToUnits(*Capsule, ECR_Ignore);
		}
		else
		{
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(VeyraUnitCollision::Channels); ++Index)
			{
				Capsule->SetCollisionResponseToChannel(VeyraUnitCollision::Channels[Index], PassThroughFrom.GetValue()[Index]);
			}
			PassThroughFrom.Reset();
		}
	}
	// Its body is wider or narrower for its hits while a BodyScale lasts (§13).
	const double Scale = Statuses && Statuses->Has(EVeyraStatusKind::BodyScale) ? Statuses->GetStrongest(EVeyraStatusKind::BodyScale) : 1.0;
	if (Scale != AppliedBodyScale)
	{
		const double Unscaled = Capsule->GetUnscaledCapsuleRadius() / AppliedBodyScale;
		Capsule->SetCapsuleRadius(static_cast<float>(Unscaled * Scale), /*bUpdateOverlaps*/ true);
		AppliedBodyScale = Scale;
	}
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
	// A rider's speed is set (§56); after the ride it slows back to its ordinary speed across the decay window.
	if (IsRiding())
	{
		Inputs.SetSpeed = Ride->SetSpeed;
	}
	else if (const UWorld* World = GetWorld(); World && RideDecay.IsSet() && RideDecay->Seconds > 0.0)
	{
		const double Alpha = (World->GetTimeSeconds() - RideDecay->StartedAt) / RideDecay->Seconds;
		if (Alpha < 1.0)
		{
			Inputs.SetSpeed = FMath::Max(Inputs.MoveSpeed, FMath::Lerp(RideDecay->FromSpeed, Inputs.MoveSpeed, Alpha));
		}
	}
	const double Speed = VeyraMovementRules::EffectiveSpeed(Inputs, UVeyraCombatTuningSubsystem::Get().Movement);
	// A mobile attacker walks through its windup at its share of that speed (ADR-027 §1).
	return static_cast<float>(WindupSpeedShare.IsSet() ? Speed * WindupSpeedShare.GetValue() : Speed);
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
	FVector Heading = Direction;
	double Reach = Distance;
	BendThroughFields(Heading, Reach);
	FForcedMove Move;
	Move.Mode = EVeyraCustomMovementMode::Displaced;
	Move.Destination = ResolveForcedMoveEnd(Heading, Reach);
	Move.Speed = Speed;
	BeginForcedMove(Move);
	if (bInterruptsDash)
	{
		OnDashEnded.Broadcast(FVeyraDashEnd{ EVeyraDashEndReason::Interrupted, nullptr });
	}
	return true;
}

bool UVeyraMovementComponent::StartDisplacement(const FVector& Direction, double Distance, double Speed, UAbilitySystemComponent& Source,
	TArray<FVeyraStatusSpec> CollisionStatuses)
{
	bool bStopped = false;
	FVector Heading = Direction;
	double Reach = Distance;
	BendThroughFields(Heading, Reach);
	ResolveForcedMoveEnd(Heading, Reach, &bStopped);
	if (!StartDisplacement(Direction, Distance, Speed))
	{
		return false;
	}
	if (!CollisionStatuses.IsEmpty())
	{
		ForcedMove->CollisionSource = &Source;
		ForcedMove->CollisionStatuses = MoveTemp(CollisionStatuses);
		ForcedMove->bMeetsTerrain = bStopped;
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

bool UVeyraMovementComponent::StartAttach(AActor& Host, double Seconds)
{
	const UWorld* World = GetWorld();
	const UVeyraStatusComponent* Statuses = FollowedStatuses.Get();
	const bool bHeld = Statuses && EnumHasAnyFlags(Statuses->GetActionBlocks(), EVeyraActionBlocks::Move);
	if (!World || !UpdatedComponent || &Host == GetOwner() || !(Seconds > 0.0) || !FMath::IsFinite(Seconds) || IsDisplaced() || bHeld)
	{
		return false;
	}
	const bool bInterruptsDash = IsDashing();
	FForcedMove Move;
	Move.Mode = EVeyraCustomMovementMode::Attached;
	Move.Destination = UpdatedComponent->GetComponentLocation();
	Move.Host = &Host;
	Move.EndsAt = World->GetTimeSeconds() + Seconds;
	BeginForcedMove(Move);
	if (bInterruptsDash)
	{
		OnDashEnded.Broadcast(FVeyraDashEnd{ EVeyraDashEndReason::Interrupted, nullptr });
	}
	// It takes its seat at once rather than on its next step.
	MoveUpdatedComponent(AttachSeat(Host) - UpdatedComponent->GetComponentLocation(), Host.GetActorQuat(), /*bSweep*/ false);
	return true;
}

void UVeyraMovementComponent::EndAttach(EVeyraAttachEndReason Reason)
{
	if (!IsAttached())
	{
		return;
	}
	const FVeyraAttachEnd End{ Reason, ForcedMove->Host };
	EndForcedMove();
	OnAttachEnded.Broadcast(End);
}

bool UVeyraMovementComponent::StartRide(const FVeyraRide& InRide)
{
	const bool bValid = InRide.SetSpeed > 0.0 && FMath::IsFinite(InRide.SetSpeed) && InRide.TurnRateDegreesPerSecond > 0.0
		&& FMath::IsFinite(InRide.TurnRateDegreesPerSecond) && InRide.DecaySeconds >= 0.0 && FMath::IsFinite(InRide.DecaySeconds);
	if (!bValid || !UpdatedComponent)
	{
		return false;
	}
	if (!IsRiding())
	{
		const FVector Moving = Velocity.GetSafeNormal2D();
		RideHeading = Moving.IsNearlyZero() ? GetOwner()->GetActorForwardVector().GetSafeNormal2D() : Moving;
	}
	Ride = InRide;
	RideDecay.Reset();
	RefreshBody();
	return true;
}

void UVeyraMovementComponent::EndRide(EVeyraRideEndReason Reason)
{
	if (!IsRiding())
	{
		return;
	}
	const FVeyraRideEnd End{ Reason, UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector, RideHeading };
	// It keeps its speed, slowing to its ordinary speed across the ride's window (§56, "Leaving").
	if (const UWorld* World = GetWorld(); World && Ride->DecaySeconds > 0.0)
	{
		RideDecay = FRideDecay{ Ride->SetSpeed, World->GetTimeSeconds(), Ride->DecaySeconds };
	}
	Ride.Reset();
	RefreshBody();
	OnRideEnded.Broadcast(End);
}

void UVeyraMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	const FVector Before = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
	// Its own movement: walking, riding or its own dash, never what moves it against its will.
	const bool bOwnMovement = !ForcedMove.IsSet() || ForcedMove->Mode == EVeyraCustomMovementMode::Dashing;
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (UpdatedComponent && bOwnMovement)
	{
		Travelled += FVector::Dist2D(Before, UpdatedComponent->GetComponentLocation());
	}
}

double UVeyraMovementComponent::TakeTravelled()
{
	const double Moved = Travelled;
	Travelled = 0.0;
	return Moved;
}

double UVeyraMovementComponent::GetRideTurnRadius() const
{
	return IsRiding() ? Ride->SetSpeed / FMath::DegreesToRadians(Ride->TurnRateDegreesPerSecond) : 0.0;
}

FVector UVeyraMovementComponent::GetRideHeading() const
{
	return IsRiding() ? RideHeading : GetOwner()->GetActorForwardVector().GetSafeNormal2D();
}

void UVeyraMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
	if (!IsRiding() || ForcedMove.IsSet() || DeltaTime <= 0.0f)
	{
		return;
	}
	const FVector Desired = Velocity.GetSafeNormal2D();
	if (Desired.IsNearlyZero())
	{
		return;
	}
	// Its heading turns no faster than the ride's rate, and it keeps its speed through the arc rather
	// than slowing to pivot (§56).
	const double MaxTurn = FMath::DegreesToRadians(Ride->TurnRateDegreesPerSecond) * DeltaTime;
	const double Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(RideHeading, Desired), -1.0, 1.0));
	if (Angle > MaxTurn)
	{
		const double Side = FVector::CrossProduct(RideHeading, Desired).Z >= 0.0 ? 1.0 : -1.0;
		RideHeading = RideHeading.RotateAngleAxis(FMath::RadiansToDegrees(MaxTurn) * Side, FVector::UpVector).GetSafeNormal2D();
	}
	else
	{
		RideHeading = Desired;
	}
	// At its set speed, less its Slows, from the moment it has somewhere to go: a vehicle does not wind up.
	Velocity = RideHeading * GetMaxSpeed() + FVector(0.0, 0.0, Velocity.Z);
}

bool UVeyraMovementComponent::IsAttached() const
{
	return ForcedMove.IsSet() && ForcedMove->Mode == EVeyraCustomMovementMode::Attached;
}

AActor* UVeyraMovementComponent::GetAttachHost() const
{
	return IsAttached() ? ForcedMove->Host.Get() : nullptr;
}

void UVeyraMovementComponent::PhysAttached(float DeltaTime)
{
	const AActor* Host = ForcedMove->Host.Get();
	const UWorld* World = GetWorld();
	if (!Host || !VeyraTargeting::IsAlive(Host))
	{
		EndAttach(EVeyraAttachEndReason::HostLost);
		return;
	}
	if (!VeyraTargeting::IsAlive(GetOwner()))
	{
		EndAttach(EVeyraAttachEndReason::Died);
		return;
	}
	if (!World || World->GetTimeSeconds() >= ForcedMove->EndsAt)
	{
		EndAttach(EVeyraAttachEndReason::Expired);
		return;
	}
	const FVector Current = UpdatedComponent->GetComponentLocation();
	const FVector Seat = AttachSeat(*Host);
	MoveUpdatedComponent(Seat - Current, Host->GetActorQuat(), /*bSweep*/ false);
	Velocity = (Seat - Current) / DeltaTime;
}

FVector UVeyraMovementComponent::AttachSeat(const AActor& Host) const
{
	// At the host's back, the two bodies touching, standing on the host's ground.
	const ACharacter* HostCharacter = Cast<ACharacter>(&Host);
	const UCapsuleComponent* HostCapsule = HostCharacter ? HostCharacter->GetCapsuleComponent() : nullptr;
	const UCapsuleComponent* Own = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
	const double HostRadius = HostCapsule ? HostCapsule->GetScaledCapsuleRadius() : 0.0;
	const double HostHalfHeight = HostCapsule ? HostCapsule->GetScaledCapsuleHalfHeight() : 0.0;
	const double OwnRadius = Own ? Own->GetScaledCapsuleRadius() : 0.0;
	const double OwnHalfHeight = Own ? Own->GetScaledCapsuleHalfHeight() : 0.0;
	FVector Back = -Host.GetActorForwardVector().GetSafeNormal2D();
	if (Back.IsNearlyZero())
	{
		Back = FVector::BackwardVector;
	}
	FVector Seat = Host.GetActorLocation() + Back * (HostRadius + OwnRadius);
	Seat.Z = Host.GetActorLocation().Z - HostHalfHeight + OwnHalfHeight;
	return Seat;
}

bool UVeyraMovementComponent::StartDash(const FVeyraDash& Dash)
{
	// One that takes over ends the dash under way first, where the unit is (ADR-031 §7).
	if (Dash.bTakesOver && IsDashing() && IsForcedMoveValid(Dash.Direction, Dash.Distance, Dash.Speed))
	{
		EndDash(EVeyraDashEndReason::Replaced, nullptr);
	}
	if (!IsForcedMoveValid(Dash.Direction, Dash.Distance, Dash.Speed) || !UpdatedComponent || IsMovementLocked())
	{
		return false;
	}
	FVector Heading = Dash.Direction;
	double Reach = Dash.Distance;
	BendThroughFields(Heading, Reach);
	FForcedMove Move;
	Move.Mode = EVeyraCustomMovementMode::Dashing;
	Move.Origin = UpdatedComponent->GetComponentLocation();
	Move.Destination = ResolveForcedMoveEnd(Heading, Reach);
	Move.Speed = Dash.Speed;
	Move.Contact = Dash.Contact;
	BeginForcedMove(Move);
	return true;
}

FVector UVeyraMovementComponent::ResolveForcedMoveEnd(const FVector& Direction, double Distance, bool* bOutStopped) const
{
	if (bOutStopped)
	{
		*bOutStopped = false;
	}
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
		if (bOutStopped)
		{
			*bOutStopped = true;
		}
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
			if (bOutStopped)
			{
				*bOutStopped = true;
			}
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

bool UVeyraMovementComponent::Blink(const FVector& Destination, const FVector& Facing)
{
	if (!CharacterOwner || !UpdatedComponent || IsDisplaced() || IsFleeing() || IsAttached())
	{
		return false;
	}
	const FVector From = UpdatedComponent->GetComponentLocation();
	// Terrain between does not stop a blink; its end must be ground the body may stand on (§9).
	FVector End(Destination.X, Destination.Y, UpdatedComponent->GetComponentLocation().Z);
	const UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const ANavigationData* NavData = Navigation ? Navigation->GetDefaultNavDataInstance() : nullptr;
	if (NavData)
	{
		const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
		const FVector ToFeet(0.0, 0.0, Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.0);
		const double Extent = UVeyraCombatTuningSubsystem::Get().ForcedMovement.NavigationExtent;
		FNavLocation Walkable;
		if (!Navigation->ProjectPointToNavigation(End - ToFeet, Walkable, FVector(Extent), NavData))
		{
			return false;
		}
		End = FVector(Walkable.Location.X, Walkable.Location.Y, End.Z);
	}
	const FRotator Rotation = Facing.IsNearlyZero() ? CharacterOwner->GetActorRotation() : Facing.GetSafeNormal2D().Rotation();
	if (!CharacterOwner->TeleportTo(End, Rotation))
	{
		return false;
	}
	// A blink takes over a dash under way, which lands nothing (ADR-031 §7).
	if (IsDashing())
	{
		EndDash(EVeyraDashEndReason::Replaced, nullptr);
	}
	// The move it was walking belongs to where it stood; its controller paths again from here.
	if (AController* Controller = CharacterOwner->GetController())
	{
		Controller->StopMovement();
	}
	StopMovementImmediately();
	AnnounceOwnMove(EVeyraOwnMove::Blink, From);
	return true;
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
	if (ForcedMove->Mode == EVeyraCustomMovementMode::Attached)
	{
		PhysAttached(DeltaTime);
		return;
	}
	AdvanceForcedMove(DeltaTime);
}

void UVeyraMovementComponent::AdvanceForcedMove(float DeltaTime)
{
	if (!ForcedMove.IsSet() || !UpdatedComponent || ForcedMove->Mode == EVeyraCustomMovementMode::Attached)
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
	// A displacement that collides stops at the first Vanguard or structure it meets (ADR-028 §3).
	bool bCollides = false;
	if (ForcedMove->Mode == EVeyraCustomMovementMode::Displaced && !ForcedMove->CollisionStatuses.IsEmpty())
	{
		const UAbilitySystemComponent* Source = ForcedMove->CollisionSource.Get();
		bCollides = FindCollision(Current, Next, Source ? Source->GetAvatarActor() : nullptr, Next) != nullptr
			|| (bArrives && ForcedMove->bMeetsTerrain);
	}
	MoveUpdatedComponent(Next - Current, UpdatedComponent->GetComponentQuat(), /*bSweep*/ false);
	Velocity = (Next - Current) / DeltaTime;

	if (bCollides)
	{
		Collide();
	}
	else if (Contact)
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
	// What takes over an attach ends it first, so the one who attached hears before the new move begins.
	if (IsAttached())
	{
		const FVeyraAttachEnd Replaced{ EVeyraAttachEndReason::Replaced, ForcedMove->Host };
		ForcedMove.Reset();
		OnAttachEnded.Broadcast(Replaced);
	}
	ForcedMove = Move;
	SetMovementMode(MOVE_Custom, static_cast<uint8>(Move.Mode));
	RefreshMovementLock();
	RefreshBody();
}

void UVeyraMovementComponent::EndForcedMove()
{
	ForcedMove.Reset();
	Velocity = FVector::ZeroVector;
	SetMovementMode(MOVE_Walking);
	RefreshMovementLock();
	RefreshBody();
}

void UVeyraMovementComponent::EndDash(EVeyraDashEndReason Reason, AActor* Contact)
{
	const FVector From = ForcedMove.IsSet() ? ForcedMove->Origin : FVector::ZeroVector;
	EndForcedMove();
	OnDashEnded.Broadcast(FVeyraDashEnd{ Reason, Contact });
	// A dash a displacement interrupts never ends as the unit's own; it does not reach here (ADR-032 §1).
	AnnounceOwnMove(EVeyraOwnMove::Dash, From);
}

void UVeyraMovementComponent::BendThroughFields(FVector& Direction, double& Distance) const
{
	const UWorld* World = GetWorld();
	const UVeyraMovementFieldSubsystem* Fields = World ? World->GetSubsystem<UVeyraMovementFieldSubsystem>() : nullptr;
	if (!Fields || Fields->GetFieldCount() == 0 || !UpdatedComponent)
	{
		return;
	}
	const FVector Start = UpdatedComponent->GetComponentLocation();
	const FVector Bent = Fields->Bend(VeyraTeams::TeamOf(CharacterOwner), Start, Start + Direction.GetSafeNormal2D() * Distance);
	const FVector Offset(Bent.X - Start.X, Bent.Y - Start.Y, 0.0);
	// A move bent onto its own start goes nowhere: it ends where it began.
	if (Offset.IsNearlyZero())
	{
		Distance = 0.0;
		return;
	}
	Direction = Offset.GetSafeNormal();
	Distance = Offset.Size();
}

void UVeyraMovementComponent::AnnounceOwnMove(EVeyraOwnMove Move, const FVector& From) const
{
	UWorld* World = GetWorld();
	UVeyraCombatEventSubsystem* Events = World ? World->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr;
	UAbilitySystemComponent* Unit = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CharacterOwner);
	if (Events && Unit && UpdatedComponent)
	{
		Events->OnUnitMoved.Broadcast(FVeyraUnitMovedEvent{ Unit, Move, From, UpdatedComponent->GetComponentLocation() });
	}
}

AActor* UVeyraMovementComponent::FindCollision(const FVector& From, const FVector& To, const AActor* Ignored, FVector& OutContactLocation) const
{
	const UWorld* World = GetWorld();
	const UCapsuleComponent* Capsule = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
	if (!World || !Capsule)
	{
		return nullptr;
	}
	TArray<FHitResult> Hits;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(VeyraDisplacementCollision), /*bTraceComplex*/ false, CharacterOwner);
	World->SweepMultiByObjectType(Hits, From, To, FQuat::Identity, VeyraUnitCollision::AllUnits(),
		FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Params);
	Hits.Sort([](const FHitResult& A, const FHitResult& B) { return A.Time < B.Time; });
	for (const FHitResult& Hit : Hits)
	{
		AActor* Unit = Hit.GetActor();
		if (Unit && Unit != Ignored && !Hit.bStartPenetrating && VeyraTargeting::IsAlive(Unit) && (VeyraUnits::IsVanguard(Unit) || VeyraUnits::IsStructure(Unit)))
		{
			OutContactLocation = Hit.Location;
			return Unit;
		}
	}
	return nullptr;
}

void UVeyraMovementComponent::Collide()
{
	UAbilitySystemComponent* Source = ForcedMove.IsSet() ? ForcedMove->CollisionSource.Get() : nullptr;
	const TArray<FVeyraStatusSpec> Statuses = ForcedMove.IsSet() ? ForcedMove->CollisionStatuses : TArray<FVeyraStatusSpec>();
	EndForcedMove();
	UAbilitySystemComponent* Unit = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CharacterOwner);
	if (!Source || !Unit)
	{
		return;
	}
	for (const FVeyraStatusSpec& Status : Statuses)
	{
		VeyraCombat::ApplyStatus(*Source, *Unit, Status);
	}
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
	World->SweepMultiByObjectType(Hits, From, To, FQuat::Identity, VeyraUnitCollision::AllUnits(),
		FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Params);
	Hits.Sort([](const FHitResult& A, const FHitResult& B) { return A.Time < B.Time; });
	for (const FHitResult& Hit : Hits)
	{
		AActor* Unit = Hit.GetActor();
		if (Unit && VeyraTargeting::IsAlive(Unit) && VeyraTargeting::CanHitEnemy(CharacterOwner, *Unit))
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
