// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "VeyraAttunementSubsystem.generated.h"

class UAbilitySystemComponent;
struct FVeyraDamageDealtEvent;
struct FVeyraDeathEvent;
struct FVeyraSpellShieldBlocked;

/**
 * The Attunements a hit sets off (Item Bible §8–§9; ADR-023 §3–§4): Reprisal Guard, Drag, Convergence,
 * Fracture, Endless Cleave and Tempered by Conflict. It listens to Combat's dealt-damage event and acts
 * through Combat's verbs, with every number from Items.json. Static Attunements fold into the holder's
 * stats (VeyraEquipment::StatsFor), and stacking buffs into the shop's; this owns only what a hit, or
 * standing near an enemy, sets off. Server only.
 */
UCLASS()
class VEYRAITEMS_API UVeyraAttunementSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Acts on one damage instance dealt, for each Attunement its source holds. */
	void OnDamageDealt(const FVeyraDamageDealtEvent& Event);

	/**
	 * Tempered by Conflict's charge (Item Bible §8): an enemy Vanguard that stays near a holder long
	 * enough becomes Tempered, and one that leaves, or dies, loses it. A timer calls it on the server.
	 */
	void UpdateTempering();

	/** Whether Holder has tempered Target, ready for its next basic attack. */
	bool IsTempered(const UAbilitySystemComponent& Holder, const UAbilitySystemComponent& Target) const;

	/** A last hit on an enemy lane Fluxborn stores Residual Current in the last hitter's item (ADR-025 §7). */
	void OnDeath(const FVeyraDeathEvent& Death);

	/** A Spell Shield was consumed: a Quieting Chime's waits to form again (ADR-025 §7). */
	void OnSpellShieldBlocked(const FVeyraSpellShieldBlocked& Blocked);

	/**
	 * What holders' Attunements do over time (Item Bible §8, §10; ADR-025 §7), each call for the
	 * regeneration tick to come:
	 * - Residual Current: a holder that has gone its quiet time without enemy-Vanguard damage, and is
	 *   missing Health, spends a tick's Current to amplify its Health Regeneration until the next call;
	 *   otherwise the amplification stops and the Current keeps.
	 * - Quieting Chime: a formed Spell Shield is kept; a consumed one forms again once ReformSeconds
	 *   have passed since both its consumption and the holder's last enemy-Vanguard damage.
	 * A timer calls it on the server each regeneration tick.
	 */
	void UpdateHeld();

	/** When Holder last took damage from an enemy Vanguard, in world time; unset if it never has. */
	TOptional<double> GetVanguardDamageTakenAt(const UAbilitySystemComponent& Holder) const;

private:
	/** A holder's Attunement that waits, or a target it primed, until a world time. */
	struct FTimed
	{
		TWeakObjectPtr<const UAbilitySystemComponent> Holder;
		TWeakObjectPtr<const UAbilitySystemComponent> Target;
		FVeyraContentId Attunement;
		double Until = 0.0;
	};

	/** One holder's Tempered by Conflict against one enemy Vanguard. */
	struct FTempering
	{
		TWeakObjectPtr<const UAbilitySystemComponent> Holder;
		TWeakObjectPtr<const UAbilitySystemComponent> Target;
		FVeyraContentId Attunement;
		/** Since when the enemy has stayed near; below 0 while it is not. */
		double NearSince = -1.0;
		bool bTempered = false;
		/** The per-enemy cooldown: no charge before this world time. */
		double ReadyAt = 0.0;
	};

	void ReprisalGuard(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder, double Now);
	void Drag(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder, UAbilitySystemComponent& Target);
	void Convergence(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder, UAbilitySystemComponent& Target,
		double Now);
	void Fracture(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder, UAbilitySystemComponent& Target);
	void EndlessCleave(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder, UAbilitySystemComponent& Target);
	void TemperedByConflict(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder,
		UAbilitySystemComponent& Target, double Now);

	/** Drag the Tempo: an enemy Vanguard's basic attack damaged Holder, so Attacker's Attack Speed slows (ADR-025 §7). */
	void DragTheTempo(UAbilitySystemComponent& Holder, UAbilitySystemComponent& Attacker);

	/** Reprisal Guard's cooldowns, by holder. */
	TArray<FTimed> Cooldowns;

	/** Convergence's primes, by holder and target. */
	TArray<FTimed> Primes;

	/** Tempered by Conflict, by holder and enemy. */
	TArray<FTempering> Tempering;

	FTimerHandle TemperingTimer;
	FDelegateHandle DamageDealtHandle;
	FDelegateHandle DeathHandle;
	FDelegateHandle SpellShieldBlockedHandle;

	/** When each holder's Quieting Chime was last consumed, in world time. */
	TMap<TWeakObjectPtr<const UAbilitySystemComponent>, double> ChimeConsumedAt;

	/** Starts the timer that calls UpdateHeld, if it is not running. Server only. */
	void EnsureHeldTimer();

	/** Each holder's latest enemy-Vanguard damage taken, in world time (ADR-025 §5). */
	TMap<TWeakObjectPtr<const UAbilitySystemComponent>, double> VanguardDamageTakenAt;
	FTimerHandle HeldTimer;
};
