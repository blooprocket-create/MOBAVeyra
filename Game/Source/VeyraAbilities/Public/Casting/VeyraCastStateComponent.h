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

	const FVeyraCastState& GetState() const { return State; }

	/** Whether a cast holds the caster, so another cast is refused. */
	bool IsBusy() const { return State.Phase != EVeyraCastPhase::None; }

	/** Raised on every machine when the state changes. */
	TMulticastDelegate<void()> OnCastStateChanged;

private:
	UFUNCTION()
	void OnRep_State();

	UPROPERTY(ReplicatedUsing = OnRep_State)
	FVeyraCastState State;
};
