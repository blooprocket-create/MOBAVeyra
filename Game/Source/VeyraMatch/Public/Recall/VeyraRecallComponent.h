// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Delegates/Delegate.h"
#include "Engine/TimerHandle.h"

#include "VeyraRecallComponent.generated.h"

struct FVeyraDeathEvent;
struct FVeyraHostileDamageEvent;
class UAbilitySystemComponent;

/** A Recall channel as every machine sees it, on the server's world clock. */
USTRUCT()
struct FVeyraRecallChannel
{
	GENERATED_BODY()

	/** When the channel began; 0 when none runs. */
	UPROPERTY()
	double StartedAt = 0.0;

	/** When it completes; 0 when none runs. */
	UPROPERTY()
	double EndsAt = 0.0;
};

/**
 * One participant's Recall (Economy & Progression Bible §10; ADR-012 §8): a channel that brings its
 * living Vanguard home when it completes. It sits on the PlayerState beside the participant's combat
 * components. The game mode starts it, and a new order ends it early; the channel itself watches
 * Combat, so hostile damage, an interruption (a Stun or a displacement, Combat Bible §9) and death
 * end it too. Server only, except the replicated channel, which the HUD shows.
 */
UCLASS()
class VEYRAMATCH_API UVeyraRecallComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVeyraRecallComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** Behind the fog on a participant: its own, its teammates' and its observers' (ADR-016 §3). */
	virtual ELifetimeCondition GetReplicationCondition() const override;
	virtual void ReadyForReplication() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Server only: starts a channel of Seconds on world time, so a pause holds it. Complete runs when
	 * it ends uninterrupted. A running channel is replaced.
	 */
	void Start(double Seconds, FSimpleDelegate Complete);

	/** Server only: ends a running channel without completing it. Returns whether one ran. */
	bool Interrupt();

	/** Whether a channel runs, on every machine. */
	bool IsRecalling() const { return Channel.EndsAt > 0.0; }

	/** The running channel, on every machine; zeroes when none runs. */
	const FVeyraRecallChannel& GetChannel() const { return Channel; }

private:
	void OnChannelComplete();
	void SetChannel(double StartedAt, double EndsAt);

	/** Follows Combat while a channel runs, and lets go when it ends. */
	void Watch(bool bWatch);
	void OnHostileDamage(const FVeyraHostileDamageEvent& Event);
	void OnDeath(const FVeyraDeathEvent& Death);
	void OnInterrupted();
	const UAbilitySystemComponent* GetAbilitySystem() const;

	UPROPERTY(Replicated)
	FVeyraRecallChannel Channel;

	FTimerHandle Timer;
	FSimpleDelegate OnComplete;
	FDelegateHandle HostileDamageHandle;
	FDelegateHandle DeathHandle;
	FDelegateHandle InterruptedHandle;
};
