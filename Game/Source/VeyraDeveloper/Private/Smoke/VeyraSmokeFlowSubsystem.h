// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Client/VeyraClientFlowTypes.h"
#include "Containers/Set.h"
#include "Content/VeyraContentId.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "VeyraSmokeFlowSubsystem.generated.h"

class AVeyraGameState;
class AVeyraPlayerController;
class IVeyraClientIntents;
struct FVeyraClientSnapshot;
namespace VeyraBackendProtocol
{
	struct FLobby;
}

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
 *   result. In a draft (-VeyraSmokeFlowMode= a Draft Pick mode, Smoke.ps1 -Flow Draft, ADR-042) it
 *   bans in its ban turns, from the roster's end so that neither player's Vanguard is banned, and
 *   locks in its pick turn. With -VeyraSmokeFlowEndsMatch it walks and ends the match from the in-match menu's
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
 * The custom lobby's scripts (Smoke.ps1 -Flow Custom, ADR-021) run in two games at once, each naming
 * the other's player with -VeyraSmokeFlowFriend=. First they become friends: the host asks by name in
 * the friends panel, and each accepts a request from the other. Then:
 * - customhost opens a Custom Game from Play, invites its friend, keeps itself on side A and the friend
 *   on side B, seats one bot a side playing the Vanguards -VeyraSmokeFlowBots= names (side A's, then
 *   side B's), chooses a starting Gold the lobby offers, sees victory on and starts the game.
 * - customguest joins from its friend's invitation and waits in the lobby for the start.
 * Both then pick as the matchmade scripts do; with -VeyraSmokeFlowSieges the host wins by siege, and
 * each checks the verified custom result, its bots and the starting Gold the host chose.
 *
 * The party's scripts (Smoke.ps1 -Flow Party, ADR-044) run in two games at once, each naming the other's
 * player with -VeyraSmokeFlowFriend=, and queue for the co-op mode -VeyraSmokeFlowMode= names. First they
 * become friends, the leader asking. Then:
 * - partyleader leaves any party an earlier run left, invites its friend from the friends panel, and once
 *   they are in, opens their member card, asks Make Party Leader and confirms it (UX-11); it readies up
 *   once the new leader chose the mode.
 * - partymember joins from the invitation, and once it leads, chooses the mode in Play, readies up and
 *   finds the match.
 * Both accept, pick and play; with -VeyraSmokeFlowSieges the first wins by siege, and each must see
 * Victory, since a co-op party shares its side.
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
		CustomHost,
		CustomGuest,
		/** Settings (ADR-024 §8): changes the match's Display Mode and a binding in the Settings screen, and waits for the account to take them. */
		SettingsChange,
		/** The next start: finds both kept, the device's and the account's, and puts them back. */
		SettingsCheck,
		/** A party (ADR-044): invites its friend, hands them leadership after confirming, and readies up. */
		PartyLeader,
		/** A party: joins its friend's party from the invitation, then leads it into the queue. */
		PartyMember,
		/** The Collection (ADR-045 §8): buys a Vanguard nobody lends it through the confirmation, then practises with it. */
		Collection,
		/** Chat (ADR-046): invites its friend into a party, then exchanges Party Chat and direct messages with them. */
		ChatLeader,
		/** Chat: joins its friend's party from the invitation, then exchanges Party Chat and direct messages with them. */
		ChatMember,
	};

	bool Tick(float DeltaSeconds);
	void TickScript(IVeyraClientIntents& Flow);
	/** The shell of a matchmade script: into the queue, and back from a match found that did not go ahead. */
	void TickMatchmadeShell(IVeyraClientIntents& Flow);
	void TickMatchFound(IVeyraClientIntents& Flow);
	/** A custom lobby's scripts in the shell: friends with the other client, then into the lobby, the host from Play and the guest by its invitation. */
	void TickCustomShell(IVeyraClientIntents& Flow);
	/** A custom lobby's scripts in the lobby: the host invites, seats, sets the rules and starts; the guest waits. */
	void TickCustomLobby(IVeyraClientIntents& Flow);
	/** Friends with the other client: the host asks by name, and either accepts the other's request. True once they are friends. */
	bool TickFriendship(IVeyraClientIntents& Flow);
	/** The party's scripts in the shell: friends, then the party formed and its leadership handed over, then the queue. */
	void TickPartyShell(IVeyraClientIntents& Flow);
	bool IsParty() const { return Script == EScript::PartyLeader || Script == EScript::PartyMember; }
	/**
	 * The chat scripts in the shell (ADR-046): friends, then a party formed by invitation, then a Party Chat
	 * line each way through the sidebar, then a direct message each way from the friend's card.
	 */
	void TickChatShell(IVeyraClientIntents& Flow);
	bool IsChat() const { return Script == EScript::ChatLeader || Script == EScript::ChatMember; }
	/** Types Text into the chat composer that shows, as the player would. */
	bool TypeChat(const FString& Text);
	/**
	 * Whether this run's own line Text in Conversation has gone: false while it waits, and false, having failed
	 * the script, if it did not go.
	 */
	bool ChatLineSent(const FVeyraChatConversation& Conversation, const FString& Text, const FString& AccountId);
	/** Whether Conversation holds SenderName's confirmed line Text. */
	static bool ChatHasLine(const FVeyraChatConversation* Conversation, const FString& SenderName, const FString& Text);
	/** The party scripts in champion select (ADR-046 §6): "/p" through the select's one chat panel, then the friend's line read. True while it waits. */
	bool TickSelectChat(IVeyraClientIntents& Flow);
	/** The party scripts in the match: a direct message to the friend with the composer's /msg, then theirs read. True while it waits. */
	bool TickMatchDirect(IVeyraClientIntents& Flow);
	/**
	 * The party scripts on the results screen: the post-match chat joined by a first message, and answered once the
	 * friend's line shows, so whichever joined last still reads the other (UX-59). True while it waits.
	 */
	bool TickPostMatchChat(IVeyraClientIntents& Flow);
	/**
	 * Party scripts, on the results screen after the post-match chat: the leader commends the other member and the
	 * member files a test report about the leader, each through the player menu (ADR-047 §5). True while it has more to do.
	 */
	bool TickConduct(IVeyraClientIntents& Flow);
	/** Types Text into the open report form's details, as the player would. */
	bool TypeReportDetails(const FString& Text);
	/** The Collection's purchase, before the script practises with what it bought: opens the page, a card, its Buy and the confirmation. */
	void TickCollection(IVeyraClientIntents& Flow);
	/** Whether the script plays a practice match: Practice, and Collection after its purchase. */
	bool IsPracticeRules() const { return Script == EScript::Practice || Script == EScript::Collection; }
	/** The host's bots: removes a bot the script did not ask for, then seats the one each side lacks. True while it changes them. */
	bool TickLobbyBots(const VeyraBackendProtocol::FLobby& Lobby);
	/** Types Name into the friends panel's name field, as the player would. */
	bool TypeFriendName(const FString& Name);
	/** Whether the shell shows the lobby's bot picker. */
	bool IsBotPickerOpen() const;
	bool IsCustom() const { return Script == EScript::CustomHost || Script == EScript::CustomGuest; }
	/** The sparring partner: every state, through the intents. */
	void TickOpponent(IVeyraClientIntents& Flow);
	void TickInMatch();

	/** -VeyraSmokeFlowReconnects: leaves the live match once, then checks it came back to its Vanguard. True while it does. */
	bool TickReconnect(AVeyraPlayerController& Controller, UWorld& World);

	/** -VeyraSmokeFlowAwaitsReturn: true until it has seen another player leave the match and come back. */
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
	 * Practice: opens the chat composer as the chat key does, sends a line to Team Chat, and waits until
	 * the server delivers it back to the player, who is on their own side (ADR-029 §1). True while it waits.
	 */
	bool TickChat(AVeyraPlayerController& Controller);

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

	/** The settings scripts in the shell, one step a tick. */
	void TickSettings();
	bool IsSettings() const { return Script == EScript::SettingsChange || Script == EScript::SettingsCheck; }
	int32 SettingsStep = 0;

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

	/** The label of a mode's card on the Play page. */
	static FString ModeLabel(const FString& ModeId);

	/**
	 * With -VeyraSmokeFlowScreenshots, asks once for a screenshot called Name and waits a moment for
	 * it to be saved. True if it asked now, so the caller acts on a later tick.
	 */
	bool Capture(const TCHAR* Name);

	/** Which Vanguard to take from those on offer; empty if none fits. */
	FString ChooseFrom(const TArray<FString>& Offered) const;

	/** In the player's ban turn, hovers a ban, then bans it. */
	void TickBan(const FVeyraClientSnapshot& Snapshot);
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
	/** The most other players it has seen in the match at once. */
	int32 MostOthers = 0;
	FString WantedVanguard;
	FString LockedVanguard;
	/** A matchmade script: the mode it was asked to queue for, and whether that mode is against AI (ADR-039 §6). */
	FString WantedMode;
	bool bVersusAI = false;
	FString ScreenshotFolder;
	TSet<FString> Captured;
	/** Real time until which the script waits, for a screenshot to be saved. */
	double HoldUntil = 0.0;
	/** Whether the script has done each step, so it does each once. */
	bool bOpenedPlay = false;
	bool bStartedPractice = false;
	/** Collection: the purchase's steps, and what it bought for how much Flux. */
	bool bOpenedCollection = false;
	bool bOpenedCollectionCard = false;
	bool bAskedToBuy = false;
	bool bConfirmedBuy = false;
	bool bPurchased = false;
	FString BoughtVanguard;
	int64 BoughtPrice = 0;
	/** Matchmade: the mode chosen, then Ready, Find Match, the answer to the match found and Cancel. */
	FString ChosenMode;
	bool bReadied = false;
	bool bFoundMatch = false;
	bool bAnswered = false;
	bool bCancelledQueue = false;
	/** Practice: the item the shop bought, the Flux Spell slot 1 swapped to, and whether both arrived and the shop closed. */
	FString BoughtItem;
	FString SwappedSpell;
	/** Practice: the item with Crit Chance bought after them, and whether developer Gold was asked for it (ADR-023). */
	FString CritItem;
	bool bAskedCritGold = false;
	bool bShopped = false;
	/** Practice: whether the scoreboard was shown, checked and let go. */
	bool bScoreboardChecked = false;
	/** Practice: when the Team Chat line went, and whether it came back. */
	double ChatSentAt = 0.0;
	bool bChatChecked = false;
	/** Practice: whether Match History was opened, and the match found and opened in it. */
	bool bOpenedHistory = false;
	bool bCheckedHistory = false;
	/** The match whose verified result the script saw. */
	FString PlayedMatchId;
	/** Custom: the other client's player, the bots' Vanguards (side A's, then side B's), and what the script has done. */
	FString FriendName;
	TArray<FString> BotVanguards;
	bool bAskedFriend = false;
	bool bFriends = false;
	bool bOpenedCustom = false;
	bool bInLobby = false;
	bool bStartedLobby = false;
	/** Real time before which the host does not invite its friend again. */
	double NextInviteAt = 0.0;
	/** Party: whether the leader left an earlier run's party, the member joined by this run's invitation, the
	 * two are in one party, and how far the leader's handover has gone (card, Make Party Leader, Confirm). */
	bool bLeftOldParty = false;
	bool bJoinedByInvite = false;
	bool bPartyFormed = false;
	int32 HandoverStep = 0;
	/**
	 * Chat: how far the exchange has gone (send Party Chat, read the friend's, open their card, Message, send a
	 * direct message, read theirs), and this run's tag on its lines: the party's, which both scripts share, so an
	 * earlier run's lines never count.
	 */
	int32 ChatStep = 0;
	FString ChatRunTag;
	/** The party scripts' chat in champion select, the match and the results: how far each has gone. */
	int32 SelectChatStep = 0;
	int32 MatchDirectStep = 0;
	int32 PostMatchStep = 0;
	/** How far the party script has come with its commendation or report on the results screen. */
	int32 ConductStep = 0;
	/** The starting Gold the lobby set for its match, which the verified scoreboard must show; unset for the game's own. */
	TOptional<double> LobbyStartingGold;
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
