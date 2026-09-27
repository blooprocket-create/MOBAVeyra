// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "VeyraSmokeFlowSubsystem.generated.h"

class UVeyraClientFlowSubsystem;

/**
 * A scripted player for the client-state coordinator (ADR-010 §2), started by -VeyraSmokeFlow= in a
 * game that signs in with -VeyraLaunchCode=stdin. It asks only through the coordinator's intents, as
 * the shell's screens do.
 *
 * -VeyraSmokeFlow=join presses Reconnect when the coordinator offers it: a script created the match
 * before the game started, and -VeyraSmoke plays it.
 *
 * -VeyraSmokeFlow=practice plays the whole solo path (Game/Scripts/Smoke.ps1 -Flow Practice): it
 * chooses a starter if asked, starts practice, hovers and locks a Vanguard, and once its match is
 * live walks toward the lane centre and ends it as the host (End Custom Match). It then checks the
 * verified result, continues to the shell and quits. -VeyraSmokeFlowVanguard=<id> names the Vanguard; otherwise it takes the first on
 * offer. It logs "VeyraSmoke: PASS" or "VeyraSmoke: FAIL", which is its result.
 */
UCLASS()
class UVeyraSmokeFlowSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	bool Tick(float DeltaSeconds);
	void TickPractice(UVeyraClientFlowSubsystem& Flow);
	/** Which Vanguard to take from those on offer; empty if none fits. */
	FString ChooseFrom(const TArray<FString>& Offered) const;
	void Finish(bool bPassed, const FString& Reason);

	bool bPractice = false;
	FString WantedVanguard;
	FString LockedVanguard;
	/** Practice: whether the script has asked for each step, so it asks once. */
	bool bStartedPractice = false;
	bool bOrderedMove = false;
	bool bAskedToEnd = false;
	FVector MoveStart = FVector::ZeroVector;
	bool bSawResults = false;
	bool bFinished = false;
	double StartRealTime = 0.0;
	FTSTicker::FDelegateHandle TickHandle;
};
