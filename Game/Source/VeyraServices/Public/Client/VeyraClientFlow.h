// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Backend/VeyraBackendTransport.h"
#include "Client/VeyraClientFlowTypes.h"
#include "Delegates/Delegate.h"
#include "Handoff/VeyraPipeLineReader.h"
#include "Templates/SharedPointer.h"
#include "VeyraMatchTypes.h"

class UVeyraServicesSettings;

/** What the flow needs from the engine. The game's subsystem provides it; tests fake it. */
class IVeyraClientFlowHost
{
public:
	virtual ~IVeyraClientFlowHost() = default;

	/** Seconds on a clock that only goes forward. */
	virtual double Now() const = 0;

	/** Looks at standard input for the launch code without waiting. */
	virtual EVeyraPipeRead PollLaunchCode(FString& OutLine) = 0;

	/** Writes a launch-handshake line on standard output (ADR-010 §5). */
	virtual void WriteHandshake(const FString& Line) = 0;

	/** Travels to the match server at Address, such as "127.0.0.1:7780", with the join ticket. False if it cannot. */
	virtual bool TravelToMatch(const FString& Address, const FString& Ticket) = 0;

	/** Forgets the join ticket and travels to the front end. */
	virtual void TravelToFrontEnd() = 0;

	/** Quits the game: only ever the player's choice. */
	virtual void QuitGame() = 0;
};

/** The flow's waits and retries, from UVeyraServicesSettings. */
struct FVeyraClientFlowConfig
{
	/** What the game reports when it redeems its launch code. */
	FString BuildVersion;
	double LaunchCodeReadTimeoutSeconds = 0.0;
	int32 RequestAttempts = 0;
	double RetryIntervalSeconds = 0.0;
	double SelectPollIntervalSeconds = 0.0;
	double MatchPollIntervalSeconds = 0.0;
	double MatchWaitTimeoutSeconds = 0.0;
	double ResultPollIntervalSeconds = 0.0;
	double ResultWaitTimeoutSeconds = 0.0;
	double ReconnectPollIntervalSeconds = 0.0;

	static VEYRASERVICES_API FVeyraClientFlowConfig FromSettings(const UVeyraServicesSettings& Settings, const FString& BuildVersion);
};

/**
 * The client-state coordinator (ADR-004, ADR-010 §2): where a signed-in game is, what the player may
 * ask for, and every conversation with the backend on the player's behalf.
 *
 * It signs in with the launch code from standard input and keeps the game session in memory only.
 * After sign-in, a live match leads to Reconnect-only, then a select in progress resumes, then a
 * player without a starter chooses one; otherwise the shell. Practice opens a champion select; once
 * every pick is locked it waits for the match's server, joins it with the join ticket, and when the
 * match ends travels back to the front end and waits for the backend's verified result. A lost
 * connection leads back through the backend: Reconnect-only while the match still runs, otherwise
 * its result.
 *
 * The presentation observes the snapshot and asks through intents; the flow and the backend decide.
 * A failure shows as the snapshot's problem, with Retry when repeating the step can help. The flow
 * never quits by itself. It logs "VeyraClientFlow: ..." and never logs a credential.
 */
class VEYRASERVICES_API FVeyraClientFlow
{
public:
	/** Backend and Host must outlive the flow. Callbacks that arrive after it is gone are ignored. */
	FVeyraClientFlow(IVeyraBackendTransport& InBackend, IVeyraClientFlowHost& InHost, FVeyraClientFlowConfig InConfig);
	~FVeyraClientFlow();

	FVeyraClientFlow(const FVeyraClientFlow&) = delete;
	FVeyraClientFlow& operator=(const FVeyraClientFlow&) = delete;

	/**
	 * Tells the launcher the game awaits its launch code and starts reading it. With a problem in
	 * the game's own settings or command line, signing in fails at once with that problem instead.
	 */
	void Start(const FString& ConfigurationProblem = FString());

	/** Reads the launch code and runs the waits that are due. The host calls it every frame. */
	void Tick();

	const FVeyraClientSnapshot& GetSnapshot() const { return Snapshot; }

	/** Broadcast after every change to the snapshot. */
	FSimpleMulticastDelegate& OnChanged() { return Changed; }

	/** Whether the intent is allowed now. Presentation enables its controls by this. */
	bool CanIssue(EVeyraClientIntent Intent) const;

	/** Seconds left on the select's pick timer, by the backend's clock as last read. */
	double GetRemainingPickSeconds() const;

