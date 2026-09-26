// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "VeyraSmokeClientSubsystem.generated.h"

class AVeyraGameState;
class AVeyraPlayerController;

/**
 * A scripted client for the multi-process smoke test (Game/Scripts/Smoke.ps1). It exists only when
 * the client starts with -VeyraSmoke. Once the match is live it orders its Vanguard toward the lane
 * centre and waits for it to move. With -VeyraSmokePause it then pauses and resumes the match and
 * checks that its own world stops and starts (ADR-006 §8). It logs "VeyraSmoke: PASS" or
 * "VeyraSmoke: FAIL" and quits with exit code 0 or 1.
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
		WaitForPause,
		WaitForResume,
		Finished,
	};

	bool Tick(float DeltaSeconds);
	void Advance(EStep NextStep, const TCHAR* Description);
	void Finish(bool bPassed, const FString& Reason);

	AVeyraPlayerController* GetController() const;
	AVeyraGameState* GetGameState() const;

	EStep Step = EStep::WaitForLiveMatch;
	bool bCheckPause = false;
	double StartRealTime = 0.0;
	FVector MoveStart = FVector::ZeroVector;
	FVector MoveDestination = FVector::ZeroVector;
	FTSTicker::FDelegateHandle TickHandle;
};
