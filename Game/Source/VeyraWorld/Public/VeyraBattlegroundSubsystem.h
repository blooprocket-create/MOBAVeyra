// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Battleground/VeyraBattlegroundTypes.h"
#include "Content/VeyraContentId.h"
#include "Engine/TimerHandle.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "Teams/VeyraTeam.h"
#include "Tuning/VeyraWorldTuning.h"

#include "VeyraBattlegroundSubsystem.generated.h"

class AVeyraFluxborn;
class AVeyraStructure;

/** A structure destroyed, as World reports it to the systems that own the outcome (ADR-011 §3). */
struct FVeyraStructureDestroyedEvent
{
	TWeakObjectPtr<AVeyraStructure> Structure;
	EVeyraStructureKind Kind = EVeyraStructureKind::LaneSpire;

	/** The team that lost it; the other side destroyed it. */
	EVeyraTeam Team = EVeyraTeam::None;
	TOptional<EVeyraLane> Lane;

	/** Combat's account of its death: who finished it and who fought it. */
	FVeyraDeathEvent Death;
};

/** A team's Team Flux as World holds it (ADR-011 §3, §10): what Match routes to it from Flux. */
struct FVeyraTeamFluxStrength
{
	/** Permanent Flux plus the temporary grants still counting. */
	double ActiveFlux = 0.0;

	/** The strength it gives the team's Fluxborn: 1 is their own. */
	double HealthMultiplier = 1.0;
	double DamageMultiplier = 1.0;
};

/**
 * The battleground's world state on the server (ADR-011 §2, §7–§9, §12): it spawns the structures
 * from the layout when the map is the battleground, sets their towers shooting, keeps their
 * invulnerability to the rules as they fall (lane order, base towers, the Prime Well), rebuilds
 * inhibitors, regenerates each Prime Well while its inhibitors stand, and announces each
 * destruction, so Match can grant Team Flux and decide victory. It spawns the lane Fluxborn in waves
 * on the match clock, keeps them as strong as their team's Flux, and removes the fallen. It is the one listener to Combat's
 * hostile damage, routing aggression to the towers and Fluxborn near it (Combat Bible §33;
 * Battleground Bible §19). Timers run on world time, so a pause holds them. A client's instance does
 * nothing.
 */
