// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Ticker.h"
#include "Content/VeyraContentId.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "VeyraSmokeClientSubsystem.generated.h"

class AVeyraGameState;
class AVeyraPlayerController;
class AVeyraPlayerState;
class AVeyraVanguardCharacter;
class UNetDriver;

/**
 * A scripted client for the multi-process smoke test (Game/Scripts/Smoke.ps1). It exists only when
 * the client starts with -VeyraSmoke, and acts only once it is connected to a server: a client that
 * joins through the session handoff starts in a local world. Once the match is live it orders its
 * Vanguard toward the lane centre, waits for it to move, and casts its Q ability at the enemy
 * Vanguard once it is in range; the server must land it. With -VeyraSmokePause it then pauses the
 * match, checks that its own world stops and stays stopped for a moment, then resumes it and checks
 * that the world starts again (ADR-006 §8). With -VeyraSmokeEndMatch it then waits until the
 * enemy's cast has hit its own Vanguard, asks the server to end the match (ADR-007 §8) and waits for
 * the end; with -VeyraSmokeWaitForEnd it only waits for the end. It logs "VeyraSmoke: PASS" or
 * "VeyraSmoke: FAIL", which is its result, and quits; with -VeyraSmokeStay=<seconds> a passing client
 * stays connected that long first. A failed connection or travel fails it at once.
 *
 * With -VeyraSmokeKit it plays any Vanguard's whole kit instead of one Q:
 * - it takes developer levels up to the first ultimate rank and ranks every ability (ADR-008 §6);
 * - it moves toward the lane centre, then casts Q, W, E and R in turn at the nearest enemy Vanguard,
 *   each counting once the server starts its cooldown and retried when the server refuses it;
 * - it orders a basic attack on the nearest enemy and waits for one to commit;
 * - it passes once an enemy Vanguard has taken damage.
 *
 * With -VeyraSmokeEndCustomMatch it plays a practice match's host (ADR-010 §7): once the match is
 * live it moves, asks to end the custom match, and passes when the match ends.
 */
UCLASS()
class UVeyraSmokeClientSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	enum class EStep : uint8
	{
		WaitForLiveMatch,
		WaitForMove,
		WaitForRange,
		WaitForHit,
		WaitForPause,
		HoldPause,
		WaitForResume,
		WaitToBeHit,
		WaitForEnd,
		// -VeyraSmokeKit
		WaitForLevels,
		WaitForRanks,
		KitCast,
		KitAttack,
		KitDamage,
		Finished,
	};

	bool Tick(float DeltaSeconds);
	void Advance(EStep NextStep, const TCHAR* Description);
	void Finish(bool bPassed, const FString& Reason);

	AVeyraPlayerController* GetController() const;
	AVeyraGameState* GetGameState() const;

	/** The living enemy Vanguard nearest From; null if none has a body. */
	AActor* FindNearestEnemyBody(const AVeyraPlayerController& Controller, const AVeyraGameState& GameState, const FVector& From) const;

	/** Orders the Vanguard toward the map's centre, stopping a fraction of StopDistance short of it. */
	void StartMove(AVeyraPlayerController& Controller, const AVeyraVanguardCharacter& Vanguard, double StopDistance);

	/** -VeyraSmokeKit: one step of casting the kit, in slot order. */
	void TickKitCast(AVeyraPlayerController& Controller, const AVeyraGameState& GameState, const AVeyraVanguardCharacter& Vanguard);

	/** -VeyraSmokeKit: whether any enemy Vanguard has lost Health. */
	bool HasAnEnemyTakenDamage(const AVeyraPlayerController& Controller, const AVeyraGameState& GameState) const;

	void RequestScreenshot();

	/** After this client's cast hits: pauses, or goes on to the end of the match, or passes. */
	void AfterHit();

	/** After the cast and any pause: waits to end the match, or for its end, or passes. */
	void AfterScript(const TCHAR* Summary);

	void OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void OnTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	EStep Step = EStep::WaitForLiveMatch;
	bool bCheckPause = false;
	bool bEndMatch = false;
	bool bWaitForEnd = false;
	/** What the script did before the end of the match, for the verdict. */
	FString ScriptSummary;
	/** Where to save a screenshot once the cast has landed (-VeyraSmokeScreenshot=); empty for none. */
	FString ScreenshotPath;
	bool bScreenshotRequested = false;
	bool bKit = false;
	/** -VeyraSmokeEndCustomMatch: a practice match's host moves, then ends the match. */
	bool bEndCustomMatch = false;
	/** -VeyraSmokeKit: the level that opens the first ultimate rank. */
	int32 KitLevel = 0;
	/** -VeyraSmokeKit: the slot being cast, from 0 (Q) to 4 (done), and whether its cast awaits the server. */
	int32 KitSlotIndex = 0;
	bool bKitCastPending = false;
	int32 KitRejectionsBefore = 0;
	/** The order refusals before the pending cast, and when the next try may go after a refusal. */
	int32 KitOrderRejectionsBefore = 0;
	double KitNextTryAt = 0.0;
	/** Real time when the kit's attack order is given again, until an attack commits. */
	double KitAttackRetryAt = 0.0;
	/** Real time when the move back toward the fight is ordered again, while no enemy is in sight. */
	double KitWalkBackAt = 0.0;

	/** Walks back toward the fight, as after a death, now and then while no enemy is in sight. */
	void WalkBackToTheFight(AVeyraPlayerController& Controller);
	/** When the pending cast's order went, to try again should neither a Commit nor a refusal ever come. */
	double KitPendingSince = 0.0;
	/** When the pending cast's cooldown was due before its order: a Commit starts a new one, due later. */
	double KitReadyAtBefore = 0.0;
	int32 KitCastAttempts = 0;
	/** The ability the pending cast asked for: once it commits, its slot may hold a follow-up instead. */
	FVeyraContentId KitCastAbility;
	/** -VeyraSmokeKit: when the slot being cast was first refused, in real seconds. */
	TOptional<double> KitFirstRefusedAt;
	/** -VeyraSmokeKit: whether this slot's cast has asked for the developer heal, which fills a resource (ADR-033 §1). */
	bool bKitAskedForResource = false;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
	double StartRealTime = 0.0;
	FVector MoveStart = FVector::ZeroVector;
	FVector MoveDestination = FVector::ZeroVector;
	double CastRange = 0.0;
	double PausedRealTime = 0.0;
	double StaySeconds = 0.0;
	TWeakObjectPtr<const AVeyraPlayerState> Enemy;
	FTSTicker::FDelegateHandle TickHandle;
};
