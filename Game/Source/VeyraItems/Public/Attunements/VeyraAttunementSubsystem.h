// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "VeyraAttunementSubsystem.generated.h"

class UAbilitySystemComponent;
struct FVeyraCastEvent;
struct FVeyraDamageDealtEvent;
struct FVeyraDamageResolution;
struct FVeyraDeathEvent;
struct FVeyraMarkedForDoomTuning;
struct FVeyraSpellShieldBlocked;

/**
 * The Attunements a hit sets off (Item Bible §8–§9; ADR-023 §3–§4; ADR-051 §3): Reprisal Guard, Drag, Convergence,
 * Fracture, Endless Cleave, Tempered by Conflict, No Allegiance, Clean Break, Through the Guard, No One Coming and
 * Reenactment. It listens to Combat's dealt-damage event and acts
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

	/** The Doom Holder has built on Target, 0 when none or expired (ADR-025 §7). */
	double GetDoom(const UAbilitySystemComponent& Holder, const UAbilitySystemComponent& Target) const;

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
	 * - High Tide: out of Vanguard combat, while it restores something, a tick's Current amplifies Health
	 *   Regeneration and speeds Safe Harbor.
	 * - Safe Harbor: out of Vanguard combat and missing Health, a tick's Reserve converts into Health;
	 *   past full Health, while High Tide spends, the rest becomes Temporary Health, up to its cap.
	 * A timer calls it on the server each regeneration tick.
	 */
	void UpdateHeld();

	/** When Holder last took damage from an enemy Vanguard, in world time; unset if it never has. */
	TOptional<double> GetVanguardDamageTakenAt(const UAbilitySystemComponent& Holder) const;

	/** Through the Guard reads whose shields each damage component broke (ADR-051 §3). */
	void OnDamageResolved(const FVeyraDamageResolution& Resolution);

	/** The holder's latest committed cast names its ability hits as actions (ADR-051 §9.1). */
	void OnCastCommitted(const FVeyraCastEvent& Cast);

	/** No One Coming: an ally arriving within the radius breaks an unlocked Abandoned mark (ADR-051 §3). Tests call it. */
	void UpdateAbandoned();

	/** Whether Holder's No One Coming marks Target Abandoned now, and whether the mark has locked in. */
	bool IsAbandoned(const UAbilitySystemComponent& Holder, const UAbilitySystemComponent& Target, bool* bOutLocked = nullptr) const;

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
	void MarkedForDoom(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder, UAbilitySystemComponent& Target,
		double Now);
	void SafeHarbor(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder);

	/** ADR-051 §3's Attunements, in VeyraBurstAttunements.cpp. LastDealt is when Holder last damaged an enemy Vanguard before this hit. */
	void NoAllegiance(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder, UAbilitySystemComponent& Target,
		double Now, TOptional<double> LastDealt);
	void TallyForCleanBreak(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder,
		UAbilitySystemComponent& Target, double Now);
	void CleanBreak(const FVeyraDeathEvent& Death);
	void NoOneComing(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder, UAbilitySystemComponent& Target,
		double Now);
	void Reenactment(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder, UAbilitySystemComponent& Target,
		double Now, TOptional<double> LastDealt);

	/** The Attunements the items Holder carries hold, each once. */
	TArray<FVeyraContentId, TInlineAllocator<6>> HeldBy(const UAbilitySystemComponent& Holder) const;

	/** One action of a holder's, as No Allegiance tells them apart: a basic attack, or an ability by its ID. */
	struct FAction
	{
		bool bBasicAttack = false;
		FVeyraContentId Ability;
		bool operator==(const FAction& Other) const { return bBasicAttack == Other.bBasicAttack && Ability == Other.Ability; }
	};

	/** The action Event was, if it was one: a basic attack, or an ability hit named by Holder's latest committed cast. */
	TOptional<FAction> ActionOf(const FVeyraDamageDealtEvent& Event, const UAbilitySystemComponent& Holder) const;

	/** No Allegiance's Openings, by holder and target. */
	struct FOpening
	{
		TWeakObjectPtr<const UAbilitySystemComponent> Holder;
		TWeakObjectPtr<const UAbilitySystemComponent> Target;
		FVeyraContentId Attunement;
		FAction Opener;
		double Until = 0.0;
	};
	TArray<FOpening> Openings;

	/** Clean Break's record of what each holder dealt each enemy Vanguard, and when. */
	struct FTally
	{
		TWeakObjectPtr<const UAbilitySystemComponent> Holder;
		TWeakObjectPtr<const UAbilitySystemComponent> Target;
		FVeyraContentId Attunement;
		TArray<TPair<double, double>> Hits;
	};
	TArray<FTally> Tallies;

	/** Through the Guard's brands, by holder, target and shield. */
	struct FBrand
	{
		TWeakObjectPtr<const UAbilitySystemComponent> Holder;
		TWeakObjectPtr<const UAbilitySystemComponent> Target;
		TWeakObjectPtr<const UAbilitySystemComponent> Provider;
		FVeyraContentId Shield;
		FVeyraContentId Attunement;
		double Until = 0.0;
		double Breach = 0.0;
	};
	TArray<FBrand> Brands;

	/** No One Coming's Abandoned marks, by holder and target. */
	struct FAbandoned
	{
		TWeakObjectPtr<const UAbilitySystemComponent> Holder;
		TWeakObjectPtr<const UAbilitySystemComponent> Target;
		FVeyraContentId Attunement;
		double Until = 0.0;
		double Dealt = 0.0;
		/** When its lock ends; 0 until it locks in. */
		double LockedUntil = 0.0;
	};
	TArray<FAbandoned> Abandons;

	/** Reenactment's memories, by holder and target. */
	struct FMemory
	{
		TWeakObjectPtr<const UAbilitySystemComponent> Holder;
		TWeakObjectPtr<const UAbilitySystemComponent> Target;
		FVeyraContentId Attunement;
		double Until = 0.0;
		double Wound = 0.0;
		FVector Where = FVector::ZeroVector;
	};
	TArray<FMemory> Memories;

	/** When each holder last damaged an enemy Vanguard, and the ability of its latest committed cast. */
	TMap<TWeakObjectPtr<const UAbilitySystemComponent>, double> VanguardDamageDealtAt;
	TMap<TWeakObjectPtr<const UAbilitySystemComponent>, FVeyraContentId> LastCast;
	FDelegateHandle ResolvedHandle;
	FDelegateHandle CastCommittedHandle;

	/** Marks Target with the most Doom any holder has on it, from Source, lasting while any Doom does. */
	void ShowDoom(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, const FVeyraContentId& Attunement, const FVeyraMarkedForDoomTuning& Tuning, double Now);

	/** One holder's Doom on one enemy Vanguard (ADR-025 §7). */
	struct FDoom
	{
		TWeakObjectPtr<const UAbilitySystemComponent> Holder;
		TWeakObjectPtr<const UAbilitySystemComponent> Target;
		FVeyraContentId Attunement;
		double Doom = 0.0;
		double ExpiresAt = 0.0;
	};
	TArray<FDoom> Dooms;

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