UCLASS()
class VEYRAWORLD_API UVeyraBattlegroundSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Server: spawns every structure the layout places. The map's marker does this; tests call it with their own layout. */
	void SpawnStructures(const FVeyraBattlegroundLayout& InLayout);

	/** Whether this world is the battleground, with its structures spawned. */
	bool HasStructures() const { return !Structures.IsEmpty(); }

	const TArray<TObjectPtr<AVeyraStructure>>& GetStructures() const { return Structures; }

	/** The structure of Kind on Team, in Lane (none for the base), at Order; null if there is none. */
	AVeyraStructure* FindStructure(EVeyraTeam Team, EVeyraStructureKind Kind, TOptional<EVeyraLane> Lane, int32 Order) const;

	/** The developer siege's next target among Defenders' structures (VeyraStructureRules::NextToSiege); null when none can be damaged. */
	AVeyraStructure* NextSiegeTarget(EVeyraTeam Defenders) const;

	/**
	 * Server: spawns a Fluxborn of Kind for Team at its end of Lane, as strong as its team's Flux,
	 * to walk the lane and on to the enemy's Prime Well. Null if refused: no battleground, no such
	 * lane or kind, or the battleground has stopped.
	 */
	AVeyraFluxborn* SpawnFluxborn(const FVeyraContentId& Kind, EVeyraTeam Team, EVeyraLane Lane);

	/** The living Fluxborn. */
	TArray<AVeyraFluxborn*> GetFluxborn() const;

	/**
	 * Server: starts the Fluxborn waves, the match clock at 0 now (Battleground Bible §17). Match calls
	 * it when the match goes live; practice has waves too. Does nothing without a battleground, once
	 * started, or once stopped.
	 */
	void StartWaves();

	/**
	 * Server: spawns wave Index in every lane for both teams, each team's units leaving its base in a
	 * file (World.json waves). Its timer calls it; tests may too.
	 */
	void SpawnWave(int32 Index);

	/** How many waves have spawned. */
	int32 GetWavesSpawned() const { return NextWave; }

	/** Whether Team's inhibitor in Lane is down now. */
	bool IsInhibitorDown(EVeyraTeam Team, EVeyraLane Lane) const;

	/** Server: Team's Team Flux changed. Its living Fluxborn take the new strength at once, keeping their Health's percentage. */
	void SetTeamFlux(EVeyraTeam Team, const FVeyraTeamFluxStrength& Flux);

	/** Team's Team Flux as World last heard it. */
	FVeyraTeamFluxStrength GetTeamFlux(EVeyraTeam Team) const;

	/**
	 * Server: stops shooting, rebuilding, regenerating and the Fluxborn's march, as when the match
	 * ends (Economy Bible §8.2: nothing more happens after victory).
	 */
	void Stop();

	/** Server: a structure was destroyed. */
	TMulticastDelegate<void(const FVeyraStructureDestroyedEvent&)> OnStructureDestroyed;

	/** Server: brings each structure's invulnerability in line with the rules. Changes call it; tests may too. */
	void RefreshInvulnerability();

	/** Server: one tick of Prime Well regeneration, Seconds long. Its timer calls it; tests may too. */
	void RegeneratePrimeWells(double Seconds);

	/**
	 * Server: brings each structure's backdoor protection Seconds up to date (Battleground Bible §19):
	 * none while an attacking Fluxborn is within its radius, else climbing to the maximum. Its timer
	 * calls it; tests may too.
	 */
	void UpdateBackdoorProtection(double Seconds);

private:
	void OnDeath(const FVeyraDeathEvent& Death);
	void OnFluxbornDied(AVeyraFluxborn& Fluxborn);

	/**
	 * An enemy Vanguard that damages a defending Vanguard draws the priority of each tower with both in
	 * range (Combat Bible §33) and the aggression of the defender's Fluxborn near it (Battleground
	 * Bible §19).
	 */
	void OnHostileDamage(const FVeyraHostileDamageEvent& Event);
	void RebuildInhibitor(TWeakObjectPtr<AVeyraStructure> Inhibitor);
	void OnRegenerationTimer();
	void OnBackdoorTimer();

	/** The next wave is due: it spawns, and the one after is scheduled. */
	void OnWaveTimer();
	void ScheduleWave();
	void SpawnWaveUnit(FVeyraContentId Kind, EVeyraTeam Team, EVeyraLane Lane);
	bool IsServer() const;
	FVeyraTeamFluxStrength& FluxOf(EVeyraTeam Team);

	UPROPERTY()
	TArray<TObjectPtr<AVeyraStructure>> Structures;

	/** The layout the structures were spawned from, whose lanes the Fluxborn walk. */
	TOptional<FVeyraBattlegroundLayout> Layout;

	TArray<TWeakObjectPtr<AVeyraFluxborn>> Fluxborn;
	FVeyraTeamFluxStrength FluxA;
	FVeyraTeamFluxStrength FluxB;

	int32 NextWave = 0;
	/** World time when the waves began: the match clock's 0, since a pause holds both. */
	double WavesStartedAt = 0.0;
	bool bWavesStarted = false;
	FTimerHandle WaveTimer;
	/** Each unit still to leave its base in a file. */
	TArray<FTimerHandle> FileTimers;

	TMap<TWeakObjectPtr<AVeyraStructure>, FTimerHandle> RebuildTimers;
	FTimerHandle RegenerationTimer;
	FTimerHandle BackdoorTimer;
	FDelegateHandle DeathHandle;
	FDelegateHandle HostileDamageHandle;
	bool bStopped = false;
};
