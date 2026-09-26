// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "VeyraNetStatsSubsystem.generated.h"

/**
 * A developer statistics logger for the M3 network spikes (ADR-006 §5): the replay cost and the
 * bandwidth sweep. On a dedicated server whose map URL or command line carries
 * VeyraNetStats=<seconds>, it logs one "VeyraNetStats:" line per interval with:
 * - the server's busy time per frame, the frame time minus the idle wait before the next tick;
 * - the bytes per second sent to clients, in total, per client and at most, and received;
 * - the bytes per second written to the replay, when one is recording;
 * - how many replicated actors exist.
 */
UCLASS()
class UVeyraNetStatsSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return IntervalSeconds > 0.0; }
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual TStatId GetStatId() const override;

private:
	void Report(double WindowSeconds);

	double IntervalSeconds = 0.0;
	double WindowStartRealTime = 0.0;
	int32 Frames = 0;
	double BusySecondsSum = 0.0;
	double BusySecondsMax = 0.0;
};
