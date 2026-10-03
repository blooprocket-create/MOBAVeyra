// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameFramework/PlayerController.h"
#include "Buyback/VeyraBuybackRules.h"
#include "Content/VeyraContentId.h"
#include "Input/VeyraCastInput.h"
#include "Input/VeyraCursorPicks.h"
#include "Input/VeyraInputSettings.h"
#include "Input/VeyraOrderMark.h"
#include "Inventory/VeyraInventoryRules.h"
#include "Progression/VeyraProgressionTypes.h"
#include "Templates/Function.h"
#include "Tools/VeyraVisionToolComponent.h"
#include "VeyraAbilityTypes.h"
#include "VeyraMatchTypes.h"
#include "Chat/VeyraChatTypes.h"
#include "Feedback/VeyraCombatTextTypes.h"
#include "Pings/VeyraPingTypes.h"
#include "Votes/VeyraVoteTypes.h"

#include "VeyraPlayerController.generated.h"

class AVeyraVanguardCharacter;
struct FVeyraCameraPreferences;

/**
 * A human player's connection to the match. It possesses nothing: it sends the player's intents to
 * the server, which validates them and drives the Vanguard through its AVeyraVanguardController
 * (ADR-006 §7). On the owning client it views the Vanguard.
 */
UCLASS()
class VEYRAMATCH_API AVeyraPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AVeyraPlayerController(const FObjectInitializer& ObjectInitializer);

	/** Owning client: asks the server to move this player's Vanguard to Destination. */
	void IssueMoveOrder(const FVector& Destination);

	/**
	 * Owning client: re-aims the current move order at Destination while the move button is held.
	 * Each update replaces the last, so it travels unreliably: a lost one is superseded by the next
	 * and never holds up the reliable orders behind it. The server checks it like any move order.
	 */
	void SteerMoveOrder(const FVector& Destination);

	/** Owning client: asks the server to attack Target with this player's Vanguard (ADR-009 §5). */
	void IssueAttackOrder(AActor* Target);

	/** Owning client: asks the server to attack-move this player's Vanguard to Destination, with the player's target preference. */
	void IssueAttackMoveOrder(const FVector& Destination);

	/** Owning client: asks the server to cast the ability in Slot at Target. */
	void IssueCastOrder(EVeyraAbilitySlot Slot, AActor* Target);

	/** Owning client: asks the server to cast the ability in Slot at Target, a unit, a ground point or both. */
	void IssueCastOrder(EVeyraAbilitySlot Slot, const FVeyraCastTarget& Target);

	/**
	 * Owning client: asks the server to begin a Recall home to the fountain (Economy & Progression
	 * Bible §10; ADR-012 §8). A refusal arrives as an order rejection.
	 */
	void RequestRecall();

	/** Shows the player's mastery emote, as its key does; the server refuses it within its cooldown (ADR-045 §9). */
	void RequestMasteryEmote();

	/**
	 * Owning client, developer builds: asks the server to pause or resume the match at once. Pause
	 * votes (Match Flow Bible §10) will replace it; Shipping servers refuse it.
	 */
	void RequestDeveloperPause(bool bPause);

	/**
	 * Owning client, developer builds: asks the server to end the match now (ADR-007 §8). Victory
	 * conditions and surrender votes (Match Flow Bible §8) will end real matches; Shipping servers
	 * refuse this.
	 */
	void RequestDeveloperEndMatch();

	/**
	 * Owning client: asks the server to end the custom match as its host (Custom Matches Bible §4;
	 * ADR-010 §7). The server refuses anyone but a practice match's host.
	 */
	void RequestEndCustomMatch();

	/**
	 * Owning client: starts a vote of Kind, which counts as its YES, or answers the open vote (Match
	 * Flow Bible §7–§10; ADR-019 §4). A refusal arrives through GetLastVoteRefusal.
	 */
	void RequestVote(EVeyraVoteKind Kind);
	void CastVote(bool bYes);

	/**
	 * Owning client: the open vote this player sees, if any: one for everyone from the game state, or
	 * its own team's, which reaches no one else (Match Flow Bible §9; ADR-019 §4).
	 */
	const FVeyraVoteState& GetOpenVote() const;

	/** Server only: the vote owner shows this player its team's open vote, or none. */
	void SetTeamVote(const FVeyraVoteState& InVote);

	/**
	 * Owning client: whether the server counts this player AFK, its Vanguard walked to safety until its
	 * next order (Match Flow Bible §5.1; ADR-019 §3).
	 */
	bool IsWarnedAfk() const { return bWarnedAfk; }

	/** Server: tells the owning client whether it is AFK. */
	void WarnAfk(bool bAfk) { ClientAbsenceWarning(bAfk); }

	/** Owning client: the reason the server gave for the last refused vote or ballot, and how many it refused. */
	EVeyraVoteRefusal GetLastVoteRefusal() const { return LastVoteRefusal; }
	int32 GetVoteRefusalCount() const { return VoteRefusalCount; }

	/** Owning client: the reason the server gave for the last refused End Custom Match, and how many it refused. */
	EVeyraEndCustomMatchRefusal GetLastEndCustomMatchRefusal() const { return LastEndCustomMatchRefusal; }
	int32 GetEndCustomMatchRefusalCount() const { return EndCustomMatchRefusalCount; }

	/** Owning client: asks the server to spend a skill point on the ability in Slot (Economy & Progression Bible §1). */
	void RequestRankUp(EVeyraAbilitySlot Slot);

	/**
	 * Owning client, developer builds: asks the server to run developer command Command, the name
	 * after "Veyra.Dev.", with Args for this player. VeyraDeveloper runs it on the server through
	 * VeyraDeveloperCommandRoute, and its reply arrives through GetLastDeveloperCommandReply and in the
	 * console. Shipping servers refuse every one. Veyra.Dev.Help lists them.
	 */
	void RequestDeveloperCommand(const FString& Command, const TArray<FString>& Args);

	/** Owning client: what the server said about the last developer command, and how many it answered. */
	const FString& GetLastDeveloperCommandReply() const { return LastDeveloperCommandReply; }
	int32 GetDeveloperCommandReplyCount() const { return DeveloperCommandReplyCount; }

	/** Owning client: the reason the server gave for the last refused rank-up, and how many it refused. */
	EVeyraRankRefusal GetLastRankUpRefusal() const { return LastRankUpRefusal; }
	int32 GetRankUpRefusalCount() const { return RankUpRefusalCount; }

	/**
	 * Owning client: asks the server to buy Item (Economy & Progression Bible §10–§11). At the fountain,
	 * or while dead, it arrives at once; elsewhere it waits for the fountain.
	 */
	void RequestBuyItem(const FVeyraContentId& Item);

	/** Owning client: asks to sell one of the item in inventory slot Slot, from 0, at the fountain (§12). */
	void RequestSellItem(int32 Slot);

	/** Owning client: asks to undo this fountain visit's latest purchase (§12). */
	void RequestUndoPurchase();

	/** Owning client: asks to cancel the pending purchase at Index, from 0, for all its Gold (§11.3). */
	void RequestCancelPurchase(int32 Index);

	/**
	 * Owning client: asks to put roster spell Spell in Flux Spell slot Slot, from 0, for Gold, at the
	 * fountain (ADR-015 §6).
	 */
	void RequestSwapFluxSpell(int32 Slot, const FVeyraContentId& Spell);

	/**
	 * Owning client: asks to put Tool in the vision-tool slot, at the fountain, for Economy.json's
	 * swap cost; the tool already there cannot be bought again (Vision Bible §3; ADR-016 §6).
	 */
	void RequestSwapVisionTool(EVeyraVisionTool Tool);

	/** Owning client: the reason the server gave for the last refused shop request, and how many it refused. */
	EVeyraShopRefusal GetLastShopRefusal() const { return LastShopRefusal; }
	int32 GetShopRefusalCount() const { return ShopRefusalCount; }

	/** Owning client: asks the server to buy the dead Vanguard back, from the shop (Economy & Progression Bible §15). */
	void RequestBuyback();

	/** Owning client: the reason the server gave for the last refused buyback, and how many it refused. */
	EVeyraBuybackRefusal GetLastBuybackRefusal() const { return LastBuybackRefusal; }
	int32 GetBuybackRefusalCount() const { return BuybackRefusalCount; }

	/** This player's Vanguard, on the server and on every client, or null before it spawns. */
	AVeyraVanguardCharacter* GetVanguard() const;

	/**
	 * The body this player's orders move now, on the server and its own client (ADR-050 §6): the Echo it commands, while
	 * it commands one, else its Vanguard. The camera follows it.
	 */
	APawn* GetCommandedBody() const;

	/** Whether this player commands an Echo now, on the server and its own client. */
	bool IsCommandingEcho() const { return CommandedUnit != nullptr; }

	/** Server: the unit this player commands in its Vanguard's stead, or null when it commands its Vanguard again. */
	void SetCommandedUnit(APawn* Unit);

	/** Owning client: the reason the server gave for the last refused order, and how many it refused. */
	EVeyraOrderRejection GetLastOrderRejection() const { return LastOrderRejection; }
	int32 GetOrderRejectionCount() const { return OrderRejectionCount; }

	/** Owning client: the reason the server gave for the last refused cast, and how many it refused. */
	EVeyraCastRejection GetLastCastRejection() const { return LastCastRejection; }
	int32 GetCastRejectionCount() const { return CastRejectionCount; }

	/**
	 * On the server this is the Vanguard's position, never the location a pawn-less client reports:
	 * replication decides what each player receives from it.
	 */
	virtual void GetPlayerViewPoint(FVector& OutLocation, FRotator& OutRotation) const override;

	/** What a click on the minimap is for (ADR-020 §2). */
	enum class EMinimapClick : uint8
	{
		/** A left click or drag: the camera looks there. */
		Camera,
		/** A right click: the Vanguard moves there. */
		Move,
		/** A ping: always, when the cursor is on the minimap. */
		Ping,
	};

	/**
	 * Owning client: pings Point for its side (ADR-020 §2). A refusal arrives through GetLastPingRefusal.
	 * With the ping key or the danger-ping key held, a click pings where the cursor points.
	 */
	void RequestPing(const FVector& Point, EVeyraPingKind Kind);

	/** Server: tells this player of a ping from its side. */
	void DeliverPing(const FVeyraPing& Ping) { ClientPinged(Ping); }

	/** Owning client: its side's pings it holds, oldest first, each for at most pings.keepSeconds. */
	const TArray<FVeyraReceivedPing>& GetPings() const { return Pings; }

	/** Owning client: the reason the server gave for the last refused ping. */
	EVeyraPingRefusal GetLastPingRefusal() const { return LastPingRefusal; }

	/** Owning client: sends Text on Channel (ADR-029 §1). A refusal arrives through GetLastChatRefusal. */
	void RequestChat(EVeyraChatChannel Channel, const FString& Text);

	/** Owning client: stops, or starts again, receiving PlayerId's chat for this match (ADR-029 §3). */
	void RequestMute(int32 PlayerId, bool bMuted);

	/** Owning client: whether it takes part in All Chat, as its setting says (ADR-029 §4). */
	void RequestAllChat(bool bOn);

	/** Server: tells this player of a chat message it may read. */
	void DeliverChat(const FVeyraChatMessage& Message) { ClientChatted(Message); }

	/** Owning client: the chat it holds, oldest first, at most chat.keepMessages. */
	const TArray<FVeyraReceivedChat>& GetChat() const { return Chat; }

	/** Owning client: whether it muted PlayerId. */
	bool IsChatMuted(int32 PlayerId) const { return ChatMuted.Contains(PlayerId); }

	/** Owning client: the reason the server gave for the last refused message. */
	EVeyraChatRefusal GetLastChatRefusal() const { return LastChatRefusal; }

	/** Owning client: adds a line of its own to its chat, such as a mute's confirmation; Subject names whom it is about. */
	void NoteChat(EVeyraChatNotice Notice, const FString& Subject);

	/**
	 * Owning client: the ground point a screen pixel on the minimap stands for, for a click of the given
	 * purpose, or nothing if the click is not on the minimap or the player turned that click off. The
	 * UI, which draws the minimap, sets it; the controller never calls the UI (ADR-006 §3).
	 */
	using FMinimapHitTest = TFunction<TOptional<FVector>(const FVector2D& /*Screen*/, EMinimapClick /*Purpose*/)>;
	void SetMinimapHitTest(FMinimapHitTest InHitTest) { MinimapHitTest = MoveTemp(InHitTest); }
	bool HasMinimapHitTest() const { return static_cast<bool>(MinimapHitTest); }

	/** Owning client: the indicator the player sees, while a cast waits or Show Cast Range previews one (ADR-041 §1). */
	const TOptional<FVeyraCastIndicator>& GetCastIndicator() const { return CastInput.GetIndicator(); }

	/** Owning client: the player's last move, Attack Move or attack order, which the presentation marks at once (ADR-062 §6). */
	const TOptional<FVeyraOrderMark>& GetOrderMark() const { return OrderMark; }

	/** Owning client: hides a waiting cast or a preview, and whether one showed. Escape asks this before the menu opens. */
	bool CancelPendingCast();

	/** Owning client: Attack Move's key was pressed and its click is awaited (ADR-041 §4). */
	bool IsAttackMoveWaiting() const { return bAttackMoveWaiting; }

	/** Owning client: the local camera, once the controller has made it (ADR-020 §1). */
	class AVeyraCameraRig* GetCameraRig() const { return CameraRig; }

	/** Owning client: Show Attack Range's key is held, so the player sees how far their basic attacks reach (ADR-052 §4). */
	bool IsShowingAttackRange() const;

	/** Owning client: a combat text number the server sent this player (ADR-052 §1), which the UI shows. */
	TMulticastDelegate<void(const FVeyraCombatTextLine&)> OnCombatText;

	/** Server: sends Line to this player's client. Unreliable: a lost number costs nothing that matters. */
	UFUNCTION(Client, Unreliable)
	void ClientCombatText(const FVeyraCombatTextLine& Line);

	/**
	 * Owning client: the units the player targets now, for When Targeted bars (ADR-052 §2): the unit its last
	 * attack order named, the one its body is attacking, and the one under the cursor. Presentation only.
	 */
	TArray<const AActor*> GetTargetedUnits() const;

	virtual void PlayerTick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void OnRep_PlayerState() override;

