// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Abilities/GameplayAbility.h"
#include "Content/VeyraContentId.h"
#include "Engine/TimerHandle.h"
#include "Misc/Optional.h"
#include "VeyraAbilityTypes.h"

#include "VeyraGameplayAbility.generated.h"

class UAbilitySystemComponent;
struct FVeyraCastTuning;

/** One cast in progress, as an archetype delivers it. */
struct FVeyraCast
{
	FVeyraContentId Ability;

	/** The slot's rank when the cast began; at least 1. */
	int32 Rank = 0;

	/** The server's unique ID for the cast (Combat Bible §45). */
	int32 CastId = 0;

	TWeakObjectPtr<UAbilitySystemComponent> Caster;

	/** The unit the cast was aimed at, if any. */
	TWeakObjectPtr<AActor> TargetActor;

	/** Where the caster stood when the cast began. */
	FVector CasterLocation = FVector::ZeroVector;

	/** The cast's ground point, brought within its range; the caster's position when it has none. */
	FVector Point = FVector::ZeroVector;

	/** From the caster toward the point, on the ground; the caster's facing when they coincide. */
	FVector Direction = FVector::ForwardVector;
};

/** How long an archetype channels a delivered cast: Ticks deliveries spread over Seconds. */
struct FVeyraChannelPlan
{
	int32 Ticks = 0;
	double Seconds = 0.0;
};

/**
 * The base of every Veyra ability: an archetype whose numbers come from tuning by content ID, run
 * only on the server (ADR-006 §7: no client prediction). It owns the rules every ability shares:
 * one validator for casting; the slot's rank from Progression; the cast's phases, windup, Commit,
 * delivery, channel and recovery, on world time (ADR-008 §4); the Commit point that pays the cost
 * and starts the cooldown (Combat Bible §26); interruption, which before Commit costs nothing and
 * starts part of the cooldown; costs from the combatant's resource (§27); and cooldowns from the
 * Veyra ledger (ADR-006 §4). An archetype adds its target rules and its delivery.
 */
UCLASS(Abstract)
class VEYRAABILITIES_API UVeyraGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UVeyraGameplayAbility();

	/**
	 * Why Caster may not cast this archetype, as content Ability, at Target now; None if it may. The
	 * one validator: VeyraAbilities::TryCast asks it before activating.
	 */
	EVeyraCastRejection CheckCast(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const;

protected:
	/** Whether this archetype's tuning defines Ability. */
	virtual bool Defines(const FVeyraContentId& Ability) const PURE_VIRTUAL(UVeyraGameplayAbility::Defines, return false;);

	/** The resource Ability costs at Commit at Rank. */
	virtual double GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const PURE_VIRTUAL(UVeyraGameplayAbility::GetResourceCost, return 0.0;);

	/** The cooldown Ability starts at Commit at Rank. */
	virtual double GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const PURE_VIRTUAL(UVeyraGameplayAbility::GetCooldownSeconds, return 0.0;);

	/** The archetype's own target rules for Ability; None when Target is acceptable. */
	virtual EVeyraCastRejection CheckTarget(const AActor& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const;

	/**
	 * Ability's cast timing. An archetype that returns one runs through the phases below; one that
	 * returns null activates on its own.
	 */
	virtual const FVeyraCastTuning* GetCastTuning(const FVeyraContentId& Ability) const;

	/** Whether casting Ability again now would end its lasting effect early instead (ADR-008 §9). */
	virtual bool EndsEarlyOnRecast(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability) const;

	/** Ends Ability's lasting effect early, for a recast that does so. */
	virtual void EndEarly(UAbilitySystemComponent& Caster, const FVeyraContentId& Ability);

	/** Delivers the cast at Commit, and says how long to channel it; no channel by default. */
	virtual FVeyraChannelPlan Deliver(const FVeyraCast& Cast);

	/** Whether Ability has an effect on enemies: casting it ends the caster's stealth (Combat Bible §11; ADR-018 §3). */
	virtual bool IsOffensive(const FVeyraContentId& Ability) const;

	/** Server: Caster began a cast of Ability. It is announced, and an offensive one ends Camouflage (ADR-018 §3, §4). */
	void NoteCastStarted(UAbilitySystemComponent& Caster, const FVeyraContentId& Ability) const;

	/**
	 * Server: Caster's cast of Ability committed. It is announced; a used-once override of it ends, and
	 * its recast window, if it has one, opens in its slot (ADR-018 §1, §3).
	 */
	void NoteCastCommitted(UAbilitySystemComponent& Caster, const FVeyraContentId& Ability) const;

	/** Delivers one tick of a channel; Tick counts from 1. */
	virtual void DeliverChannelTick(const FVeyraCast& Cast, int32 Tick);

	/** Whether Target carries a ground point that can be used: present and finite. */
	static bool HasUsablePoint(const FVeyraCastTarget& Target);

	/** Ability's rank for Caster: its slot's rank in Progression, 0 when not learned. */
	int32 GetRank(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability) const;

	/** Caster's Level in Progression, which Level-scaled amounts read (ADR-015 §3); 1 for a unit without one. */
	static int32 GetCasterLevel(const UAbilitySystemComponent& Caster);

	/** The content this spec runs, from the combatant's loadout. */
	FVeyraContentId GetContentId(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void GetCooldownTimeRemainingAndDuration(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		float& TimeRemaining, float& CooldownDuration) const override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

private:
	/** The activation a phased cast is running, for its timers. */
	struct FRunningCast
	{
		FVeyraCast Cast;
		FGameplayAbilitySpecHandle Handle;
		const FGameplayAbilityActorInfo* ActorInfo = nullptr;
		FGameplayAbilityActivationInfo ActivationInfo;
		EVeyraCastPhase Phase = EVeyraCastPhase::None;
		FVeyraChannelPlan Channel;
		int32 ChannelTicksDelivered = 0;
	};

	void OnWindupEnded();
	void OnChannelTick();
	void BeginRecovery();
	void OnCasterInterrupted();
	void EnterPhase(EVeyraCastPhase Phase, double Seconds);
	void FinishCast(bool bCancelled);

	/**
	 * The rank Commit charges and cools down at: a running cast's, read when it began (ADR-008 §6), so
	 * a rank taken during its windup changes neither; otherwise the slot's rank now.
	 */
	int32 GetCommitRank(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability) const;

	TOptional<FRunningCast> Running;
	FTimerHandle PhaseTimer;
	FDelegateHandle InterruptedHandle;
};
