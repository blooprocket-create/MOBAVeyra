// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Battleground/VeyraBattlegroundLink.h"

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Engine/World.h"
#include "Rewards/VeyraRewardSubsystem.h"
#include "Structures/VeyraStructure.h"
#include "VeyraBattlegroundSubsystem.h"
#include "VeyraCombatVerbs.h"
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
	Rewards = World.GetSubsystem<UVeyraRewardSubsystem>();
	OnPrimeWellDestroyed = MoveTemp(InOnPrimeWellDestroyed);
	if (UVeyraBattlegroundSubsystem* Subsystem = Battleground.Get())
	{
		DestroyedHandle = Subsystem->OnStructureDestroyed.AddRaw(this, &FVeyraBattlegroundLink::OnStructureDestroyed);
	}
	if (UVeyraTeamFluxSubsystem* TeamFlux = Flux.Get())
	{
		FluxChangedHandle = TeamFlux->OnTeamFluxChanged.AddRaw(this, &FVeyraBattlegroundLink::OnTeamFluxChanged);
		// World starts from each team's Flux as it stands.
		OnTeamFluxChanged(EVeyraTeam::A);
		OnTeamFluxChanged(EVeyraTeam::B);
	}
}

void FVeyraBattlegroundLink::OnTeamFluxChanged(EVeyraTeam Team)
{
	UVeyraTeamFluxSubsystem* TeamFlux = Flux.Get();
	UVeyraBattlegroundSubsystem* Subsystem = Battleground.Get();
	if (!TeamFlux || !Subsystem)
	{
		return;
	}
	const FVeyraFluxbornStrength Strength = TeamFlux->GetFluxbornStrength(Team);
	FVeyraTeamFluxStrength View;
	View.ActiveFlux = TeamFlux->GetActive(Team);
	View.HealthMultiplier = Strength.HealthMultiplier;
	View.DamageMultiplier = Strength.DamageMultiplier;
	Subsystem->SetTeamFlux(Team, View);
}

void FVeyraBattlegroundLink::Stop()
{
	if (UVeyraBattlegroundSubsystem* Subsystem = Battleground.Get())
	{
		Subsystem->OnStructureDestroyed.Remove(DestroyedHandle);
		Subsystem->Stop();
	}
	if (UVeyraTeamFluxSubsystem* TeamFlux = Flux.Get())
	{
		TeamFlux->OnTeamFluxChanged.Remove(FluxChangedHandle);
	}
	// Nothing is paid once the match ends (Economy & Progression Bible §8.2).
	if (UVeyraRewardSubsystem* Paying = Rewards.Get())
	{
		Paying->Stop();
	}
	Rewards.Reset();
	DestroyedHandle.Reset();
	FluxChangedHandle.Reset();
	Battleground.Reset();
	Flux.Reset();
}

void FVeyraBattlegroundLink::StartWaves()
{
	if (UVeyraBattlegroundSubsystem* Subsystem = Battleground.Get())
	{
		Subsystem->StartWaves();
	}
}

bool FVeyraBattlegroundLink::DeveloperSiege(UAbilitySystemComponent& Source, EVeyraTeam Team)
{
	UVeyraBattlegroundSubsystem* Subsystem = Battleground.Get();
	AVeyraStructure* Target = Subsystem ? Subsystem->NextSiegeTarget(VeyraTeams::Opposing(Team)) : nullptr;
	if (!Target)
	{
		return false;
	}
	// Its whole Health as True damage: nothing mitigates it, so it is lethal.
	UAbilitySystemComponent& Structure = *Target->GetAbilitySystemComponent();
	FVeyraRawDamageEvent Damage;
	Damage.Components.Add({ EVeyraDamageType::TrueDamage, Structure.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) });
	Damage.Delivery = EVeyraDamageDelivery::Developer;
	VeyraCombat::DealDamage(Source, Structure, Damage);
	return Target->IsDestroyed();
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
