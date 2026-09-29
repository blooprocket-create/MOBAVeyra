// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Delegates/IDelegateInstance.h"
#include "Teams/VeyraTeam.h"
#include "UObject/WeakObjectPtr.h"

class APlayerState;
class UAbilitySystemComponent;
class UVeyraBattlegroundSubsystem;
class UVeyraFluxWellSubsystem;
class UVeyraJungleSubsystem;
class UVeyraRewardSubsystem;
class UVeyraTeamFluxSubsystem;
class UWorld;
struct FVeyraFluxWellSecuredEvent;
struct FVeyraStructureDestroyedEvent;

/**
 * Match's side of the battleground (ADR-011 §2, §3): World and Flux are peers that never call each
 * other, so this routes between them. A destroyed structure grants its destroyers the Team Flux its
 * kind gives; every change to a team's Flux reaches World, whose Fluxborn follow it; and a destroyed
 * Prime Well is reported to the game mode, which decides victory; a secured Flux Well grants its side
 * that source's Team Flux. A team's permanent Flux unlocks its participants' Flux Spell slots
 * (ADR-015 §4). It starts Vision, which runs until the world ends (ADR-016 §2), and it starts the
 * battleground's waves, jungle and Wells as the match goes live, and stops them when it ends
 * (ADR-014 §6). The game mode owns one; server only.
 */
class FVeyraBattlegroundLink
{
public:
	/** Called with the side that destroyed the other side's Prime Well. */
	DECLARE_DELEGATE_OneParam(FOnPrimeWellDestroyed, EVeyraTeam /*Winner*/);

	~FVeyraBattlegroundLink();

	/** Connects World and Flux in World. Does nothing where neither exists. */
	void Start(UWorld& World, FOnPrimeWellDestroyed InOnPrimeWellDestroyed);

	/** Disconnects, and stops the battleground's timers and its rewards, as when the match ends. */
	void Stop();

	/** Starts the battleground's Fluxborn waves, its jungle and its Flux Wells, as the match goes live (Battleground Bible §6, §17). */
	void StartLive();

	/**
	 * Developer builds: Source, on Team, destroys the enemies' next structure in siege order with a
	 * lethal developer hit through the damage pipeline (ADR-011 §15). Returns whether one fell.
	 */
	bool DeveloperSiege(UAbilitySystemComponent& Source, EVeyraTeam Team);

	/**
	 * Unlocks the Flux Spell slots Participant's team's permanent Flux has reached (Battleground Bible
	 * §14), as when its Vanguard first spawns. Temporary Flux never counts.
	 */
	void UnlockSpellSlots(APlayerState& Participant) const;

private:
	void OnStructureDestroyed(const FVeyraStructureDestroyedEvent& Event);
	void OnFluxWellSecured(const FVeyraFluxWellSecuredEvent& Event);

	/**
	 * Team's Flux changed, by a grant or an expiry: its participants' spell slots and World's Fluxborn
	 * follow it (ADR-011 §3, §10; ADR-015 §4).
	 */
	void OnTeamFluxChanged(EVeyraTeam Team);

	TWeakObjectPtr<UWorld> MatchWorld;
	TWeakObjectPtr<UVeyraBattlegroundSubsystem> Battleground;
	TWeakObjectPtr<UVeyraTeamFluxSubsystem> Flux;
	TWeakObjectPtr<UVeyraRewardSubsystem> Rewards;
	TWeakObjectPtr<UVeyraJungleSubsystem> Jungle;
	TWeakObjectPtr<UVeyraFluxWellSubsystem> FluxWells;
	FDelegateHandle SecuredHandle;
	FDelegateHandle DestroyedHandle;
	FDelegateHandle FluxChangedHandle;
	FOnPrimeWellDestroyed OnPrimeWellDestroyed;
};
