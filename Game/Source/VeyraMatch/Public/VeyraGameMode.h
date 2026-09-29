// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Ticker.h"
#include "GameFramework/GameModeBase.h"
#include "Join/VeyraMatchRoster.h"
#include "Teams/VeyraTeam.h"
#include "VeyraAbilityTypes.h"
#include "VeyraMatchTypes.h"

#include "VeyraGameMode.generated.h"

class AVeyraGameState;
class AVeyraPlayerController;
class FVeyraBattlegroundLink;
class AVeyraPlayerState;
class AVeyraVanguardCharacter;
class UAbilitySystemComponent;
struct FVeyraDeathEvent;

/**
 * Runs one match on the server (Match Flow Bible §1): admits participants, assigns sides, advances
 * the phases, spawns each Vanguard at its side's fountain, decides whether an order is allowed now
 * and ends the match. It never lets the engine spawn or respawn a pawn for a player.
 *
 * A server hosting an assigned match (ADR-007) admits only its roster, each by join ticket and on
 * its rostered side. A developer server without an assignment admits direct connections and puts
 * each player on the smaller side.
 */
UCLASS()
class VEYRAMATCH_API AVeyraGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AVeyraGameMode(const FObjectInitializer& ObjectInitializer);

	/** The login option and console variable that say how many humans the match waits for. */
	static constexpr const TCHAR* ExpectedPlayersOption = TEXT("VeyraExpectedPlayers");

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Logout(AController* Exiting) override;

	/**
	 * Ends the match: it takes no more orders or players, its clock stops, and its result goes to
	 * whoever hosts it (UVeyraMatchHostSubsystem::OnMatchEnded). Winner is the side that destroyed the
	 * other's Prime Well, and None for every other reason; a call that mismatches them is refused.
	 * Ending an ended match does nothing.
	 */
	void EndMatch(EVeyraMatchEndReason Reason, EVeyraTeam Winner = EVeyraTeam::None);

	/**
	 * Developer builds: destroys the next structure of Requester's enemies in siege order with a
	 * lethal developer hit from Requester, through the damage pipeline, so invulnerability, Team Flux
	 * and victory run for real (ADR-011 §15). Returns whether one fell.
	 */
	bool HandleDeveloperSiege(const APlayerController& Requester);

	/**
	 * A player asks to end the custom match as its host (Custom Matches Bible §4; ADR-010 §7). Only
	 * the host of a practice match may; the match then ends host-ended, with no winner. Returns why it
	 * was refused, or None.
	 */
	EVeyraEndCustomMatchRefusal HandleEndCustomMatch(const APlayerController& Requester);

	/** Why the match refuses orders right now, or None: orders need the live phase and no pause. */
	EVeyraOrderRejection CheckOrdersAllowed() const;

	/** Validates a player's move order and hands it to their Vanguard's controller. */
	EVeyraOrderRejection HandleMoveOrder(AVeyraPlayerController& Player, const FVector& Destination);

	/** Checks the match allows orders, then hands a player's attack order on Target to their Vanguard's controller. */
	EVeyraOrderRejection HandleAttackOrder(AVeyraPlayerController& Player, AActor* Target);

	/** Validates a player's attack-move order and hands it to their Vanguard's controller. */
	EVeyraOrderRejection HandleAttackMoveOrder(AVeyraPlayerController& Player, const FVector& Destination);

	/** Checks the match allows casting, then casts the player's ability in Slot through VeyraAbilities. */
	EVeyraCastRejection HandleCastOrder(AVeyraPlayerController& Player, EVeyraAbilitySlot Slot, const FVeyraCastTarget& Target);

	/**
	 * Begins the player's Recall (Economy & Progression Bible §10; ADR-012 §8): the Vanguard stops and
	 * channels, and goes home to its fountain if nothing interrupts it. A channel already running
	 * carries on. Refused while orders are, while dead, and under crowd control that stops casting.
	 */
	EVeyraOrderRejection HandleRecallOrder(AVeyraPlayerController& Player);

	/** Why the match refuses rank-ups now, or None: they need preparation or the live phase, and no pause. */
	EVeyraOrderRejection CheckRankUpAllowed() const;

	/**
	 * Why the match refuses shopping now, or None: the same phases as rank-ups, and a pause freezes it
	 * (Match Flow Bible §10.2; ADR-012 §7).
	 */
	EVeyraOrderRejection CheckShopAllowed() const { return CheckRankUpAllowed(); }

	/**
	 * Adds an AI-controlled participant with its own PlayerState, as Co-op and custom matches do
	 * (ADR-006 §4). Side and Vanguard seat it, as an assigned match's bots are; without them it joins
	 * the smaller side and plays the developer order's Vanguard. It gets its Vanguard like any player,
	 * now if the match is past loading. Its controller moves only when told. Returns the new
	 * participant, or null if its side, or with no side both, are full.
	 */
	AVeyraPlayerState* AddBotParticipant(const FString& Name, EVeyraTeam Side = EVeyraTeam::None, const FVeyraContentId& Vanguard = FVeyraContentId());

	/**
	 * Pauses every gameplay clock (Match Flow Bible §10.2). Pause votes arrive later; until then the
	 * server pauses directly.
	 */
	bool PauseMatch(APlayerController& Requester);
	bool ResumeMatch();

