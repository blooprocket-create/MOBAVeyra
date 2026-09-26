// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "VeyraReplayCheckSubsystem.generated.h"

class AActor;

/**
 * A scripted replay viewer for the ADR-006 §5 replay spike (Game/Scripts/Smoke.ps1 -RecordReplay).
 * It exists only when the game starts with -VeyraReplayCheck=<replay name>. It plays that replay
 * faster than real time and checks that it shows what the smoke match did: both Vanguards, one of
 * them moving, a Vanguard losing Health to a cast, and the match pausing. Like any replay viewer,
 * it plays through the match's recorded world pause (ADR-006 §5). It logs
 * "VeyraReplayCheck: PASS" or "VeyraReplayCheck: FAIL", which is its result, and quits.
 */
UCLASS()
class UVeyraReplayCheckSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	bool Tick(float DeltaSeconds);
	void Observe(const UWorld& World);
	void Finish(bool bPassed, const FString& Reason);

	FString ReplayName;
	FTSTicker::FDelegateHandle TickHandle;
	double StartRealTime = 0.0;
	double LastProgressRealTime = 0.0;
	bool bStarted = false;
	bool bPlaying = false;
	bool bFinished = false;

	// What the replay has shown so far.
	int32 MostVanguards = 0;
	bool bSawMovement = false;
	bool bSawDamage = false;
	bool bSawPause = false;
	bool bClearedRecordedPause = false;
	TMap<TWeakObjectPtr<AActor>, FVector> FirstLocations;
};
