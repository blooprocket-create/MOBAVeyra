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
	 * whoever hosts it (UVeyraMatchHostSubsystem::OnMatchEnded). Ending an ended match does nothing.
	 */
	void EndMatch(EVeyraMatchEndReason Reason);

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

	/** Why the match refuses rank-ups now, or None: they need preparation or the live phase, and no pause. */
	EVeyraOrderRejection CheckRankUpAllowed() const;

	/**
	 * Adds an AI-controlled participant with its own PlayerState, on the smaller side, as Co-op and
	 * custom matches will (ADR-006 §4). It gets a Vanguard like any player, now if the match is past
	 * loading. It has no behaviour yet: its controller moves only when told. Returns the new
	 * participant, or null if both sides are full.
	 */
	AVeyraPlayerState* AddBotParticipant(const FString& Name);

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
	void BeginPreparation();
	void BeginLive();
	void SpawnVanguard(AVeyraPlayerState& PlayerState);

	/** Prepares a participant as its Vanguard, once per match (VeyraVanguards::PrepareCombatant). */
	bool InitializeCombatant(AVeyraPlayerState& PlayerState, UAbilitySystemComponent& AbilitySystem);

	/** A Vanguard died: its body leaves the map, and it respawns after the tuned delay (Combat Bible §18). */
	void OnDeath(const FVeyraDeathEvent& Death);
	void Respawn(TWeakObjectPtr<AVeyraPlayerState> PlayerState);

	/** Starts or stops the abandonment clock as rostered participants come and go. */
	void NoteConnectedParticipants();
	/** Ends an assigned match that nobody has been connected to for the tuned time (ADR-007 §8). */
	bool TickAbandonment(float DeltaSeconds);

	FDelegateHandle DeathHandle;

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
};
