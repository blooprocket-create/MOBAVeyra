// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Content/VeyraContentId.h"
#include "VeyraAbilityTypes.h"

#include "VeyraCastStateComponent.generated.h"

/** The cast that holds a caster now, as every machine sees it for telegraphs and the HUD. */
USTRUCT()
struct FVeyraCastState
{
	GENERATED_BODY()

	UPROPERTY()
	FVeyraContentId Ability;

	/** The server's unique ID for this cast (Combat Bible §45). */
	UPROPERTY()
	int32 CastId = 0;

	UPROPERTY()
	EVeyraCastPhase Phase = EVeyraCastPhase::None;

	/** When the phase ends, in the server's world time. */
	UPROPERTY()
	double PhaseEndsAt = 0.0;

	/** Where the cast is aimed: its ground point, or the caster's position. */
	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	/** The direction the cast faces, on the ground. */
	UPROPERTY()
	FVector Direction = FVector::ForwardVector;
};

/**
 * The caster's latest committed cast, as every machine that sees the caster knows it (ADR-063 §1). A cast that
 * commits with no channel or recovery, or with no windup, changes no other state a client sees.
 */
USTRUCT()
struct FVeyraCastCommit
{
	GENERATED_BODY()

	/** Counts the caster's commits; each commit changes it. */
	UPROPERTY()
	int32 Serial = 0;

	UPROPERTY()
	FVeyraContentId Ability;

	/** Where it was aimed: its ground point, or the caster's position. */
	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	/** The direction it faced, on the ground. */
	UPROPERTY()
	FVector Direction = FVector::ForwardVector;
};

/**
 * Where the caster's latest cast projectile ended, as every machine that sees the caster knows it (ADR-072 §4): the
 * server's own end point, so a client shows an impact where the flight ended rather than where it last drew it.
 * Presentation reads it; nothing in gameplay does.
 */
USTRUCT()
struct FVeyraProjectileEnd
{
	GENERATED_BODY()

	/** Counts the caster's projectile ends; each end changes it. */
	UPROPERTY()
	int32 Serial = 0;

	UPROPERTY()
	FVeyraContentId Ability;

	/** The cast that launched it (FVeyraCastState::CastId). */
	UPROPERTY()
	int32 CastId = 0;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;
};

/**
 * The cast that holds a combatant, if any: its windup, channel or recovery (ADR-008 §4). It lives
 * beside the Ability System Component; a participant's replicates behind the fog, to those who see
 * it (ADR-016 §3).
 */
UCLASS(ClassGroup = Abilities)
class VEYRAABILITIES_API UVeyraCastStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVeyraCastStateComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** Behind the fog on a participant: its own, its teammates' and its observers' (ADR-016 §3). */
	virtual ELifetimeCondition GetReplicationCondition() const override;
	virtual void ReadyForReplication() override;

	/** Server only. */
	void SetState(const FVeyraCastState& NewState);

	/** Server only: no cast holds the caster now. */
	void Clear();

	/** Server only: a cast of Ability, aimed at Location facing Direction, has committed. */
	void NoteCommitted(const FVeyraContentId& Ability, const FVector& Location, const FVector& Direction);

	const FVeyraCastState& GetState() const { return State; }

	/** The latest committed cast; its Serial is 0 before the first. */
	const FVeyraCastCommit& GetLastCommit() const { return LastCommit; }

	/** Server only: a projectile of Ability's cast CastId has ended at Location (ADR-072 §4). */
	void NoteProjectileEnded(const FVeyraContentId& Ability, int32 CastId, const FVector& Location);

	/** Where the caster's latest cast projectile ended; its Serial is 0 before the first. */
	const FVeyraProjectileEnd& GetLastProjectileEnd() const { return LastProjectileEnd; }

	/** Whether a cast holds the caster, so another cast is refused. */
	bool IsBusy() const { return State.Phase != EVeyraCastPhase::None; }

	/** Raised on every machine when the state changes. */
	TMulticastDelegate<void()> OnCastStateChanged;

private:
	UFUNCTION()
	void OnRep_State();

	UPROPERTY(ReplicatedUsing = OnRep_State)
	FVeyraCastState State;

	UPROPERTY(Replicated)
	FVeyraCastCommit LastCommit;

	UPROPERTY(Replicated)
	FVeyraProjectileEnd LastProjectileEnd;
};
