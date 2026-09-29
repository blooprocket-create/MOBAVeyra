// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Battleground/VeyraBattlegroundTypes.h"
#include "Brain/VeyraBotView.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "Math/RandomStream.h"
#include "VeyraMatchTypes.h"

#include "VeyraBotBrainComponent.generated.h"

class AVeyraGameMode;
class AVeyraPlayerState;

/**
 * A bot's brain (ADR-013 §4): on its Vanguard controller, on the server, it thinks on a world-time
 * timer, so a pause holds it. Each decision it senses, decides by VeyraBotRules, and acts through the
 * game mode's order paths as a player's controller does; it shops and spends skill points when the
 * match allows them. It re-issues an order only when its intent changes.
 */
UCLASS(ClassGroup = Bots)
class VEYRABOTS_API UVeyraBotBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVeyraBotBrainComponent();

	/**
	 * Before registering: the participant it plays, its role and difficulty, whether its seat wards
	 * (Bots.json warding), and the seed of its chances.
	 */
	void Configure(AVeyraPlayerState& InBot, EVeyraBotRole InRole, EVeyraBotDifficulty InDifficulty, bool bInWards, int32 Seed);

	/** One decision now, acted on. The timer calls it; tests call it directly. */
	FVeyraBotIntent Think();

	EVeyraBotRole GetRole() const { return Role; }
	EVeyraLane GetLane() const { return VeyraBots::LaneOf(Role); }
	EVeyraBotDifficulty GetDifficulty() const { return Difficulty; }

	/** The intent it last acted on. */
	const FVeyraBotIntent& GetLastIntent() const { return LastIntent; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Shop(const FVeyraBotView& View, AVeyraGameMode& GameMode);
	void RankUp(AVeyraGameMode& GameMode);
	void Act(const FVeyraBotIntent& Intent, AVeyraGameMode& GameMode);

	TWeakObjectPtr<AVeyraPlayerState> Bot;
	EVeyraBotRole Role = EVeyraBotRole::Mid;
	EVeyraBotDifficulty Difficulty = EVeyraBotDifficulty::Beginner;
	bool bWards = false;
	FVeyraBotMemory Memory;
	FRandomStream Random;
	FVeyraBotIntent LastIntent;
	FTimerHandle Timer;
};
