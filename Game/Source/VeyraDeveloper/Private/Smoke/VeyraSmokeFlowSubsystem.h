// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Set.h"
#include "Content/VeyraContentId.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "VeyraSmokeFlowSubsystem.generated.h"

class AVeyraGameState;
class AVeyraPlayerController;
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
 * continues to the shell and quits. With -VeyraSmokeFlowSieges it first sieges the bots' side with
 * Veyra.Dev.Siege until their Prime Well falls, and the match must go on (ADR-011 §14).
 *
 * The matchmade scripts (Smoke.ps1 -Flow Casual and -Flow CasualDecline) run in two games at once.
 * Each chooses a starter if asked, chooses the first matchmade mode in Play, readies up and finds a
 * match. Then:
 * - casual accepts, hovers and locks its Vanguard, plays the standard match and checks its verified
 *   result. With -VeyraSmokeFlowEndsMatch it walks and ends the match from the in-match menu's
 *   developer end; otherwise it waits for the end. With -VeyraSmokeFlowVictory the match must end in
 *   a win (Smoke.ps1 -Flow CasualVictory, ADR-011 §13): -VeyraSmokeFlowSieges wins it with
 *   Veyra.Dev.Siege and must see Victory; the other must see Defeat. With -VeyraSmokeFlowReconnects
 *   it leaves the live match and presses Reconnect, and must come back to the Vanguard it locked
 *   (Smoke.ps1 -Flow CasualReconnect, ADR-019 §1); with -VeyraSmokeFlowAwaitsReturn it ends the match
 *   only once it has seen the other player leave and come back.
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

	/** -VeyraSmokeFlowReconnects: leaves the live match once, then checks it came back to its Vanguard. True while it does. */
	bool TickReconnect(AVeyraPlayerController& Controller, UWorld& World);

	/** -VeyraSmokeFlowAwaitsReturn: true until it has seen another player's PlayerState go inactive and come back. */
	bool TickAwaitReturn(const AVeyraPlayerController& Controller, const AVeyraGameState& GameState);

	/**
	 * With -VeyraSmokeFlowSieges: asks for a developer siege at a steady pace. A match that ends by it
	 * leaves the match; a practice match stops at the enemy Prime Well. True while it sieges, so the
	 * rest of the match script waits.
	 */
	bool TickSiege(AVeyraPlayerController& Controller, const UWorld& World);

	/**
	 * Practice: at the fountain, opens the shop as its key does and buys the cheapest piece of
	 * equipment the starting Gold affords by clicking it, then closes the shop once the item arrives
	 * (ADR-012 §11). True while it shops, so the rest of the match script waits.
	 */
	bool TickShop(AVeyraPlayerController& Controller);

	/**
	 * Practice: shows the in-match scoreboard as holding its key does, waits until it shows the player
	 * on its own team with a Vanguard and a level, captures it and lets it go (ADR-017 §4). True while
	 * it waits.
	 */
	bool TickScoreboard(AVeyraPlayerController& Controller);

	/**
	 * Practice: once the Vanguard has walked away from its fountain, recalls home and waits for it to
	 * arrive (ADR-012 §8). True while it recalls.
	 */
	bool TickRecall(AVeyraPlayerController& Controller, const AActor& Vanguard);

	/**
	 * Practice, back in the shell: opens Match History as a player does, checks the match just played is
	 * listed first, opens it into its saved scoreboard and goes back (UX-51). True while it looks.
	 */
	bool TickHistory(const IVeyraClientIntents& Flow);
	void CheckResults(const FVeyraClientSnapshot& Snapshot);
	bool IsMatchmade() const { return Script == EScript::Casual || Script == EScript::Decline || Script == EScript::Requeue; }

	/**
	 * Clicks the shell's button labelled Label, the Occurrence-th where several share it. False, having
	 * failed the script, if it cannot.
	 */
	bool Click(const FString& Label, int32 Occurrence = 0);

	/**
	 * In champion select, clicks the next Flux Spell choice the script wants: the roster's first
	 * spells, one per slot, as a player would (ADR-015 §7). True once every slot holds its spell.
	 */
	bool ChooseFluxSpells(const FVeyraClientSnapshot& Snapshot, IVeyraClientIntents& Flow);

	/** The label of a Flux Spell's button. */
	static FString SpellLabel(const FVeyraContentId& SpellId);

	/** The label of the tile that opens Flux Spell slot Slot's picker, from 0. */
	static FString SpellSlotLabel(int32 Slot);

	/** The Flux Spell slot whose picker the shell shows, or INDEX_NONE. */
	int32 OpenSpellSlot() const;

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
	/** This game sieges with Veyra.Dev.Siege, and whether it has finished, how often it asked and when it asks next. */
	bool bSieges = false;
	bool bSiegeDone = false;
	int32 SiegeRequests = 0;
	double NextSiegeAt = 0.0;
	/** Casual: the match must end in a win, this game's if it sieges. */
	bool bVictory = false;
	/** Casual: this game leaves the live match and comes back, and whether it has done each. */
	bool bReconnects = false;
	bool bLeft = false;
	bool bCameBack = false;
	double LeftAtRealSeconds = 0.0;
	/** Casual: this game ends the match only once another player left and came back, and what it saw. */
	bool bAwaitsReturn = false;
	bool bSawAway = false;
	bool bSawReturn = false;
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
	/** Practice: the item the shop bought, the Flux Spell slot 1 swapped to, and whether both arrived and the shop closed. */
	FString BoughtItem;
	FString SwappedSpell;
	bool bShopped = false;
	/** Practice: whether the scoreboard was shown, checked and let go. */
	bool bScoreboardChecked = false;
	/** Practice: whether Match History was opened, and the match found and opened in it. */
	bool bOpenedHistory = false;
	bool bCheckedHistory = false;
	/** The match whose verified result the script saw. */
	FString PlayedMatchId;
	/** Practice: whether the script asked to recall, saw the channel, and saw the Vanguard home. */
	bool bAskedToRecall = false;
	bool bSawRecall = false;
	bool bRecalled = false;
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