private:
	UFUNCTION(Server, Reliable)
	void ServerIssueMoveOrder(FVector Destination);

	UFUNCTION(Server, Unreliable)
	void ServerSteerMoveOrder(FVector Destination);

	/** Server: checks a move order from either path and hands it to the game mode. */
	void ApplyMoveOrder(const FVector& Destination);

	UFUNCTION(Server, Reliable)
	void ServerIssueAttackOrder(AActor* Target);

	UFUNCTION(Server, Reliable)
	void ServerIssueAttackMoveOrder(FVector Destination, EVeyraAttackMoveTarget Preference);

	UFUNCTION(Client, Unreliable)
	void ClientOrderRejected(EVeyraOrderRejection Rejection);

	UFUNCTION(Server, Reliable)
	void ServerRecall();

	UFUNCTION(Server, Reliable)
	void ServerMasteryEmote();

	UFUNCTION(Server, Reliable)
	void ServerIssueCastOrder(EVeyraAbilitySlot Slot, FVeyraCastTarget Target);

	UFUNCTION(Client, Unreliable)
	void ClientCastRejected(EVeyraCastRejection Rejection);

	UFUNCTION(Server, Reliable)
	void ServerRequestDeveloperPause(bool bPause);

	UFUNCTION(Server, Reliable)
	void ServerRequestDeveloperEndMatch();

	UFUNCTION(Server, Reliable)
	void ServerRequestEndCustomMatch();

	UFUNCTION(Client, Reliable)
	void ClientEndCustomMatchRefused(EVeyraEndCustomMatchRefusal Refusal);

	UFUNCTION(Client, Reliable)
	void ClientAbsenceWarning(bool bAfk);

	UFUNCTION(Server, Reliable)
	void ServerRequestVote(EVeyraVoteKind Kind);

	UFUNCTION(Server, Reliable)
	void ServerCastVote(bool bYes);

	UFUNCTION(Client, Reliable)
	void ClientVoteRefused(EVeyraVoteRefusal Refusal);

	UFUNCTION(Server, Reliable)
	void ServerPing(FVector Point, EVeyraPingKind Kind);

	UFUNCTION(Client, Reliable)
	void ClientPinged(const FVeyraPing& Ping);

	UFUNCTION(Client, Reliable)
	void ClientPingRefused(EVeyraPingRefusal Refusal);

	UFUNCTION(Server, Reliable)
	void ServerChat(EVeyraChatChannel Channel, const FString& Text);

	UFUNCTION(Client, Reliable)
	void ClientChatted(const FVeyraChatMessage& Message);

	UFUNCTION(Client, Reliable)
	void ClientChatRefused(EVeyraChatRefusal Refusal);

	UFUNCTION(Server, Reliable)
	void ServerMuteChat(int32 PlayerId, bool bMuted);

	UFUNCTION(Server, Reliable)
	void ServerAllChat(bool bOn);

	UFUNCTION(Server, Reliable)
	void ServerRankUp(EVeyraAbilitySlot Slot);

	UFUNCTION(Client, Unreliable)
	void ClientRankUpRefused(EVeyraRankRefusal Refusal);

	UFUNCTION(Server, Reliable)
	void ServerBuyItem(FVeyraContentId Item);

	UFUNCTION(Server, Reliable)
	void ServerSellItem(int32 Slot);

	UFUNCTION(Server, Reliable)
	void ServerUndoPurchase();

	UFUNCTION(Server, Reliable)
	void ServerCancelPurchase(int32 Index);

	UFUNCTION(Server, Reliable)
	void ServerSwapFluxSpell(int32 Slot, FVeyraContentId Spell);

	UFUNCTION(Server, Reliable)
	void ServerSwapVisionTool(EVeyraVisionTool Tool);

	UFUNCTION(Client, Unreliable)
	void ClientShopRefused(EVeyraShopRefusal Refusal);

	UFUNCTION(Server, Reliable)
	void ServerBuyback();

	UFUNCTION(Client, Unreliable)
	void ClientBuybackRefused(EVeyraBuybackRefusal Refusal);

	/** Server: runs a shop request if the order allowance and the match allow it, and tells the client why it was refused. */
	void RunShopRequest(TFunctionRef<EVeyraShopRefusal(class UVeyraShopSubsystem& Shop, APlayerState& Participant)> Request);

	/** Server: RunShopRequest for an order that has taken its allowance already. */
	void ApplyShopRequest(TFunctionRef<EVeyraShopRefusal(class UVeyraShopSubsystem& Shop, APlayerState& Participant)> Request);

	UFUNCTION(Server, Reliable)
	void ServerRequestDeveloperCommand(const FString& Command, const TArray<FString>& Args);

	UFUNCTION(Client, Reliable)
	void ClientDeveloperCommandReply(const FString& Reply);

	UFUNCTION()
	void OnVanguardSet(APlayerState* Participant, APawn* NewPawn, APawn* OldPawn);

	/** Owning client: this frame's camera input from the keys, the screen's edges and the drag. */
	void TickCamera(float DeltaTime);

	/** Owning client: the unit the latest attack order named, until a move order replaces it. */
	TWeakObjectPtr<AActor> OrderedAttackTarget;

	/** Owning client: records the order just given for its mark. */
	void MarkOrder(EVeyraOrderMarkKind Kind, const FVector& Location, const AActor* Target);

	TOptional<FVeyraOrderMark> OrderMark;

	/** Owning client: the zoom keys' presses this frame move the player's zoom level, which persists (ADR-052 §3). */
	void TickZoom();

	/** Owning client: how many times Key went down since input was last processed; a wheel turns several notches a frame. */
	int32 PressesOf(const FKey& Key) const;

	/** Owning client: lets go of old pings, and pings where the player clicks with a ping key held. */
	void TickPings();

	/** Whether the player holds a ping key, so a click pings instead of steering the camera. */
	bool IsPinging() const;

	/** The end-of-match pan: from where the camera looked, to the fallen Prime Well once this client has it. */
	struct FEndPan
	{
		FVector From = FVector::ZeroVector;
		TOptional<FVector> To;
		double StartedAt = 0.0;
	};
	TOptional<FEndPan> EndPan;

	/** Owning client: once the match has ended, the camera pans to the fallen Prime Well; true while it does. */
	bool TickEndPan(double DeltaSeconds);

	UPROPERTY(Transient)
	TObjectPtr<class AVeyraCameraRig> CameraRig;

	/** Where the cursor was on the last frame of a middle-mouse drag. */
	TOptional<FVector2D> LastDragMouse;

	/** How long the cursor has rested in the screen's edge zone, for the Edge-Scroll Delay (SET-87). */
	double EdgeHeldSeconds = 0.0;

	/** The Vanguard has had a body: the next one is a respawn, which Return Camera on Respawn governs (SET-156). */
	bool bHadVanguard = false;

	/** The player's camera settings over the developer's (ADR-024 §6). */
	FVeyraCameraPreferences CameraPreferences() const;

	/** The player's control settings (ADR-041 §5). */
	struct FVeyraControlPreferences ControlPreferences() const;

	/** Makes PlayerKeys the developer's keys with the player's bindings, and maps the actions to them anew. */
	void RefreshKeys();
	void OnPlayerSettingChanged(const FVeyraContentId& Id);

	/** The player's keys: the developer's, with the player's bindings in place (ADR-024 §6). */
	UPROPERTY(Transient)
	TObjectPtr<UVeyraInputSettings> PlayerKeys;

	FDelegateHandle SettingsHandle;

	FMinimapHitTest MinimapHitTest;

	/** The minimap's ground point under the cursor for Purpose, if the cursor is on it. */
	TOptional<FVector> MinimapPointUnderCursor(EMinimapClick Purpose) const;

	void RejectOrder(EVeyraOrderRejection Rejection);

	// Local input (Settings Bible §1): right-click to move or attack, attack-move, and each ability
	// slot cast in the player's casting mode.
	void OnMoveOrderStarted();
	void OnMoveOrderHeld();
	void OnAttackMovePressed();
	void OnRecallPressed();
	void OnVoteYesPressed();
	void OnVoteNoPressed();
	void OnMasteryEmotePressed();
	void OnAbilityPressed(EVeyraAbilitySlot Slot);
	void OnAbilityReleased(EVeyraAbilitySlot Slot);
	void MoveToCursor(bool bSteer);

	/** Owning client: the select click casts a waiting cast; letting go of Show Cast Range hides its preview. */
	void TickCastInput();

	/** Attack-moves toward the ground, or the minimap's point, under the cursor. */
	void AttackMoveToCursor();

	/** Attack Move's key was pressed: its click, the Select Click, gives the order. */
	bool bAttackMoveWaiting = false;

	/** Does what the cast input decided: a cast goes toward the cursor; an indicator is the UI's to draw. */
	void ApplyCastStep(const FVeyraCastOutcome& Outcome);

	/** Casts Slot now, at the unit and the ground under the cursor. */
	void CastAtCursor(EVeyraAbilitySlot Slot);

	/** Owning client: the ability Slot holds now, an override included; invalid for none. */
	FVeyraContentId AbilityIn(EVeyraAbilitySlot Slot) const;

	/** Owning client: what Slot holds now, for a waiting cast to check it can still be cast. */
	FVeyraSlotNow SlotNow(EVeyraAbilitySlot Slot) const;

	/** Which indicator shows, and when a key, its release or a click casts (ADR-041 §1). */
	FVeyraCastInput CastInput;

	/** Owning client: the enemy unit under the cursor, if any; only a Vanguard while Target Vanguards Only holds. */
	AActor* FindEnemyUnderCursor() const;

	/** Owning client: the units under the cursor, nearest the camera first, until something else blocks the view. */
	TArray<FVeyraCursorUnit> UnitsUnderCursor() const;

	/** Owning client: whether attacks and casts name only Vanguards now (Settings Bible §1.4). */
	bool IsTargetingVanguardsOnly() const;

	/**
	 * Owning client: whether Slot's cast names the player's own Vanguard: its ability may name an ally, and the
	 * Self-Cast Modifier is held or Smart Self-Cast finds no allied Vanguard under the cursor (Settings Bible §1.5).
	 */
	bool ShouldSelfCast(EVeyraAbilitySlot Slot, TConstArrayView<FVeyraCursorUnit> Under) const;

	/** Target Vanguards Only, switched by its key while its mode is Toggle. */
	bool bTargetVanguardsToggled = false;

	/** Whether the move button's current press ordered an attack, which holding it does not steer. */
	bool bMoveOrderPressAttacked = false;

	UPROPERTY(Transient)
	FVeyraInputObjects Input;

