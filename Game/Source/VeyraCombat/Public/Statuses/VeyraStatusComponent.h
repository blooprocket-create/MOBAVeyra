// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "Containers/Map.h"
#include "Engine/TimerHandle.h"
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
	/** Behind the fog on a participant: its own, its teammates' and its observers' (ADR-016 §3). */
	virtual ELifetimeCondition GetReplicationCondition() const override;
	virtual void ReadyForReplication() override;

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

	/**
	 * Server only: the unit took part in a takedown (Combat Bible §18). Each status that takedowns
	 * extend gains its extension, up to its maximum in all (ADR-009 §1).
	 */
	void ExtendForTakedown();

	const FVeyraStatusLedger& GetLedger() const { return Ledger; }

	/** The fraction of speed the strongest Slow removes; 0 when there is none. */
	double GetStrongestSlow() const;

	/** The largest magnitude among the unit's statuses of Kind; 0 when it has none. */
	double GetStrongest(EVeyraStatusKind Kind) const;

	/** The actions the unit's statuses stop it taking. */
	EVeyraActionBlocks GetActionBlocks() const;

	/** Raised on every machine when the statuses change. */
	TMulticastDelegate<void()> OnStatusesChanged;

	/**
	 * Server only: something interrupted the unit's current action: a Stun, or a displacement (Combat
	 * Bible §9, §26). The unit's interruptions are announced here, beside its crowd control.
	 */
	void NotifyInterrupted();

	/** Server only: raised by NotifyInterrupted and whenever a Stun lands. */
	TMulticastDelegate<void()> OnInterrupted;

private:
	/** What only the server keeps for each entry, by its Sequence. */
	struct FServerEntry
	{
		FActiveGameplayEffectHandle Effect;
		TWeakObjectPtr<UAbilitySystemComponent> Source;
		double TakedownExtensionSeconds = 0.0;
		double TakedownExtensionMaxSeconds = 0.0;
		/** How much takedowns have extended the current application so far. */
		double ExtendedSeconds = 0.0;
		/** A DamageOverTime status's ticks (Combat Bible §14): what each deals, and those still to come. */
		FTimerHandle TickTimer;
		EVeyraDamageType TickDamageType = EVeyraDamageType::Physical;
		double TickDamage = 0.0;
		double TickSeconds = 0.0;
		int32 TicksLeft = 0;
	};

	UFUNCTION()
	void OnRep_Ledger();

	void OnEffectRemoved(const FActiveGameplayEffect& Effect);
	FActiveGameplayEffectHandle ApplyEffect(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, EVeyraStatusKind Kind,
		double Magnitude, int32 Stacks, double DurationSeconds) const;
	int32 FindActive(const FVeyraStatusSpec& Spec, const UAbilitySystemComponent& Source) const;
	void MarkLedgerChanged();

	/** Starts entry Sequence's ticks afresh: the first comes TickSeconds after now (§14: none as it lands). */
	void StartTicking(int32 Sequence, FServerEntry& Server);

	/** Deals entry Sequence's next tick in its source's name, if it still has one to deal. */
	void DealTick(int32 Sequence);

	/**
	 * Stops an entry's ticks. An entry whose effect ends with one tick still to come deals it first:
	 * that tick falls on the same moment, whichever timer runs first (§14: its last tick counts). One
	 * removed early deals nothing more. The entry must be out of the ledger already, since a lethal
	 * tick changes it.
	 */
	void StopTicking(FServerEntry& Server, bool bDealDueTick);

	/** Deals one tick of Amount of Type from Source to this unit, delivered as Periodic. */
	void DealTickDamage(const TWeakObjectPtr<UAbilitySystemComponent>& Source, EVeyraDamageType Type, double Amount) const;

	/** Server world time, which the ledger's times are in (see UVeyraCooldownComponent). */
	double GetServerNow() const;

	UPROPERTY(ReplicatedUsing = OnRep_Ledger)
	FVeyraStatusLedger Ledger;

	TMap<int32, FServerEntry> ServerEntries;

	/** The Sequence the next entry receives. */
	int32 NextSequence = 0;

	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystem;
};
