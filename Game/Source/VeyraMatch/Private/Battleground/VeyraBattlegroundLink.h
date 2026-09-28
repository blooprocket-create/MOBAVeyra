// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Delegates/IDelegateInstance.h"
#include "Teams/VeyraTeam.h"
#include "UObject/WeakObjectPtr.h"

class UAbilitySystemComponent;
class UVeyraBattlegroundSubsystem;
class UVeyraTeamFluxSubsystem;
class UWorld;
struct FVeyraStructureDestroyedEvent;

/**
 * Match's side of the battleground (ADR-011 §2, §3): World and Flux are peers that never call each
 * other, so this routes between them. A destroyed structure grants its destroyers the Team Flux its
 * kind gives; every change to a team's Flux reaches World, whose Fluxborn follow it; and a destroyed
 * Prime Well is reported to the game mode, which decides victory. The game mode owns one; server only.
 */
class FVeyraBattlegroundLink
{
public:
	/** Called with the side that destroyed the other side's Prime Well. */
	DECLARE_DELEGATE_OneParam(FOnPrimeWellDestroyed, EVeyraTeam /*Winner*/);

	~FVeyraBattlegroundLink();

	/** Connects World and Flux in World. Does nothing where neither exists. */
	void Start(UWorld& World, FOnPrimeWellDestroyed InOnPrimeWellDestroyed);

	/** Disconnects, and stops the battleground's timers, as when the match ends. */
	void Stop();

	/**
	 * Developer builds: Source, on Team, destroys the enemies' next structure in siege order with a
	 * lethal developer hit through the damage pipeline (ADR-011 §15). Returns whether one fell.
	 */
	bool DeveloperSiege(UAbilitySystemComponent& Source, EVeyraTeam Team);

private:
	void OnStructureDestroyed(const FVeyraStructureDestroyedEvent& Event);

	/** Team's Flux changed, by a grant or an expiry: World's Fluxborn follow it (ADR-011 §3, §10). */
	void OnTeamFluxChanged(EVeyraTeam Team);

	TWeakObjectPtr<UVeyraBattlegroundSubsystem> Battleground;
	TWeakObjectPtr<UVeyraTeamFluxSubsystem> Flux;
	FDelegateHandle DestroyedHandle;
	FDelegateHandle FluxChangedHandle;
	FOnPrimeWellDestroyed OnPrimeWellDestroyed;
};
