// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Set.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "VeyraSmokeFlowSubsystem.generated.h"

class IVeyraClientIntents;

/**
 * A scripted player for the client-state coordinator (ADR-010 §2), started by -VeyraSmokeFlow= in a
 * game that signs in with -VeyraLaunchCode=stdin.
 *
 * -VeyraSmokeFlow=join presses Reconnect when the coordinator offers it: a script created the match
 * before the game started, and -VeyraSmoke plays it.
 *
 * -VeyraSmokeFlow=practice plays the whole solo path (Game/Scripts/Smoke.ps1 -Flow Practice) by
 * clicking the shell's buttons, as a player would: it chooses a starter if asked, goes to Play and
 * starts practice, hovers and locks a Vanguard, and once its match is live walks toward the lane
 * centre, opens the in-match menu and ends the match as its host. It then checks the verified result,
 * continues to the shell and quits. -VeyraSmokeFlowVanguard=<id> names the Vanguard; otherwise it
 * takes the first on offer. -VeyraSmokeFlowScreenshots=<folder> saves each screen on the way, in a
 * rendering client. It logs "VeyraSmoke: PASS" or "VeyraSmoke: FAIL", which is its result.
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
	void TickPractice(IVeyraClientIntents& Flow);
	void TickInMatch();

	/** Clicks the shell's button labelled Label. False, having failed the script, if it cannot. */
	bool Click(const FString& Label);

	/** The label of a Vanguard's button. */
	static FString VanguardLabel(const FString& VanguardId);

	/**
	 * With -VeyraSmokeFlowScreenshots, asks once for a screenshot called Name and waits a moment for
	 * it to be saved. True if it asked now, so the caller acts on a later tick.
	 */
	bool Capture(const TCHAR* Name);

	/** Which Vanguard to take from those on offer; empty if none fits. */
	FString ChooseFrom(const TArray<FString>& Offered) const;
	void Finish(bool bPassed, const FString& Reason);

	bool bPractice = false;
	FString WantedVanguard;
	FString LockedVanguard;
	FString ScreenshotFolder;
	TSet<FString> Captured;
	/** Real time until which the script waits, for a screenshot to be saved. */
	double HoldUntil = 0.0;
	/** Practice: whether the script has done each step, so it does each once. */
	bool bOpenedPlay = false;
	bool bStartedPractice = false;
	bool bOrderedMove = false;
	bool bOpenedMenu = false;
	bool bConfirmingEnd = false;
	bool bAskedToEnd = false;
	FVector MoveStart = FVector::ZeroVector;
	bool bSawResults = false;
	bool bFinished = false;
	double StartRealTime = 0.0;
	FTSTicker::FDelegateHandle TickHandle;
};
