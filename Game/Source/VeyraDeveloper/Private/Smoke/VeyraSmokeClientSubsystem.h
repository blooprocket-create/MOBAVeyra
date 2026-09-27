// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Ticker.h"
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
		Finished,
	};

	bool Tick(float DeltaSeconds);
	void Advance(EStep NextStep, const TCHAR* Description);
	void Finish(bool bPassed, const FString& Reason);

	AVeyraPlayerController* GetController() const;
	AVeyraGameState* GetGameState() const;
	const AVeyraPlayerState* FindEnemy(const AVeyraPlayerController& Controller, const AVeyraGameState& GameState) const;

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
