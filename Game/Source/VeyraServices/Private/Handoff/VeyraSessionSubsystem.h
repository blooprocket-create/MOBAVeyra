// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Backend/VeyraBackendClient.h"
#include "Containers/Ticker.h"
#include "Handoff/VeyraPipeLineReader.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/UniquePtr.h"

#include "VeyraSessionSubsystem.generated.h"

namespace VeyraBackendProtocol
{
	struct FMyMatch;
}

/**
 * The game's side of the session handoff (ADR-005 L3, ADR-007 §1), started by -VeyraLaunchCode=stdin.
 * It reads the launch code from standard input, redeems it for a game session, waits until the
 * backend reports the player's match ready, gives the local player its join ticket and travels to
 * the match server. Progress is logged as "VeyraHandoff: ..."; any failure logs "VeyraHandoff:
 * FAIL: <reason>" and quits. The launch code, the game session and the ticket stay in memory and
 * are never logged.
 */
UCLASS()
class UVeyraSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	bool TickReadLaunchCode(float DeltaSeconds);
	void Redeem(const FString& LaunchCode);
	void OnRedeemed(const FVeyraBackendResponse& Response);
	void AskForMatch();
	void OnMatchAnswer(const FVeyraBackendResponse& Response);
	void Join(const VeyraBackendProtocol::FMyMatch& Match);
	void Progress(const FString& Message) const;
	void Fail(const FString& Reason);
	void StopTickers();

	TUniquePtr<FVeyraBackendClient> Backend;
	TUniquePtr<FVeyraPipeLineReader> Reader;
	FTSTicker::FDelegateHandle ReadTicker;
	FTSTicker::FDelegateHandle PollTicker;
	FString BuildVersion;
	/** The game session credential ("vgs_"). */
	FString GameSession;
	double ReadDeadline = 0.0;
	double MatchDeadline = 0.0;
	bool bSawMatch = false;
	bool bFinished = false;
};
