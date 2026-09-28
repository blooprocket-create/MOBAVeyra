// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Content/VeyraContentId.h"
#include "GameplayEffectTypes.h"

#include "VeyraCooldownComponent.generated.h"

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
};

/** The cooldown ledger's arithmetic, as plain functions of gameplay time. */
namespace VeyraCooldowns
{
	/** Seconds until Ability is ready at time Now; 0 when it is ready. */
	VEYRAABILITIES_API double RemainingSeconds(TConstArrayView<FVeyraCooldownEntry> Entries, const FVeyraContentId& Ability, double Now);

	/** Starts Ability's cooldown at time Now, replacing any running one. */
	VEYRAABILITIES_API void Start(TArray<FVeyraCooldownEntry>& Entries, const FVeyraContentId& Ability, double DurationSeconds, double Now);

	/**
	 * Scales every cooldown still running at time Now by Factor, what remains and what it started with
	 * alike, as when Ability Haste changes mid-cooldown (Combat Bible §21). Finished ones stay finished.
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
	 * Server only: starts Ability's cooldown now, BaseSeconds shortened by the owner's Ability Haste
	 * (Combat Bible §21). While it runs, a change of Haste rescales what remains.
	 */
	void StartCooldown(const FVeyraContentId& Ability, double BaseSeconds);

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
