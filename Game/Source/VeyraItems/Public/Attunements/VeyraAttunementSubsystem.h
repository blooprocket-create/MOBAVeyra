// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Subsystems/WorldSubsystem.h"
#include "VeyraAttunementSubsystem.generated.h"

class UAbilitySystemComponent;
struct FVeyraDamageDealtEvent;

/**
 * The Attunements a hit sets off (Item Bible §8–§9; ADR-022 §3–§4): Reprisal Guard, Drag, Convergence
 * and Fracture. It listens to Combat's dealt-damage event and acts through Combat's verbs, with every
 * number from Items.json. Static Attunements fold into the holder's stats (VeyraEquipment::StatsFor),
 * and stacking buffs into the shop's; this owns only what a hit on an enemy Vanguard sets off. Server only.
 */
UCLASS()
class VEYRAITEMS_API UVeyraAttunementSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Acts on one damage instance dealt, for each Attunement its source holds. */
	void OnDamageDealt(const FVeyraDamageDealtEvent& Event);

private:
	/** A holder's Attunement that waits, or a target it primed, until a world time. */
	struct FTimed
	{
		TWeakObjectPtr<const UAbilitySystemComponent> Holder;
		TWeakObjectPtr<const UAbilitySystemComponent> Target;
		FVeyraContentId Attunement;
		double Until = 0.0;
	};

	void ReprisalGuard(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder, double Now);
	void Drag(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder, UAbilitySystemComponent& Target);
	void Convergence(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder, UAbilitySystemComponent& Target,
		double Now);
	void Fracture(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder, UAbilitySystemComponent& Target);

	/** Reprisal Guard's cooldowns, by holder. */
	TArray<FTimed> Cooldowns;

	/** Convergence's primes, by holder and target. */
	TArray<FTimed> Primes;

	FDelegateHandle DamageDealtHandle;
};
