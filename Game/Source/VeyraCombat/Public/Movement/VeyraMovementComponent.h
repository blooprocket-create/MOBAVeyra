// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Delegates/IDelegateInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/Optional.h"
#include "Movement/VeyraForcedMovementTypes.h"
#include "UObject/WeakObjectPtr.h"

#include "VeyraMovementComponent.generated.h"

class UAbilitySystemComponent;
class UVeyraStatusComponent;

/** The custom movement modes of a combatant's body (MOVE_Custom's sub-mode). */
UENUM()
enum class EVeyraCustomMovementMode : uint8
{
	/** Another source's Knockback or Pull owns the movement (Combat Bible §9). */
	Displaced,
	/** The unit's own dash owns it. */
	Dashing,
	/** A Fear owns it: the unit walks away from its source (Combat Bible §8). */
	Fleeing,
	/** It holds on to another unit's body and goes where it goes (ADR-018 §2), as Patch's Bear Hug. */
	Attached,
};

/**
 * A combatant body's movement (ADR-009 §2). On the server it walks at the effective Movement Speed
 * of the combatant it follows (VeyraMovementRules::EffectiveSpeed), and runs displacements and
 * dashes: each path is planned once, stopped by terrain and ended on walkable ground, and followed at
 * its own speed through other units. It reports when something other than the unit's own orders
 * owns its movement, so its controller can hold them. Clients only show the replicated result
 * (ADR-006 §7).
 */
UCLASS(ClassGroup = Combat)
class VEYRACOMBAT_API UVeyraMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	/** Server only: follows Combatant's speed and statuses; nullptr lets go. */
	void BindCombatant(UAbilitySystemComponent* Combatant);

	virtual float GetMaxSpeed() const override;

	/**
	 * Server only: displaces the unit Distance units along Direction at Speed. The caller has applied
	 * Displacement Resistance. A newer displacement replaces what is left of an older one, and
	 * interrupts a dash (Combat Bible §9). Returns false if refused.
	 */
	bool StartDisplacement(const FVector& Direction, double Distance, double Speed);

	/** Server only: starts a dash. Refused, returning false, while the unit's movement is locked. */
	bool StartDash(const FVeyraDash& Dash);

	/**
	 * Server: the unit flees Distance along Direction at Speed, as a Fear makes it, unless a
	 * displacement holds it; it ends a dash (Combat Bible §8). False if it cannot.
	 */
	bool StartFleeing(const FVector& Direction, double Distance, double Speed);

	/** Whether a Fear moves the unit now. */
	bool IsFleeing() const;

	/**
	 * Server only: the body holds on to Host's back for Seconds, following it, and passes through
	 * units meanwhile (ADR-018 §2). Refused, returning false, while a displacement holds it, while its
	 * statuses stop it moving, or for its own body. It ends when its time runs out, when either body
	 * dies, when a displacement, Fear or another attach takes over, or on EndAttach.
	 */
	bool StartAttach(AActor& Host, double Seconds);

	/** Server only: lets go of the host, for Reason, if it holds one. */
	void EndAttach(EVeyraAttachEndReason Reason);

	bool IsAttached() const;

	/** The body it holds on to now; nullptr if none. */
	AActor* GetAttachHost() const;

	/**
	 * Server only: the body rides (Combat Bible §56): its Movement Speed is Ride's set speed, its
	 * heading turns no faster than Ride's rate while it keeps its speed through the arc, and it passes
	 * through units. It stays under its own orders; displacement and crowd control apply as ever and
	 * never end it. A newer ride replaces an older. False for invalid values.
	 */
	bool StartRide(const FVeyraRide& Ride);

	/** Server only: the ride ends, for Reason; the body slows back to its ordinary speed across the ride's decay window. */
	void EndRide(EVeyraRideEndReason Reason);

	bool IsRiding() const { return Ride.IsSet(); }

	/** The tightest circle a rider can turn at its set speed, in units; 0 when not riding. */
	double GetRideTurnRadius() const;

	/** Which way the rider is heading; its facing when not riding. */
	FVector GetRideHeading() const;

	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;

	/**
	 * Where a forced movement of Distance along Direction from the unit's position ends: terrain stops
	 * it at the nearest point the body fits (Combat Bible §9), and so does the end of walkable ground;
	 * the end then moves to the nearest walkable point within the tuned extent. With none that close,
	 * it ends where it starts.
	 */
	FVector ResolveForcedMoveEnd(const FVector& Direction, double Distance) const;

	bool IsDisplaced() const;
	bool IsDashing() const;

	/** Where the displacement or dash under way ends. */
	TOptional<FVector> GetForcedMoveDestination() const;

	/**
	 * Server only: whether the unit's own cast holds it in place, as a windup or channel that locks
	 * movement does (Combat Bible §48). The body stops, and its orders wait, while it does.
	 */
	void SetCastLocksMovement(bool bLocks);

	/** Whether the unit cannot follow its orders now: it is stunned, displaced, dashing or casting in place. */
	bool IsMovementLocked() const { return bMovementLocked; }

	/**
	 * Whether the body moves toward a living enemy Vanguard, as the combat tuning's pursuit rule
	 * defines it (Combat Bible §23; ADR-008 §9). A move-speed bonus toward enemy Vanguards holds only then.
	 */
	bool IsMovingTowardEnemyVanguard() const;

	/** Server only: raised when IsMovementLocked changes, with its new value. */
	TMulticastDelegate<void(bool)> OnMovementLockChanged;

	/** Server only: raised when a dash ends, and why. */
	TMulticastDelegate<void(const FVeyraDashEnd&)> OnDashEnded;

	/** Server only: raised when it lets go of its host, and why. */
	TMulticastDelegate<void(const FVeyraAttachEnd&)> OnAttachEnded;

	/** Server only: raised as a ride ends, and why. */
	TMulticastDelegate<void(const FVeyraRideEnd&)> OnRideEnded;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;