protected:
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options,
		const FString& Portal = TEXT("")) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;
	virtual bool CanSpectate_Implementation(APlayerController* Viewer, APlayerState* ViewTarget) override;

private:
	AVeyraGameState& GetVeyraGameState() const;
	int32 CountTeamMembers(EVeyraTeam Team) const;
	/** Whether both sides already have as many participants as the tuning allows. */
	bool IsFull() const;
	void AssignTeam(AVeyraPlayerState& PlayerState) const;

	/**
	 * Chooses the participant's Vanguard: a rostered participant plays the one the assignment names
	 * (ADR-010 §9); anyone else its development request, or the next in the developer order (ADR-008 §8).
	 */
	void AssignVanguard(AVeyraPlayerState& PlayerState);

	AActor* FindTeamStart(EVeyraTeam Team) const;

	/** The map has a start for each side standing on built navigation. */
	bool IsMapReady() const;
	bool HaveExpectedPlayersJoined();

	void OnLoadingTimedOut();

	/** Adds the assignment's bots on their sides, each walking with a UVeyraBotWanderComponent (ADR-010 §7). */
	void AddAssignedBots();

	void BeginPreparation();
	void BeginLive();
	void SpawnVanguard(AVeyraPlayerState& PlayerState);

	/** Prepares a participant as its Vanguard, once per match (VeyraVanguards::PrepareCombatant). */
	bool InitializeCombatant(AVeyraPlayerState& PlayerState, UAbilitySystemComponent& AbilitySystem);

	/** A Vanguard died: its body leaves the map, and it respawns after the tuned delay (Combat Bible §18). */
	void OnDeath(const FVeyraDeathEvent& Death);
	void Respawn(TWeakObjectPtr<AVeyraPlayerState> PlayerState);

	/** The battleground reports a Prime Well destroyed: Winner destroyed the other side's (ADR-011 §13). */
	void OnPrimeWellDestroyed(EVeyraTeam Winner);

	/** A Recall channel completed: the living Vanguard arrives at its side's fountain. */
	void CompleteRecall(TWeakObjectPtr<AVeyraPlayerState> PlayerState);

	/** Restores each living Vanguard standing at its own fountain (Battleground Bible §12; ADR-011 §11). */
	void RecoverAtFountains();

	/** Starts or stops the abandonment clock as rostered participants come and go. */
	void NoteConnectedParticipants();
	/** Ends an assigned match that nobody has been connected to for the tuned time (ADR-007 §8). */
	bool TickAbandonment(float DeltaSeconds);

	FDelegateHandle DeathHandle;

	/** Connects the battleground's World and Flux while the match runs (ADR-011 §3). */
	TSharedPtr<FVeyraBattlegroundLink> Battleground;

	/** Set when this server hosts an assigned match. */
	TUniquePtr<FVeyraMatchRoster> Roster;

	/**
	 * Real time since which no rostered participant has been connected. The clock counts through
	 * pauses: it is a server lifecycle clock, not a gameplay timer (ADR-006 §8).
	 */
	TOptional<double> NobodyConnectedSince;
	FTSTicker::FDelegateHandle AbandonmentTicker;

	UPROPERTY(EditDefaultsOnly, Category = "Classes")
	TSubclassOf<AVeyraVanguardCharacter> VanguardClass;

	int32 ExpectedPlayers = 0;

	/** How many participants have been given a Vanguard, for the developer join order. */
	int32 VanguardsAssigned = 0;

	bool bLoadingTimedOut = false;
	FTimerHandle LoadingTimeout;
	FTimerHandle PreparationTimer;
	FTimerHandle FountainTimer;
};
