// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Gate/VeyraFogGate.h"

#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Iris/ReplicationSystem/Filtering/NetObjectFilter.h"
#include "Iris/ReplicationSystem/ObjectReplicationBridge.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Net/Iris/ReplicationSystem/ReplicationSystemUtil.h"
#include "VeyraVisionLog.h"

namespace
{
	const TCHAR* SideGroupName(EVeyraTeam Team)
	{
		return Team == EVeyraTeam::A ? TEXT("Veyra.Side.A") : TEXT("Veyra.Side.B");
	}

	/** The connection a player's client is on, or 0 for a controller with none (a bot, a local player). */
	uint32 ConnectionOf(const APlayerController& Controller)
	{
		const UNetConnection* Connection = Controller.GetNetConnection();
		return Connection ? Connection->GetConnectionHandle().GetParentConnectionId() : 0;
	}
}

FVeyraFogGate::~FVeyraFogGate()
{
	Stop();
}

bool FVeyraFogGate::Start(UWorld& World)
{
	Stop();
	UReplicationSystem* Found = UE::Net::FReplicationSystemUtil::GetReplicationSystem(&World);
	if (!Found)
	{
		return false;
	}
	System = Found;
	for (const EVeyraTeam Side : { EVeyraTeam::A, EVeyraTeam::B })
	{
		const UE::Net::FNetObjectGroupHandle Group = System->CreateGroup(SideGroupName(Side));
		System->AddInclusionFilterGroup(Group);
		SideGroups.Add(Side, Group);
	}
	UE_LOG(LogVeyraVision, Log, TEXT("The fog gate is up: a client receives its side's units and what its player sees."));
	return true;
}

void FVeyraFogGate::Stop()
{
	if (!System)
	{
		return;
	}
	for (const TPair<TWeakObjectPtr<const APlayerController>, FPlayerGroup>& Player : Players)
	{
		System->DestroyGroup(Player.Value.Group);
	}
	for (const TPair<EVeyraTeam, UE::Net::FNetObjectGroupHandle>& Side : SideGroups)
	{
		System->DestroyGroup(Side.Value);
	}
	Players.Reset();
	SideGroups.Reset();
	InSideGroups.Reset();
	System = nullptr;
}

UE::Net::FNetRefHandle FVeyraFogGate::HandleOf(const AActor& Actor) const
{
	return System && System->GetReplicationBridge() ? System->GetReplicationBridge()->GetReplicatedRefHandle(&Actor) : UE::Net::FNetRefHandle();
}

bool FVeyraFogGate::AddToSide(const AActor& Unit, EVeyraTeam Team)
{
	const UE::Net::FNetObjectGroupHandle* Group = SideGroups.Find(Team);
	const UE::Net::FNetRefHandle Handle = HandleOf(Unit);
	if (!Group || !Handle.IsValid())
	{
		return false;
	}
	bool bAlreadyIn = false;
	InSideGroups.Add(Handle, &bAlreadyIn);
	if (!bAlreadyIn)
	{
		System->AddToGroup(*Group, Handle);
	}
	return true;
}

void FVeyraFogGate::SyncPlayer(const APlayerController& Controller, EVeyraTeam Team, const TSet<const AActor*>& Seen)
{
	const uint32 Connection = ConnectionOf(Controller);
	if (!System || Connection == 0 || !SideGroups.Contains(Team))
	{
		return;
	}
	FPlayerGroup* Player = Players.Find(&Controller);
	if (!Player || Player->Team != Team)
	{
		if (Player)
		{
			System->DestroyGroup(Player->Group);
		}
		// A player who reconnects at once may get back the connection its old controller had, before a
		// pass forgot that one: its group would still let what that player saw through, under this name.
		for (auto It = Players.CreateIterator(); It; ++It)
		{
			if (It.Key() != &Controller && It.Value().Connection == Connection)
			{
				System->DestroyGroup(It.Value().Group);
				It.RemoveCurrent();
			}
		}
		Player = &Players.Add(&Controller);
		Player->Connection = Connection;
		Player->Team = Team;
		Player->Group = System->CreateGroup(FName(*FString::Printf(TEXT("Veyra.Sightings.%u"), Connection)));
		System->AddInclusionFilterGroup(Player->Group);
		System->SetGroupFilterStatus(Player->Group, Connection, UE::Net::ENetFilterStatus::Allow);
		for (const TPair<EVeyraTeam, UE::Net::FNetObjectGroupHandle>& Side : SideGroups)
		{
			System->SetGroupFilterStatus(Side.Value, Connection, Side.Key == Team ? UE::Net::ENetFilterStatus::Allow : UE::Net::ENetFilterStatus::Disallow);
		}
	}

	TSet<UE::Net::FNetRefHandle> Wanted;
	for (const AActor* Unit : Seen)
	{
		if (const UE::Net::FNetRefHandle Handle = Unit ? HandleOf(*Unit) : UE::Net::FNetRefHandle(); Handle.IsValid())
		{
			Wanted.Add(Handle);
		}
	}
	for (auto It = Player->Members.CreateIterator(); It; ++It)
	{
		if (!Wanted.Contains(*It))
		{
			System->RemoveFromGroup(Player->Group, *It);
			It.RemoveCurrent();
		}
	}
	for (const UE::Net::FNetRefHandle Handle : Wanted)
	{
		bool bAlreadyIn = false;
		Player->Members.Add(Handle, &bAlreadyIn);
		if (!bAlreadyIn)
		{
			System->AddToGroup(Player->Group, Handle);
		}
	}
}

void FVeyraFogGate::ForgetPlayersExcept(const TSet<const APlayerController*>& Present)
{
	for (auto It = Players.CreateIterator(); It; ++It)
	{
		const APlayerController* Controller = It.Key().Get();
		if (!Controller || !Present.Contains(Controller))
		{
			if (System)
			{
				System->DestroyGroup(It.Value().Group);
			}
			It.RemoveCurrent();
		}
	}
}
