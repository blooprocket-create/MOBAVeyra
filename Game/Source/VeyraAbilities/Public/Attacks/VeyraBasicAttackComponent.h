// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Attacks/VeyraAttackSpeed.h"
#include "Attacks/VeyraBasicAttackTypes.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "VeyraCombatVerbs.h"

#include "VeyraBasicAttackComponent.generated.h"

class UAbilitySystemComponent;
struct FVeyraDeathEvent;

/**
 * A combatant's basic attacks (Combat Bible §4, §16, §17, §22, §48; ADR-009 §5), beside its Ability
 * System Component. Each attack winds up, commits, and then backswings, on the world clock, so a
 * pause holds it. The target must be a living enemy in range at the start and again at Commit.
 * Commit builds the attack, adds a pending empowerment and the modifiers' contributions, and
 * prepares its damage (§50); a melee attack lands at once, a ranged one sends a homing projectile.
 * It counts consecutive attacks on one enemy Vanguard (the hit chain) and announces On Attack and
 * On Hit for passives. Server only, apart from the replicated state presentation reads.
 */
UCLASS(ClassGroup = Abilities)
class VEYRAABILITIES_API UVeyraBasicAttackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVeyraBasicAttackComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: the unit's basic attacks from now on. Refused, returning false, if Profile is invalid. */
	bool SetProfile(const FVeyraBasicAttackProfile& InProfile);

	bool HasProfile() const { return bHasProfile; }
	const FVeyraBasicAttackProfile& GetProfile() const { return Profile; }

	/** Server: why an attack at Target could not start now; None if it could. */
	EVeyraAttackRejection CheckAttack(const AActor* Target) const;

	/** Server: begins an attack at Target: the windup, then Commit. */
	EVeyraAttackRejection StartAttack(AActor& Target);

	/**
	 * Server: moving, acting or an interruption ends the current attack. Before Commit it is cancelled
	 * and costs nothing; after Commit only the backswing is cut short, and the hit stays secured (§48).
	 */
	void CancelAttack();

	/** Server: reaches the current attack's Commit. Its windup timer calls it; tests call it directly. */
	void Commit();

	/** Server: the next attack consumes Empowerment at its Commit, unless it expires first. A newer one replaces it. */
	void Empower(FVeyraAttackEmpowerment Empowerment);

	/** Server: whether an empowerment waits for the next attack. */
	bool IsEmpowered() const;

	/** The timing an attack starting now would have (Combat Bible §22). */
	FVeyraAttackTiming GetTiming() const;

	const FVeyraAttackState& GetState() const { return State; }

	/** Server: when the next attack may start, in the server's world time. */
	double GetNextAttackAt() const { return NextAttackAt; }

	/** Server: the enemy Vanguard the hit chain counts, and how many consecutive attacks it has taken. */
	AActor* GetChainTarget() const { return ChainTarget.Get(); }
	int32 GetChain() const { return Chain; }

	/** Server: at Commit, before its damage is prepared, so attack modifiers can add to the attack. */
	TMulticastDelegate<void(FVeyraAttackPlan&)> OnModifyAttack;

	/** Server: On Attack, when an attack commits (Combat Bible §16). */
	TMulticastDelegate<void(const FVeyraAttackEvent&)> OnAttack;

	/** Server: On Hit, when an attack connects with its target. */
	TMulticastDelegate<void(const FVeyraAttackEvent&)> OnHit;

	/** Server: an attack ended, finished or cancelled, so an order can carry on. */
	TMulticastDelegate<void()> OnAttackEnded;

protected:
	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** An attack from its start to the end of its backswing. */
	struct FRunningAttack
	{
		TWeakObjectPtr<AActor> Target;
		double StartedAt = 0.0;
		FVeyraAttackTiming Timing;
	};

	/** What a committed attack does when it lands, prepared at Commit. */
	struct FLandingAttack
	{
		FVeyraAttackEvent Event;
		FVector AttackerLocation = FVector::ZeroVector;
		FVeyraPreparedDamage Damage;
		TArray<FVeyraStatusSpec> TargetStatuses;
		TOptional<FVeyraShape> CleaveShape;
		FVeyraPreparedDamage CleaveDamage;
		TArray<FVeyraStatusSpec> CleaveStatuses;
		TOptional<FVeyraShape> ImpactShape;
		FVeyraPreparedDamage ImpactDamage;
		TArray<FVeyraStatusSpec> ImpactStatuses;
	};

	FVeyraAttackPlan BuildPlan(UAbilitySystemComponent& Attacker, AActor& Target, const FVeyraAttackTiming& Timing);
	FLandingAttack Prepare(UAbilitySystemComponent& Attacker, const AActor& Body, const FVeyraAttackPlan& Plan) const;
	void Land(const FLandingAttack& Landing);

	/** Deals Damage and Statuses to the attacker's enemies in Placed other than the target: proc damage (ADR-009 §5). */
	void HitAround(const FVeyraAttackEvent& Event, const FVeyraPlacedShape& Placed, const FVeyraPreparedDamage& Damage,
		TConstArrayView<FVeyraStatusSpec> Statuses) const;

	void EndBackswing();
	void EndAttack();
	void EnterPhase(EVeyraAttackPhase Phase, AActor* Target, double EndsAt);
	void ResetChain();
	void OnCombatStateChanged(bool bInCombat);
	void OnDeath(const FVeyraDeathEvent& Death);

	UAbilitySystemComponent* GetAbilitySystem() const;

	/** Server world time, which the attack's times are in (see UVeyraCooldownComponent). */
	double GetServerNow() const;

	UPROPERTY(Replicated)
	FVeyraAttackState State;

	FVeyraBasicAttackProfile Profile;
	bool bHasProfile = false;

	TOptional<FRunningAttack> Running;
	double NextAttackAt = 0.0;

	TOptional<FVeyraAttackEmpowerment> Empowerment;
	double EmpowermentExpiresAt = 0.0;

	TWeakObjectPtr<AActor> ChainTarget;
	int32 Chain = 0;

	FTimerHandle PhaseTimer;
	FDelegateHandle CombatStateHandle;
	FDelegateHandle DeathHandle;
	FDelegateHandle InterruptedHandle;
};
