// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Content/VeyraContentId.h"

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
}

/**
 * A combatant's ability cooldowns (ADR-006 §4 amendment: a Veyra ledger, not Gameplay Effects). It
 * lives on the PlayerState, so cooldowns keep running through death and respawn (Combat Bible §44).
 * It counts in world time, which stops while the match is paused (ADR-006 §8). Replicated to the
 * owner and to replays for the HUD.
 */
UCLASS(ClassGroup = Abilities)
class VEYRAABILITIES_API UVeyraCooldownComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVeyraCooldownComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server only: starts Ability's cooldown now. */
	void StartCooldown(const FVeyraContentId& Ability, double DurationSeconds);

	/**
	 * Seconds until Ability is ready at server gameplay time Now. The server passes its world time;
	 * a client passes the GameState's gameplay server time.
	 */
	double GetRemainingSeconds(const FVeyraContentId& Ability, double Now) const;

	/** Server only: seconds until Ability is ready, now. */
	double GetRemainingSecondsNow(const FVeyraContentId& Ability) const;

	/** The duration Ability's current cooldown started with, or 0 if it has none. */
	double GetDurationSeconds(const FVeyraContentId& Ability) const;

private:
	UPROPERTY(Replicated)
	TArray<FVeyraCooldownEntry> Entries;
};
