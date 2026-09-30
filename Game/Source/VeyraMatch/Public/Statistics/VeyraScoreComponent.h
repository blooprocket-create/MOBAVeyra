// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/ActorComponent.h"

#include "VeyraScoreComponent.generated.h"

/** What the in-match scoreboard shows of a player (ADR-017 §3, §9.4): public to everyone. */
USTRUCT()
struct FVeyraScore
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Kills = 0;

	UPROPERTY()
	int32 Deaths = 0;

	UPROPERTY()
	int32 Assists = 0;

	/** Last hits on Fluxborn. */
	UPROPERTY()
	int32 MinionKills = 0;

	/** Last hits on jungle creatures. */
	UPROPERTY()
	int32 JungleKills = 0;

	bool operator==(const FVeyraScore& Other) const = default;
};

/**
 * A participant's public score, on its PlayerState: every client receives it, whatever it sees, as
 * the in-match scoreboard shows it. Only the statistics service sets it; the rest of the record stays
 * on the server until the result (ADR-017 §3).
 */
UCLASS()
class VEYRAMATCH_API UVeyraScoreComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVeyraScoreComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const FVeyraScore& GetScore() const { return Score; }

	/** Server only. */
	void SetScore(const FVeyraScore& NewScore);

private:
	UPROPERTY(Replicated)
	FVeyraScore Score;
};
