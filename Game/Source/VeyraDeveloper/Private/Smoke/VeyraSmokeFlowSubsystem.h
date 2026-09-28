// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Set.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "VeyraSmokeFlowSubsystem.generated.h"

class IVeyraClientIntents;
struct FVeyraClientSnapshot;

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
 * continues to the shell and quits.
 *
 * The matchmade scripts (Smoke.ps1 -Flow Casual and -Flow CasualDecline) run in two games at once.
 * Each chooses a starter if asked, chooses the first matchmade mode in Play, readies up and finds a
 * match. Then:
 * - casual accepts, hovers and locks its Vanguard, plays the standard match and checks its verified
 *   result. With -VeyraSmokeFlowEndsMatch it walks and ends the match from the in-match menu's
 *   developer end; otherwise it waits for the end.
 * - decline declines the match found, and passes once it is back in the shell out of the queue.
 * - requeue accepts; when another player declines, it must be back in the queue. It cancels the
 *   queue and passes.
 *
 * -VeyraSmokeFlow=opponent is not a test but a sparring partner for a person playing the matchmade
 * path (Game/Scripts/Play.ps1 -Opponent). It queues for the first matchmade mode and accepts every
 * match found; in champion select it locks a Vanguard nobody has locked once the other team has
 * locked, or late on the timer; in the match it stands still; after the results it queues again. It
 * asks through the coordinator's intents, retries what can be retried, and runs until it is closed.
 *
 * -VeyraSmokeFlowVanguard=<id> names the Vanguard; otherwise it takes the first on offer.
 * -VeyraSmokeFlowScreenshots=<folder> saves each screen on the way, in a rendering client. It logs
 * "VeyraSmoke: PASS" or "VeyraSmoke: FAIL", which is its result.
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
	/** What the script plays. */
	enum class EScript : uint8
	{
		Join,
		Practice,
		Casual,
		Decline,
		Requeue,
		Opponent,
	};

	bool Tick(float DeltaSeconds);
	void TickScript(IVeyraClientIntents& Flow);
	/** The shell of a matchmade script: into the queue, and back from a match found that did not go ahead. */
	void TickMatchmadeShell(IVeyraClientIntents& Flow);
	void TickMatchFound(IVeyraClientIntents& Flow);
	/** The sparring partner: every state, through the intents. */
	void TickOpponent(IVeyraClientIntents& Flow);
	void TickInMatch();
	void CheckResults(const FVeyraClientSnapshot& Snapshot);
	bool IsMatchmade() const { return Script == EScript::Casual || Script == EScript::Decline || Script == EScript::Requeue; }

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

	EScript Script = EScript::Join;
	/** Casual: this game ends the match; the other waits for the end. */
	bool bEndsMatch = false;
	FString WantedVanguard;
	FString LockedVanguard;
	FString ScreenshotFolder;
	TSet<FString> Captured;
	/** Real time until which the script waits, for a screenshot to be saved. */
	double HoldUntil = 0.0;
	/** Whether the script has done each step, so it does each once. */
	bool bOpenedPlay = false;
	bool bStartedPractice = false;
	/** Matchmade: the mode chosen, then Ready, Find Match, the answer to the match found and Cancel. */
	FString ChosenMode;
	bool bReadied = false;
	bool bFoundMatch = false;
	bool bAnswered = false;
	bool bCancelledQueue = false;
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
