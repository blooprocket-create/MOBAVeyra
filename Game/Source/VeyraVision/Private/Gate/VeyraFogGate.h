// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Iris/ReplicationSystem/NetObjectGroupHandle.h"
#include "Iris/ReplicationSystem/NetRefHandle.h"
#include "Teams/VeyraTeam.h"
#include "UObject/WeakObjectPtr.h"

class AActor;
class APlayerController;
class UReplicationSystem;
class UWorld;

/**
 * The fog gate on Iris (ADR-006 §5, made real by ADR-016 §3). Every gated unit's class is hidden
 * from every client by the engine's filter-out filter (DefaultEngine.ini). Inclusion groups open it
 * up: one per side, allowed for that side's players, holding the side's own units; and one per
 * player, allowed for that player alone, holding the enemy and neutral units that player sees. A
 * unit leaving a player's group is removed from that player's client. Deny by default: a unit the
 * gate never heard of reaches nobody.
 *
 * Server only; does nothing where the world replicates without Iris, or not at all.
 */
class FVeyraFogGate
{
public:
	~FVeyraFogGate();

	/** Starts gating World's replication. False where it has no Iris replication system. */
	bool Start(UWorld& World);

	/** Removes the gate's groups; everything it opened closes. */
	void Stop();

	bool IsStarted() const { return System != nullptr; }

	/**
	 * Puts Unit in Team's group, so its own side receives it. False while it has not begun
	 * replicating; call again later. A unit already in its group is left alone.
	 */
	bool AddToSide(const AActor& Unit, EVeyraTeam Team);

	/**
	 * Makes Controller's player receive exactly Seen, besides their side's own units: what joined Seen
	 * since the last call is added to their group, and what left it removed. Creates the player's
	 * group, and allows their side's group, the first time.
	 */
	void SyncPlayer(const APlayerController& Controller, EVeyraTeam Team, const TSet<const AActor*>& Seen);

	/** Forgets players whose controllers are gone, and their groups. */
	void ForgetPlayersExcept(const TSet<const APlayerController*>& Present);

private:
	struct FPlayerGroup
	{
		UE::Net::FNetObjectGroupHandle Group;
		uint32 Connection = 0;
		EVeyraTeam Team = EVeyraTeam::None;
		TSet<UE::Net::FNetRefHandle> Members;
	};

	UE::Net::FNetRefHandle HandleOf(const AActor& Actor) const;

	UReplicationSystem* System = nullptr;
	TMap<EVeyraTeam, UE::Net::FNetObjectGroupHandle> SideGroups;
	TSet<UE::Net::FNetRefHandle> InSideGroups;
	TMap<TWeakObjectPtr<const APlayerController>, FPlayerGroup> Players;
};
