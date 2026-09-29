// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Battleground/VeyraBattlegroundTypes.h"
#include "Engine/TimerHandle.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "Teams/VeyraTeam.h"

#include "VeyraBattlegroundSubsystem.generated.h"

class AVeyraStructure;
struct FVeyraBattlegroundLayout;

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

/**
 * The battleground's world state on the server (ADR-011 §2, §8, §9, §12): it spawns the structures
 * from the layout when the map is the battleground, sets their towers shooting, keeps their
 * invulnerability to the rules as they fall (lane order, base towers, the Prime Well), rebuilds
 * inhibitors, regenerates each Prime Well while its inhibitors stand, and announces each
 * destruction, so Match can grant Team Flux and decide victory. It is the one listener to Combat's
 * hostile damage, routing tower aggression (Combat Bible §33). Timers run on world time, so a pause
 * holds them. A client's instance does nothing.
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
	void SpawnStructures(const FVeyraBattlegroundLayout& Layout);

	/** Whether this world is the battleground, with its structures spawned. */
	bool HasStructures() const { return !Structures.IsEmpty(); }

	const TArray<TObjectPtr<AVeyraStructure>>& GetStructures() const { return Structures; }

	/** The structure of Kind on Team, in Lane (none for the base), at Order; null if there is none. */
	AVeyraStructure* FindStructure(EVeyraTeam Team, EVeyraStructureKind Kind, TOptional<EVeyraLane> Lane, int32 Order) const;

	/** The developer siege's next target among Defenders' structures (VeyraStructureRules::NextToSiege); null when none can be damaged. */
	AVeyraStructure* NextSiegeTarget(EVeyraTeam Defenders) const;

	/**
	 * Server: stops shooting, rebuilding and regenerating, as when the match ends (Economy Bible §8.2:
	 * nothing more happens after victory).
	 */
	void Stop();

	/** Server: a structure was destroyed. */
	TMulticastDelegate<void(const FVeyraStructureDestroyedEvent&)> OnStructureDestroyed;

	/** Server: brings each structure's invulnerability in line with the rules. Changes call it; tests may too. */
	void RefreshInvulnerability();

	/** Server: one tick of Prime Well regeneration, Seconds long. Its timer calls it; tests may too. */
	void RegeneratePrimeWells(double Seconds);

private:
	void OnDeath(const FVeyraDeathEvent& Death);

	/** An enemy Vanguard that damages a defending Vanguard, both in a tower's range, draws that tower's priority (§33). */
	void OnHostileDamage(const FVeyraHostileDamageEvent& Event);
	void RebuildInhibitor(TWeakObjectPtr<AVeyraStructure> Inhibitor);
	void OnRegenerationTimer();
	bool IsServer() const;

	UPROPERTY()
	TArray<TObjectPtr<AVeyraStructure>> Structures;

	TMap<TWeakObjectPtr<AVeyraStructure>, FTimerHandle> RebuildTimers;
	FTimerHandle RegenerationTimer;
	FDelegateHandle DeathHandle;
	FDelegateHandle HostileDamageHandle;
	bool bStopped = false;
};
