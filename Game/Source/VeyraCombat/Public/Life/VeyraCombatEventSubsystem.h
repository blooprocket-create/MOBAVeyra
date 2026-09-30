// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Damage/VeyraDamageTypes.h"
#include "Statuses/VeyraStatusTypes.h"
#include "Subsystems/WorldSubsystem.h"

#include "VeyraCombatEventSubsystem.generated.h"

class UAbilitySystemComponent;

/** One Vanguard's latest contribution to a unit's death: damage, crowd control or a debuff (Combat Bible §18). */
struct FVeyraContribution
{
	TWeakObjectPtr<UAbilitySystemComponent> Contributor;

	/** When it last contributed, in the server's world time. */
	double AtSeconds = 0.0;
};

/** A finalized death (Combat Bible §18). */
struct FVeyraDeathEvent
{
	TWeakObjectPtr<UAbilitySystemComponent> Victim;

	/** Whoever dealt the lethal damage: a Vanguard, a Fluxborn or a structure. Null for damage with no source. */
	TWeakObjectPtr<UAbilitySystemComponent> Killer;

	/**
	 * The enemy Vanguard credited with the kill (Combat Bible §18): the Killer when it is one;
	 * otherwise the enemy Vanguard that contributed most recently within the kill-credit window, as
	 * when a tower finishes a Vanguard someone was fighting. Null for an Execution.
	 */
	TWeakObjectPtr<UAbilitySystemComponent> CreditedKiller;

	/**
	 * For a Vanguard victim, the enemy Vanguards other than the credited killer who damaged,
	 * crowd-controlled or debuffed it within the assist window (§18): with the credited killer, the
	 * takedown's participants. Empty for any other victim: kills and assists are Vanguard terms.
	 */
	TArray<TWeakObjectPtr<UAbilitySystemComponent>> Assisters;

	/**
	 * Every enemy Vanguard that contributed to the victim, whatever it is, with when it last did.
	 * Economy's windows read it for participation and structure rewards (Economy Bible §3.2, §8.1).
	 */
	TArray<FVeyraContribution> Contributions;

	/** When the victim died, in the server's world time. */
	double DiedAtSeconds = 0.0;

	/**
	 * Where the victim's body stood when it died; unset for a unit with no body. Rewards measure who
	 * was near the death from it (Economy Bible §2), whatever becomes of the body afterwards.
	 */
	TOptional<FVector> Location;
};

/** What one shield absorbed of a damage component, by its provider (Match Statistics Bible §3). */
struct FVeyraShieldShare
{
	TWeakObjectPtr<UAbilitySystemComponent> Provider;
	double Absorbed = 0.0;
};

/**
 * One damage component as it resolved against a unit (Combat Bible §25 steps 7–9): what it cost the
 * unit, and whose shields took it. Statistics read it; Combat keeps none (ADR-017 §1).
 */
struct FVeyraDamageResolution
{
	TWeakObjectPtr<UAbilitySystemComponent> Source;
	TWeakObjectPtr<UAbilitySystemComponent> Target;
	EVeyraDamageType Type = EVeyraDamageType::Physical;

	/** Health actually removed: never what shields or Temporary Health took, nor overkill. */
	double HealthLost = 0.0;

	double TemporaryHealthSpent = 0.0;

	/** What each shield absorbed, oldest first, by the shield's provider. */
	TArray<FVeyraShieldShare> Shields;
};

/** Health actually restored to a unit, never above its Max Health (Combat Bible §6). */
struct FVeyraHealthRestored
{
	/** The unit that healed it, when one did: none for regeneration, the fountain or a Well. */
	TWeakObjectPtr<UAbilitySystemComponent> Provider;
	TWeakObjectPtr<UAbilitySystemComponent> Target;
	double Restored = 0.0;
};

/** A status as it was applied, after Tenacity (Combat Bible §8–§14). */
struct FVeyraStatusApplied
{
	TWeakObjectPtr<UAbilitySystemComponent> Source;
	TWeakObjectPtr<UAbilitySystemComponent> Target;
	EVeyraStatusKind Kind = EVeyraStatusKind::Slow;

	/** When it began and ends, in server gameplay time. */
	double StartsAt = 0.0;
	double EndsAt = 0.0;

	/** Which status: a passive may wait for one, as Cadence waits for The Last Volley (ADR-018 §3). */
	FVeyraContentId Id;
};

/** A cast as it begins or commits (ADR-018 §3). */
struct FVeyraCastEvent
{
	TWeakObjectPtr<UAbilitySystemComponent> Caster;
	FVeyraContentId Ability;

	/** Whether it has an effect on enemies: offensive casts end stealth (Combat Bible §11). */
	bool bOffensive = false;
};

/** A forced displacement as it starts (ADR-018 §3): who moved whom, and how far after resistance. */
struct FVeyraDisplacementEvent
{
	TWeakObjectPtr<UAbilitySystemComponent> Source;
	TWeakObjectPtr<UAbilitySystemComponent> Target;
	double Distance = 0.0;
};

