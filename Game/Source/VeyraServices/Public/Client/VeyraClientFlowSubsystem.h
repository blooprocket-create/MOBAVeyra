// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Client/VeyraClientFlow.h"
#include "Containers/Ticker.h"
#include "Engine/EngineBaseTypes.h"
#include "Handoff/VeyraPipeLineReader.h"
#include "Handoff/VeyraPipeLineWriter.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/UniquePtr.h"

#include "VeyraClientFlowSubsystem.generated.h"

class AVeyraGameState;
class UNetDriver;

/**
 * Hosts the client-state coordinator, FVeyraClientFlow, in a game started by a launcher with
 * -VeyraLaunchCode=stdin (ADR-010 §2). It gives the flow the engine: the clock, standard input and
 * output, travel, and word of loaded worlds, match phases and failed connections.
 *
 * Presentation reads the snapshot, listens for changes, and asks through the intents of GetClient;
 * whether each is allowed now is CanIssue's to say.
 */
UCLASS()
class VEYRASERVICES_API UVeyraClientFlowSubsystem : public UGameInstanceSubsystem, public IVeyraClientFlowHost
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** What presentation, and scripts standing in for the player, may observe and ask. Valid until OnClientEnding. */
	IVeyraClientIntents& GetClient() const { return *Flow; }

	/**
	 * Fires once as the subsystem deinitializes, while the client is still valid. A GameInstance's
	 * subsystems deinitialize in no set order, so a subsystem holding the client lets go of it here.
	 */
	FSimpleMulticastDelegate& OnClientEnding() { return ClientEnding; }

private:
	// IVeyraClientFlowHost
	virtual double Now() const override;
	virtual EVeyraPipeRead PollLaunchCode(FString& OutLine) override;
	virtual void WriteHandshake(const FString& Line) override;
	virtual bool TravelToMatch(const FString& Address, const FString& Ticket) override;
	virtual void TravelToFrontEnd() override;
	virtual void QuitGame() override;

	/** Why the game cannot sign in as configured; empty when it can. */
	FString FindConfigurationProblem(FString& OutBuildVersion) const;

	bool Tick(float DeltaSeconds);

	/** Follows the match's phase whenever a connected world's game state appears. */
	void WatchGameState();

	void OnPostLoadMap(UWorld* World);
	void OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void OnTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
	bool IsOurs(const UWorld* World) const;

	TUniquePtr<IVeyraBackendTransport> Backend;
	TUniquePtr<FVeyraPipeLineReader> LaunchCodeReader;
	TUniquePtr<FVeyraPipeLineWriter> Handshake;
	TUniquePtr<FVeyraClientFlow> Flow;
	FSimpleMulticastDelegate ClientEnding;
	TWeakObjectPtr<AVeyraGameState> WatchedGameState;
	FDelegateHandle PhaseHandle;
	FTSTicker::FDelegateHandle TickHandle;
	FDelegateHandle PostLoadMapHandle;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
};