public:
	/** The keys this player plays with: their bindings over the developer's; the developer's before any are read. */
	const UVeyraInputSettings& GetKeys() const;

private:

	double LastHeldMoveOrderTime = 0.0;

	/** Server: spends one order from the player's allowance, refilled at the tuned rate. */
	bool TakeOrderAllowance();

	double OrderAllowance = 0.0;
	double OrderAllowanceTime = 0.0;
	bool bOrderAllowanceStarted = false;

	EVeyraOrderRejection LastOrderRejection = EVeyraOrderRejection::None;
	int32 OrderRejectionCount = 0;

	EVeyraCastRejection LastCastRejection = EVeyraCastRejection::None;
	int32 CastRejectionCount = 0;

	EVeyraRankRefusal LastRankUpRefusal = EVeyraRankRefusal::None;
	int32 RankUpRefusalCount = 0;

	EVeyraEndCustomMatchRefusal LastEndCustomMatchRefusal = EVeyraEndCustomMatchRefusal::None;
	int32 EndCustomMatchRefusalCount = 0;

	EVeyraVoteRefusal LastVoteRefusal = EVeyraVoteRefusal::None;
	int32 VoteRefusalCount = 0;

	TArray<FVeyraReceivedPing> Pings;
	TArray<FVeyraReceivedChat> Chat;
	/** Adds Line to the chat it holds, stamped now, keeping only the newest. */
	void KeepChat(FVeyraReceivedChat&& Line);
	/** Tells the server whether the player keeps All Chat on, as their setting says (ADR-029 §4). */
	void ReportAllChat();
	TSet<int32> ChatMuted;
	EVeyraChatRefusal LastChatRefusal = EVeyraChatRefusal::None;
	EVeyraPingRefusal LastPingRefusal = EVeyraPingRefusal::None;

	/** Its team's open vote; a controller replicates to its own player only. */
	UPROPERTY(Replicated)
	FVeyraVoteState TeamVote;

	/** The Echo its orders move in its Vanguard's stead, if any (ADR-050 §6). */
	UPROPERTY(Replicated)
	TObjectPtr<APawn> CommandedUnit;

	bool bWarnedAfk = false;

	EVeyraShopRefusal LastShopRefusal = EVeyraShopRefusal::None;
	int32 ShopRefusalCount = 0;

	EVeyraBuybackRefusal LastBuybackRefusal = EVeyraBuybackRefusal::None;
	int32 BuybackRefusalCount = 0;

	FString LastDeveloperCommandReply;
	int32 DeveloperCommandReplyCount = 0;
};
