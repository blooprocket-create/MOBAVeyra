// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Content/VeyraContentId.h"
#include "GameplayEffectTypes.h"

#include "VeyraCooldownComponent.generated.h"

/** Which Haste shortens a cooldown (Combat Bible §21): Ability Haste and Item Haste never cross-apply. */
enum class EVeyraCooldownHaste : uint8
{
	/** A Vanguard's ability: Ability Haste shortens it, and a change of it rescales what remains. */
	Ability,
	/** An item's Active: Item Haste's, which nothing grants yet; Ability Haste leaves it alone. */
	Item,
	/** A Flux Spell's: fixed, as no Haste of any kind shortens it (Combat Bible §21; ADR-015 §2). */
	Fixed,
};

/** One ability's cooldown: when it is ready again, in server gameplay time, and how long it was. */
USTRUCT()
struct FVeyraCooldownEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FVeyraContentId Ability;

	UPROPERTY()
	double ReadyAt = 0.0;

	/** The duration it started with, kept so Ability Haste can rescale it later (Combat Bible §21). */
	UPROPERTY()
	double DurationSeconds = 0.0;

	/** Whether Ability Haste rescales it: false for an item's Active. Server only. */
	UPROPERTY(NotReplicated)
	bool bAbilityHaste = true;
};

/** The cooldown ledger's arithmetic, as plain functions of gameplay time. */
namespace VeyraCooldowns
{
	/** Seconds until Ability is ready at time Now; 0 when it is ready. */
	VEYRAABILITIES_API double RemainingSeconds(TConstArrayView<FVeyraCooldownEntry> Entries, const FVeyraContentId& Ability, double Now);

	/** Starts Ability's cooldown at time Now, replacing any running one. bAbilityHaste: whether Ability Haste rescales it. */
	VEYRAABILITIES_API void Start(TArray<FVeyraCooldownEntry>& Entries, const FVeyraContentId& Ability, double DurationSeconds, double Now,
		bool bAbilityHaste = true);

	/** Forgets Ability's cooldown, so it is ready. False if it had none. */
	VEYRAABILITIES_API bool Clear(TArray<FVeyraCooldownEntry>& Entries, const FVeyraContentId& Ability);

	/**
	 * Shortens Ability's cooldown still running at time Now by Fraction of what remains, then by Seconds
	 * more, never below ready (ADR-030 §3). False if it was ready.
	 */
	VEYRAABILITIES_API bool Reduce(TArray<FVeyraCooldownEntry>& Entries, const FVeyraContentId& Ability, double Now, double Fraction, double Seconds = 0.0);

	/**
	 * Scales every Ability-Haste cooldown still running at time Now by Factor, what remains and what it
	 * started with alike, as when Ability Haste changes mid-cooldown (Combat Bible §21). Finished ones
	 * stay finished; items' cooldowns are left alone.
	 */
	VEYRAABILITIES_API void Rescale(TArray<FVeyraCooldownEntry>& Entries, double Factor, double Now);
}

/**
 * A combatant's ability cooldowns (ADR-006 §4 amendment: a Veyra ledger, not Gameplay Effects). It
 * lives on the PlayerState, so cooldowns keep running through death and respawn (Combat Bible §44).
 * It counts in the server's world time, which stops while the match is paused (ADR-006 §8).
 * Replicated to the owner and to replays for the HUD.
 */
UCLASS(ClassGroup = Abilities)
class VEYRAABILITIES_API UVeyraCooldownComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVeyraCooldownComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	/**
	 * Server only: starts Ability's cooldown now. A Vanguard ability's BaseSeconds is shortened by the
	 * owner's Ability Haste (Combat Bible §21), and a change of Haste while it runs rescales what
	 * remains; an item's Active is not.
	 */
	void StartCooldown(const FVeyraContentId& Ability, double BaseSeconds, EVeyraCooldownHaste Haste = EVeyraCooldownHaste::Ability);

	/** Server only: makes Ability ready now, forgetting any cooldown it still has. */
	void ClearCooldown(const FVeyraContentId& Ability);

	/** Server only: shortens Ability's running cooldown by Fraction of what remains, then by Seconds (ADR-030 §3). */
	void ReduceCooldown(const FVeyraContentId& Ability, double Fraction, double Seconds = 0.0);

	/** Server only: makes every ability ready now. Returns how many were still cooling down. */
	int32 ClearAllCooldowns();

	/**
	 * Seconds until Ability is ready at server gameplay time Now. The server passes its world time;
	 * a client passes the GameState's gameplay server time.
	 */
	double GetRemainingSeconds(const FVeyraContentId& Ability, double Now) const;

	/** Seconds until Ability is ready, now, on the server or a client (see GetServerNow). */
	double GetRemainingSecondsNow(const FVeyraContentId& Ability) const;

	/** The duration Ability's current cooldown started with, or 0 if it has none. */
	double GetDurationSeconds(const FVeyraContentId& Ability) const;

private:
	/**
	 * Server world time, which the ledger counts in. On the server that is its own world clock. A
	 * client's world clock started when it loaded the map, so a client uses the game state's
	 * synchronized estimate of the server's clock instead.
	 */
	double GetServerNow() const;

	/** The owner's Ability Haste changed: running cooldowns keep their proportion (§21). Server only. */
	void OnAbilityHasteChanged(const FOnAttributeChangeData& Change);

	UPROPERTY(Replicated)
	TArray<FVeyraCooldownEntry> Entries;

	FDelegateHandle HasteHandle;
};
