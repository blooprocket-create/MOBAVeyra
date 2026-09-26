// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Join/VeyraMatchAssignment.h"
#include "Subsystems/EngineSubsystem.h"

#include "VeyraMatchHostSubsystem.generated.h"

/**
 * What a match server hosts, and what it tells whoever started it (ADR-007). The match's input is
 * its assignment, set once at start-up before the first map loads; its outputs are two events. The
 * trusted-services client (VeyraServices) sets the one and listens to the others, so VeyraMatch
 * never talks to the backend itself.
 *
 * A server with no assignment is a developer server that accepts direct connections in development
 * builds (ADR-007 §9).
 */
UCLASS()
class VEYRAMATCH_API UVeyraMatchHostSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	static UVeyraMatchHostSubsystem* Get();

	/**
	 * Sets the match this server hosts. It checks the assignment against this build's rules: a
	 * match ID, at least one participant, each on side A or B, no side larger than the tuned team
	 * size, and every account and ticket hash present and unique. On any problem it keeps no
	 * assignment and returns every problem.
	 */
	TArray<FString> SetAssignment(FVeyraMatchAssignment InAssignment);

	/** Forgets the assignment. For tests, which share one engine. */
	void ClearAssignment();

	const TOptional<FVeyraMatchAssignment>& GetAssignment() const { return Assignment; }

	/** The match's map has loaded and the server accepts players. */
	FSimpleMulticastDelegate OnAcceptingPlayers;

	/** The match has ended with this result. */
	TMulticastDelegate<void(const FVeyraMatchResult&)> OnMatchEnded;

private:
	TOptional<FVeyraMatchAssignment> Assignment;
};
