// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameFramework/GameModeBase.h"
#include "Teams/VeyraTeam.h"
#include "VeyraMatchTypes.h"

#include "VeyraGameMode.generated.h"

class AVeyraGameState;
class AVeyraPlayerController;
class AVeyraPlayerState;
class AVeyraVanguardCharacter;

/**
 * Runs one match on the server (Match Flow Bible §1): admits participants, assigns sides, advances
 * the phases, spawns each Vanguard at its side's fountain and decides whether an order is allowed
 * now. It never lets the engine spawn or respawn a pawn for a player.
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
	virtual void Tick(float DeltaSeconds) override;

	/** Why the match refuses orders right now, or None: orders need the live phase and no pause. */
	EVeyraOrderRejection CheckOrdersAllowed() const;

	/** Validates a player's move order and hands it to their Vanguard's controller. */
	EVeyraOrderRejection HandleMoveOrder(AVeyraPlayerController& Player, const FVector& Destination);

	/**
	 * Pauses every gameplay clock (Match Flow Bible §10.2). Pause votes arrive later; until then the
	 * server pauses directly.
	 */
	bool PauseMatch(APlayerController& Requester);
	bool ResumeMatch();

protected:
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;
	virtual bool CanSpectate_Implementation(APlayerController* Viewer, APlayerState* ViewTarget) override;

private:
	AVeyraGameState& GetVeyraGameState() const;
	int32 CountTeamMembers(EVeyraTeam Team) const;
	void AssignTeam(AVeyraPlayerState& PlayerState) const;
	AActor* FindTeamStart(EVeyraTeam Team) const;

	/** The map has a start for each side standing on built navigation. */
	bool IsMapReady() const;
	bool HaveExpectedPlayersJoined();

	void OnLoadingTimedOut();
	void BeginPreparation();
	void BeginLive();
	void SpawnVanguard(AVeyraPlayerState& PlayerState);

	UPROPERTY(EditDefaultsOnly, Category = "Classes")
	TSubclassOf<AVeyraVanguardCharacter> VanguardClass;

	int32 ExpectedPlayers = 0;
	bool bLoadingTimedOut = false;
	FTimerHandle LoadingTimeout;
	FTimerHandle PreparationTimer;
};
