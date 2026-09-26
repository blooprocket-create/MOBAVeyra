// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Absorption/VeyraAbsorptionLedger.h"
#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "Containers/Map.h"
#include "UObject/WeakObjectPtr.h"

#include "VeyraDamageAbsorptionComponent.generated.h"

class UAbilitySystemComponent;
struct FActiveGameplayEffect;
struct FGameplayEffectSpec;

/**
 * A unit's shields and Temporary Health (Combat Bible §7). Each entry belongs to one shield or
 * Temporary Health effect: the server adds it when the effect is applied, removes it when the effect
 * ends, and removes the effect when damage empties the entry. The ledger replicates for presentation.
 */
UCLASS(ClassGroup = Combat)
class VEYRACOMBAT_API UVeyraDamageAbsorptionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVeyraDamageAbsorptionComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Follows the Ability System Component's shield and Temporary Health effects. Called once. */
	void BindTo(UAbilitySystemComponent& AbilitySystem);

	/**
	 * Combat Bible §25 steps 7–9 for one mitigated damage component, on the server. Health is the
	 * unit's ordinary Health before the component; the caller applies the result's HealthLost.
	 */
	FVeyraAbsorptionResult ApplyIncomingDamage(EVeyraDamageType Type, double Amount, bool bInvulnerable, double Health);

	const FVeyraAbsorptionLedger& GetLedger() const { return Ledger; }

private:
	void OnEffectAdded(UAbilitySystemComponent* AbilitySystem, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle);
	void OnEffectRemoved(const FActiveGameplayEffect& Effect);
	void MarkLedgerDirty();

	UPROPERTY(Replicated)
	FVeyraAbsorptionLedger Ledger;

	/** The effect behind each ledger entry, by the entry's Sequence. Server only. */
	TMap<int32, FActiveGameplayEffectHandle> EffectsBySequence;

	/** The Sequence the next entry receives; entries are numbered in the order they arrive. */
	int32 NextSequence = 0;

	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystem;
};
