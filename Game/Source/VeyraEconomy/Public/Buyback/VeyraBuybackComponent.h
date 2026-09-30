// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Buyback/VeyraBuybackRules.h"
#include "Components/ActorComponent.h"

#include "VeyraBuybackComponent.generated.h"

class UVeyraGoldComponent;

/**
 * One participant's buybacks (Economy & Progression Bible §15; ADR-020 §3): how many it has bought this
 * match, and when its cooldown ends. It sits on the PlayerState beside its Gold, and its owner sees both,
 * so the shop can price the next one. Economy owns the transaction; the match respawns the Vanguard, and
 * nothing about its death is undone.
 */
UCLASS()
class VEYRAECONOMY_API UVeyraBuybackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBoughtBack, double /*Cost*/);

	UVeyraBuybackComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Owner and server only: buybacks bought this match. */
	int32 GetPurchases() const { return Purchases; }

	/** Owner and server only: when, in the server's world time, the next may be bought; 0 before the first. */
	double GetReadyAt() const { return ReadyAt; }

	/** The next buyback at MatchSeconds and Now, for a Vanguard dead or not, paid from Gold. */
	FVeyraBuybackQuote Quote(double MatchSeconds, double Now, bool bDead, const UVeyraGoldComponent& Gold) const;

	/**
	 * Server only: buys back, paying from Gold and starting the cooldown, or refuses, changing nothing. The
	 * caller respawns the Vanguard.
	 */
	EVeyraBuybackRefusal Buy(double MatchSeconds, double Now, bool bDead, UVeyraGoldComponent& Gold);

	/** Server: each buyback as it is bought, for the match statistics (ADR-017 §2). */
	FOnBoughtBack OnBoughtBack;

private:
	UPROPERTY(Replicated)
	int32 Purchases = 0;

	UPROPERTY(Replicated)
	double ReadyAt = 0.0;
};
