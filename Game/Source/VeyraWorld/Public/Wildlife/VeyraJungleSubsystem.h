// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Engine/TimerHandle.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "Teams/VeyraTeam.h"

#include "VeyraJungleSubsystem.generated.h"

class AVeyraWildlife;

/** One camp as it stands, for bots and the HUD. */
struct FVeyraCampState
{
	/** Its index among the camps: Team A's half in World.json's order, then Team B's. */
	int32 Index = INDEX_NONE;

	/** The half of the battleground it lies in. */
	EVeyraTeam Half = EVeyraTeam::None;

	FVeyraContentId Species;
	FVector2D Center = FVector2D::ZeroVector;

	/** Its living creatures. */
	int32 Alive = 0;

	/** When it next spawns, in world time; 0 while its creatures stand. */
	double SpawnsAt = 0.0;
};

/**
 * The jungle on the server (Battleground Bible §7, §8, §17; ADR-014 §2). Once Match starts it as the
 * match goes live, it spawns each camp at its spawn time on the match clock, on Team A's half as
 * World.json places it and on Team B's as its mirror. A hurt creature's whole camp answers the
 * attacker. Each creature's death is reported to Economy with its species; a camp's last death grants
 * its species' trait to the Vanguard credited with it and starts the camp's own respawn timer. Timers
 * run on world time, so a pause holds them. It needs the battleground. A client's instance does
 * nothing.
 */
UCLASS()
class VEYRAWORLD_API UVeyraJungleSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Server: starts the camps' clocks, the match clock at 0 now. Does nothing without the battleground, once started, or once stopped. */
	void Start();

	/** Server: spawns no more camps, as when the match ends. Standing creatures stay. */
	void Stop();

	/** Server: spawns camp Index's creatures now, if it has none standing. Its timer calls it; tests may too. Returns how many spawned. */
	int32 SpawnCamp(int32 Index);

	/** Every camp, in index order. Empty before the jungle starts. */
	TArray<FVeyraCampState> GetCamps() const;

	/** Camp Index's living creatures. */
	TArray<AVeyraWildlife*> GetCreatures(int32 Index) const;

private:
	/** A camp on the server. */
	struct FCamp
	{
		int32 TuningIndex = INDEX_NONE;
		EVeyraTeam Half = EVeyraTeam::None;
		FVector2D Center = FVector2D::ZeroVector;
		TArray<TWeakObjectPtr<AVeyraWildlife>> Creatures;
		FTimerHandle Timer;
		double SpawnsAt = 0.0;
	};

	void OnDeath(const FVeyraDeathEvent& Death);
	void OnHostileDamage(const FVeyraHostileDamageEvent& Event);
	void OnCreatureDied(AVeyraWildlife& Creature, const FVeyraDeathEvent& Death);
	void ScheduleSpawn(int32 Index, double Seconds);
	bool IsServer() const;

	TArray<FCamp> Camps;
	FDelegateHandle DeathHandle;
	FDelegateHandle HostileDamageHandle;
	bool bStarted = false;
	bool bStopped = false;
};