	/** Which intents a state allows at all, before the snapshot's details: a pure table. */
	static bool IsIntentAllowed(EVeyraClientState State, EVeyraClientIntent Intent);

	/** Each intent is refused, returning false, when CanIssue says no or its argument is not on offer. */
	bool ChooseStarter(const FString& VanguardId);
	bool StartPractice();
	bool HoverVanguard(const FString& VanguardId);
	bool LockVanguard(const FString& VanguardId);
	bool Reconnect();
	bool ContinueFromResults();
	bool Retry();
	bool Quit();

	/** The host loaded a world. */
	void NotifyWorld(EVeyraClientWorld World);

	/** The match's replicated phase changed, or became known. */
	void NotifyMatchPhase(EVeyraMatchPhase Phase);

	/** The connection to a match server failed, or travel to it did. */
	void NotifyConnectionFailed(const FString& Reason);

private:
	enum class EVerb : uint8
	{
		Get,
		Post,
		Put,
	};

	struct FWait
	{
		double At = 0.0;
		uint32 Epoch = 0;
		TFunction<void()> Run;
	};

	using FAnswer = TFunction<void(const FVeyraBackendResponse&)>;

	// Signing in.
	void TickLaunchCode();
	void Redeem(const FString& LaunchCode);
	void OnRedeemed(const FVeyraBackendResponse& Response);
	void FailSignIn(VeyraLaunchHandshake::EFailure Failure, const FString& Reason);

	// Where the player is.
	void Resume();
	void LoadProfile();
	void EnterSelecting(const VeyraBackendProtocol::FSelect& Select);
	void ApplySelect(const VeyraBackendProtocol::FSelect& Select);
	void LoadAvailableVanguards();
	void PollSelect();
	void LearnHowSelectEnded();
	void EnterMatchStarting(const FString& MatchId);
	void PollMatch();
	void Connect(const VeyraBackendProtocol::FMyMatch& Match);
	void LeaveMatch(bool bEnded, const FString& Notice);
	void EnterAwaitingResults();
	void PollResult();
	void ShowResults(TOptional<VeyraBackendProtocol::FMatchOutcome> Outcome);
	void EnterReconnectOnly(const FString& MatchId);
	void PollReconnect();
	void EndSession();

	/** Enters a state: its waits, problem, notice and busy flag start afresh, and answers to the old state's requests are ignored. */
	void Enter(EVeyraClientState NewState, const FString& Notice = FString());

	/** Runs Run after Seconds, unless the state changes first. */
	void After(double Seconds, TFunction<void()> Run);

	/**
	 * Sends a request with the game session. An answer that arrives after the state changed is
	 * ignored. A refused session ends it. No answer, or a server error, is tried again up to the
	 * configured attempts, then shown as a problem with Retry. Anything else goes to OnAnswer.
	 */
	void Call(EVerb Verb, const FString& Path, const FString& Body, FAnswer OnAnswer, int32 Attempt = 1);

	/** Shows a problem. With Retry set, the Retry intent runs it. */
	void ShowProblem(const FString& Code, const FString& Message, TFunction<void()> RetryStep);

	/** Shows the backend's refusal of a request, by its error code. */
	void ShowRefusal(const FVeyraBackendResponse& Response, const TCHAR* What, TFunction<void()> RetryStep);

	/** Shows an answer that could not be read. */
	void ShowBadAnswer(const TCHAR* What, const FString& Problem, TFunction<void()> RetryStep);

	void SetBusy(bool bBusy);
	void Broadcast();
	void Log(const FString& Message) const;

	IVeyraBackendTransport& Backend;
	IVeyraClientFlowHost& Host;
	FVeyraClientFlowConfig Config;
	/** Expires with the flow, so late callbacks know to do nothing. */
	TSharedRef<bool> Alive;

	FVeyraClientSnapshot Snapshot;
	FSimpleMulticastDelegate Changed;

	/** Increases on every state change; a request or wait from an older state is stale. */
	uint32 Epoch = 0;
	TArray<FWait> Waits;
	TFunction<void()> RetryAction;

	bool bReadingLaunchCode = false;
	double LaunchCodeDeadline = 0.0;
	/** The game session credential ("vgs_"). In memory only. */
	FString GameSession;
	FString SelectId;
	double MatchDeadline = 0.0;
	double ResultDeadline = 0.0;
	/** The player left their match because it ended, not because the connection failed. */
	bool bMatchEnded = false;
};
