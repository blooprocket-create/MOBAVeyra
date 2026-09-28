// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Damage/VeyraDamageTypes.h"
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

/** Damage dealt by a unit to a unit on the opposing side, as it lands. */
struct FVeyraHostileDamageEvent
{
	TWeakObjectPtr<UAbilitySystemComponent> Source;
	TWeakObjectPtr<UAbilitySystemComponent> Target;
	EVeyraDamageDelivery Delivery = EVeyraDamageDelivery::Ability;
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

	FOnDeath OnDeath;

	/** Hostile damage that was dealt, shields included (Battleground Bible §19: tower and Fluxborn aggro). */
	FOnHostileDamage OnHostileDamage;
};
