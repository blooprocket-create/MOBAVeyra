// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Battleground/VeyraBattlegroundLink.h"

#include "Engine/World.h"
#include "VeyraBattlegroundSubsystem.h"
#include "VeyraTeamFluxSubsystem.h"

namespace
{
	/** The Team Flux source a destroyed structure is (Battleground Bible §5, §10, §18); none for the Prime Well. */
	TOptional<EVeyraFluxSource> FluxSourceOf(EVeyraStructureKind Kind)
	{
		switch (Kind)
		{
		case EVeyraStructureKind::LaneSpire:
			return EVeyraFluxSource::LaneSpire;
		case EVeyraStructureKind::BaseTower:
			return EVeyraFluxSource::BaseTower;
		case EVeyraStructureKind::Inhibitor:
			return EVeyraFluxSource::Inhibitor;
		case EVeyraStructureKind::PrimeWell:
			return {};
		}
		return {};
	}
}

FVeyraBattlegroundLink::~FVeyraBattlegroundLink()
{
	Stop();
}

void FVeyraBattlegroundLink::Start(UWorld& World, FOnPrimeWellDestroyed InOnPrimeWellDestroyed)
{
	Battleground = World.GetSubsystem<UVeyraBattlegroundSubsystem>();
	Flux = World.GetSubsystem<UVeyraTeamFluxSubsystem>();
	OnPrimeWellDestroyed = MoveTemp(InOnPrimeWellDestroyed);
	if (UVeyraBattlegroundSubsystem* Subsystem = Battleground.Get())
	{
		DestroyedHandle = Subsystem->OnStructureDestroyed.AddRaw(this, &FVeyraBattlegroundLink::OnStructureDestroyed);
	}
}

void FVeyraBattlegroundLink::Stop()
{
	if (UVeyraBattlegroundSubsystem* Subsystem = Battleground.Get())
	{
		Subsystem->OnStructureDestroyed.Remove(DestroyedHandle);
		Subsystem->Stop();
	}
	DestroyedHandle.Reset();
	Battleground.Reset();
	Flux.Reset();
}

void FVeyraBattlegroundLink::OnStructureDestroyed(const FVeyraStructureDestroyedEvent& Event)
{
	const EVeyraTeam Destroyers = VeyraTeams::Opposing(Event.Team);
	if (const TOptional<EVeyraFluxSource> Source = FluxSourceOf(Event.Kind); Source.IsSet())
	{
		if (UVeyraTeamFluxSubsystem* TeamFlux = Flux.Get())
		{
			TeamFlux->Grant(Destroyers, Source.GetValue());
		}
	}
	else
	{
		OnPrimeWellDestroyed.ExecuteIfBound(Destroyers);
	}
}