private:
	struct FForcedMove
	{
		EVeyraCustomMovementMode Mode = EVeyraCustomMovementMode::Displaced;
		FVector Destination = FVector::ZeroVector;
		double Speed = 0.0;
		EVeyraDashContact Contact = EVeyraDashContact::None;
		/** An attach's host, and when it lets go (world seconds). */
		TWeakObjectPtr<AActor> Host;
		double EndsAt = 0.0;
	};

	void BeginForcedMove(const FForcedMove& Move);

	/** An attach's step: the body takes its seat at the host's back, or lets go. */
	void PhysAttached(float DeltaTime);
	FVector AttachSeat(const AActor& Host) const;
	void EndForcedMove();
	void EndDash(EVeyraDashEndReason Reason, AActor* Contact);

	/** The first living enemy unit the body touches moving from From to To, and where it touches. */
	AActor* FindEnemyContact(const FVector& From, const FVector& To, FVector& OutContactLocation) const;

	void RefreshMovementLock();

	/** Its statuses changed: its lock, and the body they shape (Ghosted, BodyScale; ADR-018 §2). */
	void OnFollowedStatusesChanged();
	void RefreshBody();

	/** While it passes through units, the response to them it had before; unset otherwise. */
	TOptional<ECollisionResponse> PassThroughFrom;

	/** The BodyScale its radius carries now; 1 for none. */
	double AppliedBodyScale = 1.0;

	/** The ride under way, and the heading its turns are measured from. */
	TOptional<FVeyraRide> Ride;
	FVector RideHeading = FVector::ForwardVector;

	/** After a ride: the speed it left at, when, and over how long it slows to its ordinary speed. */
	struct FRideDecay
	{
		double FromSpeed = 0.0;
		double StartedAt = 0.0;
		double Seconds = 0.0;
	};
	TOptional<FRideDecay> RideDecay;

	TWeakObjectPtr<UAbilitySystemComponent> FollowedCombatant;
	TWeakObjectPtr<UVeyraStatusComponent> FollowedStatuses;
	FDelegateHandle StatusesChangedHandle;
	TOptional<FForcedMove> ForcedMove;
	bool bCastLocksMovement = false;
	bool bMovementLocked = false;
};
