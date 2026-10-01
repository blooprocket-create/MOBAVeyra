// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Abilities/VeyraGameplayAbility.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Engine/TimerHandle.h"
#include "Delegates/IDelegateInstance.h"

#include "VeyraSelfBuffAbility.generated.h"

class AVeyraCompanion;
class AVeyraPlacedMarker;
struct FVeyraBuffMarkerTuning;
struct FVeyraMarkerEnd;

struct FVeyraHealTuning;
struct FVeyraSelfBuffAbilityTuning;

/**
 * The archetype for an ability that buffs its caster (ADR-008 §3): statuses and a shield on the
 * caster, optionally an aura of statuses for nearby allied Vanguards, refreshed on an interval
 * (ADR-008 §9), and optionally a heal for the caster and its most wounded ally (ADR-015 §3). A recast
 * may end it early. With the CasterOrAlly recipient it buffs the allied Vanguard the cast names
 * instead, and its aura follows that ally (ADR-027 §4). Each ability of this kind is an entry in
 * Abilities.json's selfBuff map.
 */
struct FVeyraHostileDamageEvent;

UCLASS()
class VEYRAABILITIES_API UVeyraSelfBuffAbility : public UVeyraGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual bool Defines(const FVeyraContentId& Ability) const override;
	virtual double GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual double GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual const FVeyraCastTuning* GetCastTuning(const FVeyraContentId& Ability) const override;
	virtual EVeyraCastRejection CheckTarget(const AActor& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const override;
	virtual bool EndsEarlyOnRecast(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability) const override;
	virtual void EndEarly(UAbilitySystemComponent& Caster, const FVeyraContentId& Ability) override;
	virtual FVeyraChannelPlan Deliver(const FVeyraCast& Cast) override;
	virtual bool IsOffensive(const FVeyraContentId& Ability) const override;

private:
	/** Leaves Tuning's marker where the caster stands, in place of any it left before (ADR-030 §5). */
	void PlaceMarker(UWorld& World, UAbilitySystemComponent& Caster, const FVeyraCast& Cast, const FVeyraBuffMarkerTuning& Tuning);

	/** Its marker ended: recalled or destroyed, it bursts where it stood. */
	void OnMarkerEnded(const FVeyraMarkerEnd& End);

	/** The marker it left, while it stands; what it bursts with; and its watch on markers' ends. */
	TWeakObjectPtr<AVeyraPlacedMarker> Marker;
	FVeyraContentId MarkerAbility;
	int32 MarkerRank = 0;
	int32 MarkerCastId = 0;
	FDelegateHandle MarkerEndHandle;

	/**
	 * The forms of Ability's stance: its slot's own ability and the override that holds the slot, each
	 * a self-buff its recast ends early (ADR-018 §1), as Vera's Dig In and its volley form. They share
	 * one stance: casting one ends the others', and recasting any ends them all. Only Ability itself
	 * for any other.
	 */
	TArray<FVeyraContentId> FormsOf(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability) const;

	/** Removes the statuses of each of Forms. */
	static void EndForms(UAbilitySystemComponent& Caster, TConstArrayView<FVeyraContentId> Forms);

	void RefreshAura();

	/** One interval of its drain: the resource it takes, or the buff's end once the resource or its time runs out (ADR-033 §6). */
	void Drain();

	TWeakObjectPtr<UAbilitySystemComponent> DrainCaster;
	FVeyraContentId DrainAbility;
	double DrainEndsAt = 0.0;
	FTimerHandle DrainTimer;
	void StopAura();

	/**
	 * Who takes Cast's buff: the allied Vanguard it names, for a CasterOrAlly buff, while that ally lives;
	 * otherwise its caster (ADR-027 §4).
	 */
	static UAbilitySystemComponent* RecipientOf(const FVeyraCast& Cast, const FVeyraSelfBuffAbilityTuning& Buff, UAbilitySystemComponent& Caster);

	/**
	 * Restores Heal's Health to Recipient and to its most wounded ally in range, and gives each Heal's
	 * statuses (ADR-015 §3); the healing and the statuses are Caster's.
	 */
	void DeliverHeal(UAbilitySystemComponent& Caster, UAbilitySystemComponent& Recipient, const FVeyraHealTuning& Heal) const;

	/** The living allied Vanguard within Range of Unit, not Unit, that lacks the most of its Health; none if all are whole. */
	UAbilitySystemComponent* FindMostWoundedAlly(const UAbilitySystemComponent& Unit, double Range) const;
	bool IsAuraRunning() const;

	/** The aura under way: whose, for which ability, and until when in world time. */
	TWeakObjectPtr<UAbilitySystemComponent> AuraCaster;
	/** Whom the aura follows: its caster, or the ally it buffs (ADR-027 §4). */
	TWeakObjectPtr<UAbilitySystemComponent> AuraHolder;
	FVeyraContentId AuraAbility;
	double AuraEndsAt = 0.0;
	FTimerHandle AuraTimer;

	/** An end payload under way (ADR-018 §6): counts the hostile hits its caster takes until it comes. */
	void StartPayload(UAbilitySystemComponent& Caster, const FVeyraContentId& Ability);
	void OnHostileDamage(const FVeyraHostileDamageEvent& Event);
	void FirePayload();
	void StopPayload();

	TWeakObjectPtr<UAbilitySystemComponent> PayloadCaster;
	FVeyraContentId PayloadAbility;
	int32 PayloadHits = 0;
	FTimerHandle PayloadTimer;
	FDelegateHandle HostileDamageHandle;

	/**
	 * Its caster's companion's part (ADR-034 §7): the statuses the companion holds while the buff lasts, a
	 * chain between the two, and an end with the companion. A later cast's part takes over.
	 */
	void StartCompanionPart(UWorld& World, UAbilitySystemComponent& Caster, const FVeyraCast& Cast, const FVeyraSelfBuffAbilityTuning& Buff);

	/** One pulse of the chain: the enemies on the line take its effects, each no more often than its rate. */
	void PulseChain();

	/** Ends the companion's part: its statuses, the chain and the watch on the companion. */
	void StopCompanionPart();

	void OnCompanionBanished(AVeyraCompanion& Companion);

	/** Whether Buff still lasts on Caster: its first status, its own, still holds. */
	static bool LastsFor(const UAbilitySystemComponent& Caster, const FVeyraSelfBuffAbilityTuning& Buff);

	TWeakObjectPtr<UAbilitySystemComponent> PartCaster;
	TWeakObjectPtr<AVeyraCompanion> PartCompanion;
	FVeyraContentId PartAbility;
	FVeyraPreparedEffects ChainEffects;
	FVeyraAbilityHitSource ChainSource;
	/** When each enemy on the chain may take it again, in world time. */
	TMap<TWeakObjectPtr<AActor>, double> ChainNextAt;
	FTimerHandle ChainTimer;
	FDelegateHandle BanishHandle;
};
