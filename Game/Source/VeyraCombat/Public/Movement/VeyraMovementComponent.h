// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Delegates/IDelegateInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/Optional.h"
#include "Movement/VeyraForcedMovementTypes.h"
#include "Movement/VeyraMotion.h"
#include "UObject/WeakObjectPtr.h"

#include "VeyraMovementComponent.generated.h"

class UAbilitySystemComponent;
class UVeyraStatusComponent;
enum class EVeyraOwnMove : uint8;

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
 * owns its movement, so its controller can hold them. Its units start and stop at once: they walk at their
 * speed from the moment they have somewhere to go (ADR-079 §6).
 *
 * The server tells every machine the body's motion rather than its place (ADR-079): where it began, when, and the corners
 * it walks at its speed. A machine that only shows the body draws it from that motion on the server's clock every frame,
 * so it moves smoothly however rarely the server speaks, and lands, stops and turns where the server's own does. The
 * player's own machine draws an order it has just given at once, and its body ahead by what the order waited.
 */
UCLASS(ClassGroup = Combat)
class VEYRACOMBAT_API UVeyraMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UVeyraMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Server only: follows Combatant's speed and statuses; nullptr lets go. */
	void BindCombatant(UAbilitySystemComponent* Combatant);

	/** The body's motion as the server last told it (ADR-079 §1). */
	const FVeyraMotion& GetMotion() const { return Motion; }

	/** Whether this machine has been told the body's motion; the server always has. */
	bool HasMotion() const { return bHasMotion; }

	/**
	 * Server: the player's order the body follows now, 0 for none. It is told with the body's motion, so the player's own
	 * machine knows which order a motion answers (ADR-079 §5).
	 */
	void SetMotionOrderNumber(uint16 OrderNumber);

	/**
	 * The player's own machine, for a body it commands: draws Predicted, an order the player has just given (its
	 * OrderNumber, begun now on the server's clock), until the server's motion answers that order or a later one, the
	 * order is refused, or the tuning's prediction time runs out (ADR-079 §5). Presentation only: nothing reaches the server.
	 */
	void PredictMotion(const FVeyraMotion& Predicted);

	/** The player's own machine: the server refused OrderNumber, so a prediction of it ends. */
	void ForgetPrediction(uint16 OrderNumber);

	/** A machine that only shows the body: the motion it is drawn by now, a prediction while one stands. */
	const FVeyraMotion& GetDrawnMotion() const;

	/** A machine that only shows the body: how many seconds ahead of its server motion it is drawn (ADR-079 §5). */
	double GetDrawnLeadSeconds() const { return LeadSeconds; }

	virtual void SimulatedTick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual float GetMaxSpeed() const override;

	/**
	 * Server: the share of its speed the unit keeps while its basic attack winds up, as a mobile
	 * attacker does (ADR-027 §1); unset while it winds up standing, or does not attack.
	 */
	void SetWindupSpeedShare(TOptional<double> Share) { WindupSpeedShare = Share; }

	/**
	 * Server only: displaces the unit Distance units along Direction at Speed. The caller has applied
	 * Displacement Resistance. A newer displacement replaces what is left of an older one, and
	 * interrupts a dash (Combat Bible §9). Returns false if refused.
	 */
	bool StartDisplacement(const FVector& Direction, double Distance, double Speed);

	/**
	 * Server only: as StartDisplacement, and one that collides stops there and gives the unit
	 * CollisionStatuses from Source (ADR-028 §3): terrain or the end of walkable ground shortens its
	 * path, or its body meets another living Vanguard or a structure on the way. The source's own body
	 * and units it already touches do not stop it.
	 */
	bool StartDisplacement(const FVector& Direction, double Distance, double Speed, UAbilitySystemComponent& Source, TArray<FVeyraStatusSpec> CollisionStatuses);

	/** Server only: moves a forced move on by DeltaTime; its physics calls it each step, and tests may. */
	void AdvanceForcedMove(float DeltaTime);

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

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Server: the distance the body has moved itself since the last call, walking, riding or dashing,
	 * never displaced, fleeing or held on, as Raska's Momentum counts it (ADR-018 §7), and starts again.
	 */
	double TakeTravelled();

	/**
	 * Where a forced movement of Distance along Direction from the unit's position ends: terrain stops
	 * it at the nearest point the body fits (Combat Bible §9), and so does the end of walkable ground;
	 * the end then moves to the nearest walkable point within the tuned extent. With none that close,
	 * it ends where it starts.
	 */
	FVector ResolveForcedMoveEnd(const FVector& Direction, double Distance, bool* bOutStopped = nullptr) const;

	bool IsDisplaced() const;
	bool IsDashing() const;

	/**
	 * Server: moves the body at once to the nearest walkable point at Destination, facing Facing unless it
	 * is zero (Combat Bible §9; ADR-030 §4). A dash under way ends, and so does the body's move. False,
	 * and no move, while displaced, fleeing or held on, or where no walkable point is near.
	 */
	bool Blink(const FVector& Destination, const FVector& Facing);

	/** Where the displacement or dash under way ends. */
	TOptional<FVector> GetForcedMoveDestination() const;

	/** Server: how many seconds the displacement, dash or Fear under way has left to run; 0 for none, or a hold. */
	double GetForcedMoveSecondsLeft() const;

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

	/**
	 * Server only: raised when the body is set down somewhere else at once (a blink). Its controller keeps the order
	 * it was following and finds a new path to it from here.
	 */
	FSimpleMulticastDelegate OnRelocated;

	/** Server only: whether the body is being set down somewhere else now, so a path stopped for it keeps its order. */
	bool IsRelocating() const { return bRelocating; }

	/** Server only: raised when a dash ends, and why. */
	TMulticastDelegate<void(const FVeyraDashEnd&)> OnDashEnded;

	/** Server only: raised when it lets go of its host, and why. */
	TMulticastDelegate<void(const FVeyraAttachEnd&)> OnAttachEnded;

	/** Server only: raised as a ride ends, and why. */
	TMulticastDelegate<void(const FVeyraRideEnd&)> OnRideEnded;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;

