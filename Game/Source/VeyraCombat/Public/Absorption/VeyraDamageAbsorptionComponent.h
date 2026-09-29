// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Absorption/VeyraAbsorptionLedger.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "Containers/Map.h"
#include "Misc/Optional.h"
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
	/** Behind the fog on a participant: its own, its teammates' and its observers' (ADR-016 §3). */
	virtual ELifetimeCondition GetReplicationCondition() const override;
	virtual void ReadyForReplication() override;

	/** Follows the Ability System Component's shield and Temporary Health effects. Called once. */
	void BindTo(UAbilitySystemComponent& AbilitySystem);

	/**
	 * Server only: grants a shield from Source (ADR-009 §3). A grant meets an active shield with its
	 * identity from the same source as its Reapply says, and never takes the shield past its maximum
	 * or its cap group past its total; merging never lowers what a shield has left. A merged or
	 * replaced shield keeps its age for absorption order (Combat Bible §7). Returns the shield's
	 * effect, or an invalid handle when the grant is refused or there is no room for it.
	 */
	FActiveGameplayEffectHandle GrantShield(UAbilitySystemComponent& Source, const FVeyraShieldGrant& Grant);

	/**
	 * Combat Bible §25 steps 7–9 for one mitigated damage component, on the server. Health is the
	 * unit's ordinary Health before the component; the caller applies the result's HealthLost. Each
	 * shield's share goes to OutShieldShares, when given, with the shield's provider.
	 */
	FVeyraAbsorptionResult ApplyIncomingDamage(EVeyraDamageType Type, double Amount, bool bInvulnerable, double Health,
		TArray<FVeyraShieldShare>* OutShieldShares = nullptr);

	const FVeyraAbsorptionLedger& GetLedger() const { return Ledger; }

private:
	/** What only the server keeps for each ledger entry. */
	struct FServerEntry
	{
		FActiveGameplayEffectHandle Effect;
		/** A shield's identity, source and cap group, when a grant gave it them. */
		FVeyraContentId Id;
		TWeakObjectPtr<UAbilitySystemComponent> Source;
		FVeyraContentId CapGroup;
	};

	/** The grant GrantShield is applying, which OnEffectAdded records its entry with. */
	struct FPendingGrant
	{
		FVeyraContentId Id;
		TWeakObjectPtr<UAbilitySystemComponent> Source;
		FVeyraContentId CapGroup;
		/** The entry the grant replaces or merges into, which keeps its Sequence; INDEX_NONE for a new shield. */
		int32 ReplacesSequence = INDEX_NONE;
	};

	void OnEffectAdded(UAbilitySystemComponent* AbilitySystem, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle);
	void OnEffectRemoved(const FActiveGameplayEffect& Effect);
	void MarkLedgerDirty();

	UPROPERTY(Replicated)
	FVeyraAbsorptionLedger Ledger;

	/** Each ledger entry's effect and identity, by the entry's Sequence. Server only. */
	TMap<int32, FServerEntry> ServerEntries;

	TOptional<FPendingGrant> PendingGrant;

	/** The Sequence the next entry receives; entries are numbered in the order they arrive. */
	int32 NextSequence = 0;

	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystem;
};
