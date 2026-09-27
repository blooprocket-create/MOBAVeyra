// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Backend/VeyraBackendClient.h"
#include "Containers/Ticker.h"
#include "Handoff/VeyraLaunchHandshake.h"
#include "Handoff/VeyraPipeLineReader.h"
#include "Handoff/VeyraPipeLineWriter.h"
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
 * are never logged. A launcher learns when to send the code, and whether signing in worked, from the
 * launch-handshake lines on standard output (ADR-010 §5).
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
	/** Logs the failure and quits; before sign-in, it also tells the launcher why. */
	void Fail(const FString& Reason, TOptional<VeyraLaunchHandshake::EFailure> SignInFailure = {});
	void StopTickers();

	TUniquePtr<FVeyraBackendClient> Backend;
	TUniquePtr<FVeyraPipeLineReader> Reader;
	TUniquePtr<FVeyraPipeLineWriter> Handshake;
	bool bSignedIn = false;
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