private:
	struct FForcedMove
	{
		EVeyraCustomMovementMode Mode = EVeyraCustomMovementMode::Displaced;
		FVector Destination = FVector::ZeroVector;
		double Speed = 0.0;
		EVeyraDashContact Contact = EVeyraDashContact::None;
		/** Where a dash began, for the unit's own move as it ends (ADR-032 §1). */
		FVector Origin = FVector::ZeroVector;
		/** An attach's host, and when it lets go (world seconds). */
		TWeakObjectPtr<AActor> Host;
		double EndsAt = 0.0;
		/** A displacement that collides: whose, what it gives, and whether terrain shortened its path. */
		TWeakObjectPtr<UAbilitySystemComponent> CollisionSource;
		TArray<FVeyraStatusSpec> CollisionStatuses;
		bool bMeetsTerrain = false;
	};

	/** Set while a blink stops its controller's path, so the controller keeps the order it was following. */
	bool bRelocating = false;

	/** The body's motion, as the server last told it (ADR-079 §1). */
	UPROPERTY(ReplicatedUsing = OnRep_Motion)
	FVeyraMotion Motion;

	UFUNCTION()
	void OnRep_Motion();

	/** Server: what the body is doing now, as a motion begun at Now. */
	FVeyraMotion DescribeMotion(double Now) const;

	/** Server: the path its controller follows now, to know it by; nullptr while it follows none. */
	const void* GetFollowedPath() const;

	/** Server: the corners its controller's path still walks from From; false while it follows none. */
	bool GetPathCornersAhead(const FVector& From, TArray<FVector>& OutCorners) const;

	/** Server: tells every machine the body's motion afresh when what it does strays from what was told (ADR-079 §2). */
	void PublishMotion();

	/** Server: the order told with each motion, and whether the body was set down elsewhere since the last. */
	uint16 MotionOrderNumber = 0;
	bool bSetDownPending = false;
	bool bHasMotion = false;
	/** Server: the path the told motion walked, so a new path is told at once. */
	const void* ToldPath = nullptr;

	/** A machine that only shows the body: draws it from its motion this frame (ADR-079 §3). */
	void DrawMotion(float DeltaSeconds);

	/** Where the drawn motion has the body at the server's Now, before any correction, and its velocity there. */
	FVector DrawnTargetAt(double Now, FVector& OutVelocity) const;

	/** The motion it is drawn by changed: the drawn body eases from where it was, Before, onto the new one. */
	void Rebase(const FVector& Before, bool bSetDown);

	/** The player's own machine: the order it predicts, and when the prediction lapses (server seconds). */
	TOptional<FVeyraMotion> Predicted;
	double PredictedUntil = 0.0;

	/** The order whose answer the body is drawn ahead of, and by how many seconds. */
	uint16 LeadOrder = 0;
	double LeadSeconds = 0.0;

	/** The drawn body's ease onto its latest motion: the offset it began from, and how long it has eased. */
	FVector2D CorrectionFrom = FVector2D::ZeroVector;
	double CorrectionAge = 0.0;
	uint8 DrawnSetDownSerial = 0;

	void BeginForcedMove(const FForcedMove& Move);

	/** An attach's step: the body takes its seat at the host's back, or lets go. */
	void PhysAttached(float DeltaTime);
	FVector AttachSeat(const AActor& Host) const;
	void EndForcedMove();
	void EndDash(EVeyraDashEndReason Reason, AActor* Contact);

	/** Announces the unit's own move, from From to where it stands now (ADR-032 §1). */
	void AnnounceOwnMove(EVeyraOwnMove Move, const FVector& From) const;

	/** Bends a forced move of Distance along Direction through the movement fields of the unit's enemies (ADR-033 §5). */
	void BendThroughFields(FVector& Direction, double& Distance) const;

	/** The first living enemy unit the body touches moving from From to To, and where it touches. */
	AActor* FindEnemyContact(const FVector& From, const FVector& To, FVector& OutContactLocation) const;

	/**
	 * The first living Vanguard or structure, other than the body and Ignored, the body meets moving from
	 * From to To, and where it meets it; one it already touches is passed through.
	 */
	AActor* FindCollision(const FVector& From, const FVector& To, const AActor* Ignored, FVector& OutContactLocation) const;

	/** A colliding displacement ends where it is, and gives its statuses. */
	void Collide();

	void RefreshMovementLock();

	/** Its statuses changed: its lock, and the body they shape (Ghosted, BodyScale; ADR-018 §2). */
	void OnFollowedStatusesChanged();
	void RefreshBody();

	/**
	 * While it passes through units, its response to each unit channel before (VeyraUnitCollision::Channels,
	 * in order); unset otherwise.
	 */
	TOptional<TArray<ECollisionResponse, TInlineAllocator<3>>> PassThroughFrom;

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

	TOptional<double> WindupSpeedShare;

	/** How far it has moved itself since TakeTravelled last read it. */
	double Travelled = 0.0;

	TWeakObjectPtr<UAbilitySystemComponent> FollowedCombatant;
	TWeakObjectPtr<UVeyraStatusComponent> FollowedStatuses;
	FDelegateHandle StatusesChangedHandle;
	TOptional<FForcedMove> ForcedMove;
	bool bCastLocksMovement = false;
	bool bMovementLocked = false;
};
