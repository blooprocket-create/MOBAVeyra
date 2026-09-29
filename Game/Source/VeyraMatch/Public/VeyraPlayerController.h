// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameFramework/PlayerController.h"
#include "Content/VeyraContentId.h"
#include "Input/VeyraInputSettings.h"
#include "Inventory/VeyraInventoryRules.h"
#include "Progression/VeyraProgressionTypes.h"
#include "Templates/Function.h"
#include "Tools/VeyraVisionToolComponent.h"
#include "VeyraAbilityTypes.h"
#include "VeyraMatchTypes.h"
#include "Votes/VeyraVoteTypes.h"

#include "VeyraPlayerController.generated.h"

class AVeyraVanguardCharacter;

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

	/** Owning client: asks the server to attack-move this player's Vanguard to Destination. */
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
	 * Owning client, developer builds: asks the server for Amount XP, or for enough XP to gain Levels
	 * levels, until minions give XP (ADR-008 §6). Shipping servers refuse them. The console commands
	 * Veyra.Dev.GrantXp and Veyra.Dev.GrantLevels send them.
	 */
	void RequestDeveloperExperience(int32 Amount);
	void RequestDeveloperLevels(int32 Levels);

	/**
	 * Owning client, developer builds: asks the server to destroy the next enemy structure in siege
	 * order, as Veyra.Dev.Siege does (ADR-011 §15), so a match can be won in minutes. Shipping servers
	 * refuse it.
	 */
	void RequestDeveloperSiege();

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

	/** This player's Vanguard, on the server and on every client, or null before it spawns. */
	AVeyraVanguardCharacter* GetVanguard() const;

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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
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
	void ServerIssueAttackMoveOrder(FVector Destination);

	UFUNCTION(Client, Unreliable)
	void ClientOrderRejected(EVeyraOrderRejection Rejection);

	UFUNCTION(Server, Reliable)
	void ServerRecall();

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

	/** Server: runs a shop request if the order allowance and the match allow it, and tells the client why it was refused. */
	void RunShopRequest(TFunctionRef<EVeyraShopRefusal(class UVeyraShopSubsystem& Shop, APlayerState& Participant)> Request);

	/** Server: RunShopRequest for an order that has taken its allowance already. */
	void ApplyShopRequest(TFunctionRef<EVeyraShopRefusal(class UVeyraShopSubsystem& Shop, APlayerState& Participant)> Request);

	UFUNCTION(Server, Reliable)
	void ServerRequestDeveloperExperience(int32 Amount);

	UFUNCTION(Server, Reliable)
	void ServerRequestDeveloperLevels(int32 Levels);

	UFUNCTION(Server, Reliable)
	void ServerRequestDeveloperSiege();

	/** Server, developer builds: the participant's progression, if its XP may be granted. */
	class UVeyraProgressionComponent* FindDeveloperProgression() const;

	UFUNCTION()
	void OnVanguardSet(APlayerState* Participant, APawn* NewPawn, APawn* OldPawn);

	void RejectOrder(EVeyraOrderRejection Rejection);

	// Local input (Settings Bible §1): right-click to move or attack, attack-move, and Quick Cast on
	// each ability slot.
	void OnMoveOrderStarted();
	void OnMoveOrderHeld();
	void OnAttackMovePressed();
	void OnRecallPressed();
	void OnVoteYesPressed();
	void OnVoteNoPressed();
	void OnAbilityPressed(EVeyraAbilitySlot Slot);
	void MoveToCursor(bool bSteer);

	/** Owning client: the enemy unit under the cursor, if any. */
	AActor* FindEnemyUnderCursor() const;

	/** Whether the move button's current press ordered an attack, which holding it does not steer. */
	bool bMoveOrderPressAttacked = false;

	UPROPERTY(Transient)
	FVeyraInputObjects Input;

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

	/** Its team's open vote; a controller replicates to its own player only. */
	UPROPERTY(Replicated)
	FVeyraVoteState TeamVote;

	bool bWarnedAfk = false;

	EVeyraShopRefusal LastShopRefusal = EVeyraShopRefusal::None;
	int32 ShopRefusalCount = 0;
};
