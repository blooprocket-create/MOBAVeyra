// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Rules/VeyraVisionRules.h"
#include "Subsystems/WorldSubsystem.h"
#include "Targeting/VeyraVisibility.h"
#include "Teams/VeyraTeam.h"
#include "TimerManager.h"

#include "VeyraVisionSubsystem.generated.h"

class APlayerController;
class APlayerState;
class AVeyraVisionTeamState;
class AVeyraWard;
class FVeyraFogGate;
struct FVeyraDeathEvent;

/**
 * A match's vision (ADR-016 §2, §3), on the server. Every pass (Vision.json's updateSeconds) it works
 * out which enemy and neutral units each team sees, from its living Vanguards, Fluxborn and standing
 * structures, and records it. Targeting, orders and bots read that record through the visibility
 * contract, and the fog gate makes each client receive exactly what its player sees, so what a
 * player may target and what reaches them always agree.
 *
 * Match starts it when the match begins and stops it at the end; a world it never started sees
 * everything (unit tests and development worlds without a match).
 */
UCLASS()
class VEYRAVISION_API UVeyraVisionSubsystem : public UWorldSubsystem, public IVeyraVisibility
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWardPlaced, const AVeyraWard&, APlayerState& /*Placer*/);

	UVeyraVisionSubsystem();
	virtual ~UVeyraVisionSubsystem() override;

	/** Server: a ward was placed, for the match statistics (ADR-017 §3). Its destruction is a death. */
	FOnWardPlaced OnWardPlaced;

	/** Server: starts working vision out, governing targeting and gating what clients receive. */
	void Start();

	/** Stops; the world sees everything again. */
	void Stop();

	bool IsStarted() const { return bStarted; }

	/** Works vision out now rather than at the next pass. For tests and for Start. */
	void UpdateNow();

	/**
	 * The battleground's Dense Fog, both teams' circles (Battleground Bible §11): an enemy Vanguard
	 * inside a volume is seen only by Vanguards inside the same volume (Vision Bible §2). Match gives
	 * it World's layout; abilities will add their own.
	 */
	void SetDenseFog(TArray<FVeyraFogCircle> Circles);

	/** The battleground's Dense Fog: a place every player knows (the fog itself is always seen). */
	TConstArrayView<FVeyraFogCircle> GetDenseFog() const { return Fog; }

	/**
	 * Server: places a ward for Placer's side at Where (Vision Bible §4), which its side receives at
	 * once and the enemy never, unless True Sight covers it. Null if it could not be placed.
	 */
	AVeyraWard* PlaceWard(APlayerState& Placer, const FVector& Where);

	/**
	 * Server: True Sight around Follow for Team for DurationSeconds (Vision Bible §5), as Sweeper grants
	 * it: Team sees the Invisible units it covers, such as enemy wards, and an outline of each enemy
	 * Vanguard inside Dense Fog it covers, which grants no targeting.
	 */
	void AddTrueSight(EVeyraTeam Team, const AActor& Follow, double Radius, double DurationSeconds);

	// IVeyraVisibility
	virtual bool CanSee(const UObject& Observer, const AActor& Target) const override;
	virtual bool IsVisibleToTeam(EVeyraTeam Team, const AActor& Target) const override;
	virtual void RevealArea(EVeyraTeam Team, const FVector& Centre, double Radius, double DurationSeconds) override;

	virtual void Deinitialize() override;

private:
	/** Whether Unit is hidden from those who cannot see it: every unit but structures and Flux Wells. */
	static bool IsGated(const AActor& Unit);

	/** Whether ordinary sight never shows Unit to its enemies, as a ward's Invisibility (Vision Bible §4). */
	static bool IsInvisible(const AActor& Unit);

	/** A destroyed ward pays its destroyer and leaves the battleground (§8). */
	void OnDeath(const FVeyraDeathEvent& Death);

	/** An area lit for a side for a while: ordinary vision, and over Dense Fog a presence sensor. */
	struct FSightArea
	{
		int32 Id = 0;
		EVeyraTeam Team = EVeyraTeam::None;
		FVector2D Centre = FVector2D::ZeroVector;
		double Radius = 0.0;
		double Until = 0.0;
	};

	/** True Sight around a unit, for a while. */
	struct FTrueSight
	{
		EVeyraTeam Team = EVeyraTeam::None;
		TWeakObjectPtr<const AActor> Follow;
		double Radius = 0.0;
		double Until = 0.0;
	};

	/** An outline a side keeps while it lingers. */
	struct FKeptOutline
	{
		FVector Location = FVector::ZeroVector;
		double Until = 0.0;
	};

	/** Whether Side's True Sight covers Unit now. */
	bool IsInTrueSight(EVeyraTeam Side, const AActor& Unit) const;

	/** Tells each side the presence its sensors feel and the outlines its True Sight draws (ADR-016 §5). */
	void UpdateSensors(const TArray<const AActor*>& Gated, double Now);

	/** Spawns each side's team state once, on the server. */
	void SpawnTeamStates(UWorld& World);

	void OnActorSpawned(AActor* Actor);

	/**
	 * Puts each player in the net condition groups of the participants they may see: their teammates',
	 * and those whose Vanguards they see now (ADR-016 §3). Out of sight, a player keeps the last values.
	 */
	void UpdateParticipantData(UWorld& World);

	/** What each side saw at the last pass: its enemies' and neutral units it may see and target. */
	TMap<EVeyraTeam, TSet<TWeakObjectPtr<const AActor>>> Seen;
	/**
	 * Every gated unit the last pass knew, and its sight sources: a unit that spawned since is judged
	 * against those sources at once, so it is not hidden until the next pass.
	 */
	TSet<TWeakObjectPtr<const AActor>> Known;
	TArray<FVeyraSightSource> Sources;

	/** The fog, and each circle's volume (VeyraVisionRules::ConnectVolumes). */
	TArray<FVeyraFogCircle> Fog;
	TArray<int32> FogVolumes;
	/** The enemy Vanguards inside fog at the last pass, and the volume each is in. */
	TMap<TWeakObjectPtr<const AActor>, int32> Fogged;
	/** What each Vanguard sees inside its own fog volume, which its team does not share (Vision Bible §2). */
	TMap<TWeakObjectPtr<const AActor>, TSet<TWeakObjectPtr<const AActor>>> FogSightings;
	TArray<FSightArea> SightAreas;
	int32 NextSightAreaId = 1;
	TArray<FTrueSight> TrueSights;
	TMap<EVeyraTeam, TMap<TWeakObjectPtr<const AActor>, FKeptOutline>> Outlined;
	/** When each sensor (a ward's ID, or an area's) last pinged each fog circle, so pings keep their cadence. */
	TMap<TPair<uint64, int32>, double> LastPings;
	TMap<EVeyraTeam, TWeakObjectPtr<AVeyraVisionTeamState>> TeamStates;
	/** The participants' groups each player is in now, so only changes are sent. */
	TMap<TWeakObjectPtr<APlayerController>, TSet<FName>> JoinedGroups;
	/** Shared rather than unique so this header need not know it (Private/Gate). */
	TSharedPtr<FVeyraFogGate> Gate;
	FTimerHandle Timer;
	FDelegateHandle SpawnedHandle;
	FDelegateHandle DeathHandle;
	bool bStarted = false;
};
