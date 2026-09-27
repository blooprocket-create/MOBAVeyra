// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "Containers/Map.h"
#include "Statuses/VeyraStatusTypes.h"
#include "UObject/WeakObjectPtr.h"

#include "VeyraStatusComponent.generated.h"

class UAbilitySystemComponent;
struct FActiveGameplayEffect;

/**
 * A unit's statuses (ADR-009 §1): the truth for each one's identity, source, kind, magnitude,
 * stacks and timing. The server applies each under its stacking policy (Combat Bible §46) and backs
 * it with one Gameplay Effect, so it freezes with the world during a pause and ends at death with
 * the other temporary effects (§44). The ledger replicates to every machine for presentation.
 */
UCLASS(ClassGroup = Combat)
class VEYRACOMBAT_API UVeyraStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVeyraStatusComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Follows the Ability System Component's effects, so each status ends with its effect. Called once. */
	void BindTo(UAbilitySystemComponent& AbilitySystem);

	/**
	 * Server only: applies Spec from Source under its stacking policy. Tenacity shortens crowd control
	 * as it lands (§8). Returns false if refused; an application the policy leaves out, such as a
	 * weaker one under UniqueReplaceStrongest, is accepted and changes nothing.
	 */
	bool Apply(UAbilitySystemComponent& Source, const FVeyraStatusSpec& Spec);

	/** Server only: ends status Id early, from every source. Returns whether the unit had it. */
	bool Remove(const FVeyraContentId& Id);

	const FVeyraStatusLedger& GetLedger() const { return Ledger; }

	/** The fraction of speed the strongest Slow removes; 0 when there is none. */
	double GetStrongestSlow() const;

	/** The actions the unit's statuses stop it taking. */
	EVeyraActionBlocks GetActionBlocks() const;

	/** Raised on every machine when the statuses change. */
	TMulticastDelegate<void()> OnStatusesChanged;

	/** Server only: raised when a status interrupts the unit's current action: a Stun (Combat Bible §26). */
	TMulticastDelegate<void()> OnInterrupted;

private:
	/** What only the server keeps for each entry, by its Sequence. */
	struct FServerEntry
	{
		FActiveGameplayEffectHandle Effect;
		TWeakObjectPtr<UAbilitySystemComponent> Source;
	};

	UFUNCTION()
	void OnRep_Ledger();

	void OnEffectRemoved(const FActiveGameplayEffect& Effect);
	FActiveGameplayEffectHandle ApplyEffect(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, EVeyraStatusKind Kind,
		double Magnitude, int32 Stacks, double DurationSeconds) const;
	int32 FindActive(const FVeyraStatusSpec& Spec, const UAbilitySystemComponent& Source) const;
	void MarkLedgerChanged();

	/** Server world time, which the ledger's times are in (see UVeyraCooldownComponent). */
	double GetServerNow() const;

	UPROPERTY(ReplicatedUsing = OnRep_Ledger)
	FVeyraStatusLedger Ledger;

	TMap<int32, FServerEntry> ServerEntries;

	/** The Sequence the next entry receives. */
	int32 NextSequence = 0;

	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystem;
};
