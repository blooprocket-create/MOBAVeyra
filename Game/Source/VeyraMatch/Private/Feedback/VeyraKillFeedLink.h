// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Delegates/IDelegateInstance.h"
#include "UObject/WeakObjectPtr.h"

class UVeyraCombatEventSubsystem;
class UWorld;
struct FVeyraDeathEvent;

/**
 * Match's kill feed (ADR-065 §10): it hears Combat's deaths on the server and sends every player a line for each
 * Vanguard's and structure's fall, marking the match's first takedown First Blood. A bot's participant has no controller
 * and is sent nothing. The game mode owns one. Server only.
 */
class FVeyraKillFeedLink
{
public:
	~FVeyraKillFeedLink();

	/** Listens to World's deaths. */
	void Start(UWorld& World);

	void Stop();

private:
	void OnDeath(const FVeyraDeathEvent& Death);

	TWeakObjectPtr<UVeyraCombatEventSubsystem> Events;
	FDelegateHandle DeathHandle;
	bool bFirstBloodTaken = false;
};
