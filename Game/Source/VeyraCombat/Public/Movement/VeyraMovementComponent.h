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
	};

	void BeginForcedMove(const FForcedMove& Move);
	void EndForcedMove();
	void EndDash(EVeyraDashEndReason Reason, AActor* Contact);

	/** The first living enemy unit the body touches moving from From to To, and where it touches. */
	AActor* FindEnemyContact(const FVector& From, const FVector& To, FVector& OutContactLocation) const;

	void RefreshMovementLock();

	/** Its statuses changed: its lock, and the body they shape (Ghosted, BodyScale; ADR-018 §2). */
	void OnFollowedStatusesChanged();
	void RefreshBody();

	/** The body as it was before any status shaped it; unset until the first refresh. */
	TOptional<float> BaseCapsuleRadius;
	TOptional<ECollisionResponse> BasePawnResponse;

	TWeakObjectPtr<UAbilitySystemComponent> FollowedCombatant;
	TWeakObjectPtr<UVeyraStatusComponent> FollowedStatuses;
	FDelegateHandle StatusesChangedHandle;
	TOptional<FForcedMove> ForcedMove;
	bool bCastLocksMovement = false;
	bool bMovementLocked = false;
};