/** A Spell Shield that blocked a hostile ability hit and was consumed (Combat Bible §19). */
struct FVeyraSpellShieldBlocked
{
	/** The unit whose Spell Shield it was. */
	TWeakObjectPtr<UAbilitySystemComponent> Target;
	/** The unit whose ability hit was blocked. */
	TWeakObjectPtr<UAbilitySystemComponent> Source;
	/** The Spell Shield status consumed. */
	FVeyraContentId Shield;
};

/** Damage dealt by a unit to a unit on the opposing side, as it lands. */
struct FVeyraHostileDamageEvent
{
	TWeakObjectPtr<UAbilitySystemComponent> Source;
	TWeakObjectPtr<UAbilitySystemComponent> Target;
	EVeyraDamageDelivery Delivery = EVeyraDamageDelivery::Ability;
};

/**
 * One damage instance a unit dealt to an enemy, as it resolved (ADR-023 §4): what each of its types
 * cost the target after mitigation, in Health, Temporary Health and shields, never overkill. A
 * damage-over-time tick is its own instance, with the Periodic delivery.
 */
struct FVeyraDamageDealtEvent
{
	TWeakObjectPtr<UAbilitySystemComponent> Source;
	TWeakObjectPtr<UAbilitySystemComponent> Target;
	EVeyraDamageDelivery Delivery = EVeyraDamageDelivery::Ability;
	/** By type, each type at most once; a type that cost nothing is absent. */
	FVeyraDamageComponents Dealt;

	double Of(EVeyraDamageType Type) const
	{
		const FVeyraDamageComponent* Component = Dealt.FindByPredicate([Type](const FVeyraDamageComponent& Each) { return Each.Type == Type; });
		return Component ? Component->Amount : 0.0;
	}

	double Total() const
	{
		double Sum = 0.0;
		for (const FVeyraDamageComponent& Component : Dealt)
		{
			Sum += Component.Amount;
		}
		return Sum;
	}
};

/**
 * Combat's outcomes, announced to the systems above it as events rather than through hard
 * references (ARCHITECTURE.md §1.7). Match listens for deaths to schedule respawns, Economy for
 * rewards, and the battleground for who is hurting whom. Server only: Combat decides outcomes only
 * on the server.
 */
UCLASS()
class VEYRACOMBAT_API UVeyraCombatEventSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnDeath, const FVeyraDeathEvent&);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnHostileDamage, const FVeyraHostileDamageEvent&);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnDamageDealt, const FVeyraDamageDealtEvent&);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnDamageResolved, const FVeyraDamageResolution&);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnHealthRestored, const FVeyraHealthRestored&);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnStatusApplied, const FVeyraStatusApplied&);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnCast, const FVeyraCastEvent&);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnDisplaced, const FVeyraDisplacementEvent&);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSpellShieldBlocked, const FVeyraSpellShieldBlocked&);

	FOnDeath OnDeath;

	/** A Spell Shield blocked a hostile ability hit and was consumed (Combat Bible §19; ADR-025 §4). */
	FOnSpellShieldBlocked OnSpellShieldBlocked;

	/** Hostile damage that was dealt, shields included (Battleground Bible §19: tower and Fluxborn aggro). */
	FOnHostileDamage OnHostileDamage;

	/**
	 * Each damage instance dealt to an enemy, with what it cost by type (ADR-023 §4), after
	 * OnHostileDamage: Attunements that act on a hit read it.
	 */
	FOnDamageDealt OnDamageDealt;

	/**
	 * Every damage component that cost a unit something, before any death it causes (ADR-017 §1):
	 * Health, Temporary Health or a shield.
	 */
	FOnDamageResolved OnDamageResolved;

	/** Health actually restored, with the unit that healed it when one did. */
	FOnHealthRestored OnHealthRestored;

	/** A status applied or refreshed on a unit. */
	FOnStatusApplied OnStatusApplied;

	/** A cast began: its windup starts, or it commits at once (ADR-018 §3). */
	FOnCast OnCastStarted;

	/** A cast reached its Commit and was paid for (Combat Bible §26). */
	FOnCast OnCastCommitted;

	/** A unit forced another to move: a Knockback, Pull or Knockup's travel (Combat Bible §9). */
	FOnDisplaced OnDisplaced;

	/** Broadcasts OnDamageResolved, and counts what the component cost toward the instance being dealt. */
	void ResolveDamage(const FVeyraDamageResolution& Resolution);

	/**
	 * The damage pipeline opens an instance around its application and closes it after: what each
	 * component cost between the two is what the instance dealt. Instances nest, as when a hit's
	 * death deals damage of its own.
	 */
	void BeginDealing(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, EVeyraDamageDelivery Delivery);
	FVeyraDamageDealtEvent EndDealing();

private:
	/** The instances being dealt, innermost last. */
	TArray<FVeyraDamageDealtEvent, TInlineAllocator<2>> Dealing;
};
