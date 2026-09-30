// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"
#include "Votes/VeyraVoteRules.h"

#include "VeyraVoteSubsystem.generated.h"

class AVeyraPlayerState;

/**
 * Server: the match's votes (Match Flow Bible §7–§10; ADR-019 §4). Players start and answer them
 * through their controllers; this owner records ballots, casts the automatic ones, tallies on real
 * time (a pause stops world time, and an intermission must still end) and announces what passed. The
 * game mode acts on it: a remake or surrender ends the match, a pause opens the intermission, which
 * ends early on a resume vote or by itself. The GameState shows the open vote to every player.
 */
UCLASS()
class VEYRAMATCH_API UVeyraVoteSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** Server: votes are taken while the match is live, from now. */
	void Start();

	/** Server: the match ended; no vote goes on. */
	void Stop();

	/** Server: Requester starts a vote of Kind; it counts as its YES. Returns why not, or None. */
	EVeyraVoteRefusal Request(const AVeyraPlayerState& Requester, EVeyraVoteKind Kind);

	/** Server: Voter answers the open vote. Returns why the ballot does not count, or None. */
	EVeyraVoteRefusal CastBallot(const AVeyraPlayerState& Voter, bool bYes);

	/** Server: a passed pause opened an intermission, which ends by itself after the tuned time. */
	void BeginIntermission();

	/** Server: the intermission ended, early or on time. */
	void EndIntermission();

	/** A vote passed: the kind, and the team that voted (None for everyone). */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnVotePassed, EVeyraVoteKind /*Kind*/, EVeyraTeam /*Team*/);
	FOnVotePassed OnVotePassed;

	/** The intermission ran its full time. */
	FSimpleMulticastDelegate OnIntermissionOver;

	bool IsVoteOpen() const { return Box.IsSet(); }

private:
	bool Tick(float DeltaSeconds);
	TArray<FVeyraVoter> GatherVoters() const;
	void Publish(TConstArrayView<FVeyraVoter> Voters, double Now) const;
	static double RealNow();

	TOptional<FVeyraBallotBox> Box;
	FVeyraVoteCooldowns Cooldowns;
	/** Real seconds when the intermission ends by itself. */
	TOptional<double> IntermissionEndsAt;
	bool bRunning = false;
	FTSTicker::FDelegateHandle TickHandle;
};
