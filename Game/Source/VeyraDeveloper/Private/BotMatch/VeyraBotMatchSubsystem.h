// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Subsystems/WorldSubsystem.h"
#include "VeyraMatchTypes.h"

#include "VeyraBotMatchSubsystem.generated.h"

/**
 * A match that bots play (ADR-013, M9 acceptance). On a dedicated server whose map URL carries
 * VeyraPlayingBots=<n>, it seats n playing bots when preparation begins: each joins the smaller side,
 * plays the next released Vanguard in turn, and has a brain like a practice match's bots.
 * VeyraBotDifficulty=Beginner or Intermediate sets how they play (Beginner without it), and
 * VeyraBotVanguards=<id>,<id>,... which Vanguards they take in turn instead. Every
 * minute of match clock it logs how the match stands, which Game/Scripts/Smoke.ps1 -PlayingBots
 * reads. Developer builds only.
 */
UCLASS()
class UVeyraBotMatchSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return BotCount > 0; }
	virtual TStatId GetStatId() const override;

private:
	void Seat(UWorld& World);
	void Report(UWorld& World) const;

	int32 BotCount = 0;
	/** The Vanguards the URL names for the bots, in order; empty for every one bots know. */
	TArray<FVeyraContentId> ChosenVanguards;
	EVeyraBotDifficulty Difficulty = EVeyraBotDifficulty::Beginner;
	bool bSeated = false;
	double NextReportAt = 0.0;
};
