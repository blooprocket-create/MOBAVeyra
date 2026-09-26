// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "VeyraSmokeClientSubsystem.generated.h"

class AVeyraGameState;
class AVeyraPlayerController;
class AVeyraPlayerState;
class AVeyraVanguardCharacter;

/**
 * A scripted client for the multi-process smoke test (Game/Scripts/Smoke.ps1). It exists only when
 * the client starts with -VeyraSmoke. Once the match is live it orders its Vanguard toward the lane
 * centre, waits for it to move, and casts its Q ability at the enemy Vanguard once it is in range;
 * the server must land it. With -VeyraSmokePause it then pauses and resumes the match and checks
 * that its own world stops and starts (ADR-006 §8). It logs "VeyraSmoke: PASS" or
 * "VeyraSmoke: FAIL", which is its result, and quits.
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
		WaitForResume,
		Finished,
	};

	bool Tick(float DeltaSeconds);
	void Advance(EStep NextStep, const TCHAR* Description);
	void Finish(bool bPassed, const FString& Reason);

	AVeyraPlayerController* GetController() const;
	AVeyraGameState* GetGameState() const;
	const AVeyraPlayerState* FindEnemy(const AVeyraPlayerController& Controller, const AVeyraGameState& GameState) const;

	/** After the move: casts at the enemy when in range, or pauses, or passes. */
	void AfterHit();

	EStep Step = EStep::WaitForLiveMatch;
	bool bCheckPause = false;
	double StartRealTime = 0.0;
	FVector MoveStart = FVector::ZeroVector;
	FVector MoveDestination = FVector::ZeroVector;
	double CastRange = 0.0;
	TWeakObjectPtr<const AVeyraPlayerState> Enemy;
	FTSTicker::FDelegateHandle TickHandle;
};
