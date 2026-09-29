// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Backend/VeyraBackendClient.h"
#include "Containers/Ticker.h"
#include "Subsystems/EngineSubsystem.h"
#include "Templates/UniquePtr.h"

#include "VeyraMatchServerSubsystem.generated.h"

class UVeyraMatchHostSubsystem;
struct FVeyraMatchResult;

/**
 * A match server's side of the handoff (ADR-007 §5–7), on a dedicated server started with
 * -VeyraAssignment=stdin, and on every Shipping dedicated server, which cannot run without one.
 * Before the first map loads, it reads the assignment from standard input and hands the match to
 * VeyraMatch. It tells the backend when the match accepts players and how it ended, then quits.
 * Any failure logs "VeyraHandoff: FAIL: <reason>" and quits with a failure status, and the backend
 * fails the match. The server credential stays in memory and is never logged.
 */
UCLASS()
class UVeyraMatchServerSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	/** Takes the assignment, or returns why it could not. */
	FString TakeAssignment(UVeyraMatchHostSubsystem& MatchHost);

	/** Waits for one line on standard input, up to the configured time. Returns the problem, or empty. */
	static FString WaitForLine(FString& OutLine);

	void OnAcceptingPlayers();
	void OnMatchEnded(const FVeyraMatchResult& Result);

	/** Posts Body to Path, trying again after transient failures, and calls OnDone with the outcome. */
	void Report(const FString& What, const FString& Path, const FString& Body, int32 Attempt, TFunction<void(bool)> OnDone);

	void Fail(const FString& Reason);

	UPROPERTY()
	TObjectPtr<UVeyraMatchHostSubsystem> Host;

	TUniquePtr<FVeyraBackendClient> Backend;
	FString MatchId;
	/** The match-server credential ("vms_"). */
	FString ServerCredential;
	FDelegateHandle AcceptingHandle;
	FDelegateHandle EndedHandle;
	FTSTicker::FDelegateHandle RetryTicker;
	/** Quits once the ended match's players have watched its end. */
	FTSTicker::FDelegateHandle QuitTicker;
	bool bReadySent = false;
	bool bResultSent = false;
};
