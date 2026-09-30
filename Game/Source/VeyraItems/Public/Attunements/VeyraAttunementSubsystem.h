// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "VeyraAttunementSubsystem.generated.h"

class UAbilitySystemComponent;
struct FVeyraDamageDealtEvent;

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

	/** Reprisal Guard's cooldowns, by holder. */
	TArray<FTimed> Cooldowns;

	/** Convergence's primes, by holder and target. */
	TArray<FTimed> Primes;

	/** Tempered by Conflict, by holder and enemy. */
	TArray<FTempering> Tempering;

	FTimerHandle TemperingTimer;
	FDelegateHandle DamageDealtHandle;
};
