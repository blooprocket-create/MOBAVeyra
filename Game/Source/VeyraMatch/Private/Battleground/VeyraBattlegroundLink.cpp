// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Battleground/VeyraBattlegroundLink.h"

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Layout/VeyraLayout.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Rewards/VeyraRewardSubsystem.h"
#include "Shop/VeyraShopSubsystem.h"
#include "Structures/VeyraStructure.h"
#include "Tuning/VeyraFluxTuningSubsystem.h"
#include "VeyraBattlegroundSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraTeamFluxSubsystem.h"
#include "VeyraVisionSubsystem.h"
#include "Wells/VeyraFluxWellSubsystem.h"
#include "Wildlife/VeyraJungleSubsystem.h"

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
	MatchWorld = &World;
	// Vision governs the match from before its first player joins, so no unit ever reaches a client that
	// may not see it; it runs until the world ends, through the match's end (ADR-016 §2, §3).
	UVeyraVisionSubsystem* Vision = World.GetSubsystem<UVeyraVisionSubsystem>();
	if (Vision)
	{
		Vision->Start();
	}
	Battleground = World.GetSubsystem<UVeyraBattlegroundSubsystem>();
	Flux = World.GetSubsystem<UVeyraTeamFluxSubsystem>();
	Rewards = World.GetSubsystem<UVeyraRewardSubsystem>();
	Jungle = World.GetSubsystem<UVeyraJungleSubsystem>();
	FluxWells = World.GetSubsystem<UVeyraFluxWellSubsystem>();
	if (UVeyraFluxWellSubsystem* Wells = FluxWells.Get())
	{
		SecuredHandle = Wells->OnFluxWellSecured.AddRaw(this, &FVeyraBattlegroundLink::OnFluxWellSecured);
	}
	OnPrimeWellDestroyed = MoveTemp(InOnPrimeWellDestroyed);
	if (UVeyraBattlegroundSubsystem* Subsystem = Battleground.Get())
	{
		DestroyedHandle = Subsystem->OnStructureDestroyed.AddRaw(this, &FVeyraBattlegroundLink::OnStructureDestroyed);
		// World places the battleground's Dense Fog; Vision rules what it hides (ADR-016 §4).
		if (const FVeyraBattlegroundLayout* Layout = Subsystem->GetLayout(); Layout && Vision)
		{
			TArray<FVeyraFogCircle> Circles;
			for (const FVeyraFogPlacement& Placement : VeyraLayout::DenseFog(*Layout))
			{
				Circles.Add(FVeyraFogCircle{ Placement.Center, Placement.Radius });
			}
			Vision->SetDenseFog(MoveTemp(Circles));
		}
	}
	if (UVeyraTeamFluxSubsystem* TeamFlux = Flux.Get())
	{
		FluxChangedHandle = TeamFlux->OnTeamFluxChanged.AddRaw(this, &FVeyraBattlegroundLink::OnTeamFluxChanged);
		// World and the spell slots start from each team's Flux as it stands.
		OnTeamFluxChanged(EVeyraTeam::A);
		OnTeamFluxChanged(EVeyraTeam::B);
	}
}

void FVeyraBattlegroundLink::OnTeamFluxChanged(EVeyraTeam Team)
{
	UVeyraTeamFluxSubsystem* TeamFlux = Flux.Get();
	if (!TeamFlux)
	{
		return;
	}
	// The team's spell slots open as its permanent Flux reaches them, anywhere (Battleground Bible §14).
	if (const AGameStateBase* GameState = MatchWorld.IsValid() ? MatchWorld->GetGameState() : nullptr)
	{
		for (APlayerState* Participant : GameState->PlayerArray)
		{
			if (Participant && VeyraTeams::TeamOf(Participant) == Team)
			{
				UnlockSpellSlots(*Participant);
			}
		}
	}
	UVeyraBattlegroundSubsystem* Subsystem = Battleground.Get();
	if (!Subsystem)
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

void FVeyraBattlegroundLink::UnlockSpellSlots(APlayerState& Participant) const
{
	const UVeyraTeamFluxSubsystem* TeamFlux = Flux.Get();
	UVeyraAbilityLoadoutComponent* Loadout = Participant.FindComponentByClass<UVeyraAbilityLoadoutComponent>();
	const EVeyraTeam Team = VeyraTeams::TeamOf(&Participant);
	if (!TeamFlux || !Loadout || Team == EVeyraTeam::None)
	{
		return;
	}
	Loadout->SetUnlockedSpellSlots(VeyraFlux::UnlockedSpellSlots(TeamFlux->GetPermanent(Team), UVeyraFluxTuningSubsystem::Get().SpellSlots.Thresholds));
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
	if (UVeyraJungleSubsystem* Camps = Jungle.Get())
	{
		Camps->Stop();
	}
	if (UVeyraFluxWellSubsystem* Wells = FluxWells.Get())
	{
		Wells->OnFluxWellSecured.Remove(SecuredHandle);
		Wells->Stop();
	}
	// Nothing is paid once the match ends (Economy & Progression Bible §8.2).
	if (UVeyraRewardSubsystem* Paying = Rewards.Get())
	{
		Paying->Stop();
	}
	Rewards.Reset();
	Jungle.Reset();
	FluxWells.Reset();
	SecuredHandle.Reset();
	DestroyedHandle.Reset();
	FluxChangedHandle.Reset();
	Battleground.Reset();
	Flux.Reset();
	MatchWorld.Reset();
}

void FVeyraBattlegroundLink::StartLive()
{
	if (UVeyraBattlegroundSubsystem* Subsystem = Battleground.Get())
	{
		Subsystem->StartWaves();
	}
	if (UVeyraJungleSubsystem* Camps = Jungle.Get())
	{
		Camps->Start();
	}
	if (UVeyraFluxWellSubsystem* Wells = FluxWells.Get())
	{
		Wells->Start();
	}
}

void FVeyraBattlegroundLink::OnFluxWellSecured(const FVeyraFluxWellSecuredEvent& Event)
{
	// Its side takes the Well's temporary Team Flux (Battleground Bible §6).
	if (UVeyraTeamFluxSubsystem* TeamFlux = Flux.Get())
	{
		TeamFlux->Grant(Event.Team, EVeyraFluxSource::FluxWell);
	}
	// And refills each of its participants' Flux Flasks (Item Bible §10; ADR-022 §6).
	UWorld* World = MatchWorld.Get();
	UVeyraShopSubsystem* Shop = World ? World->GetSubsystem<UVeyraShopSubsystem>() : nullptr;
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (Shop && GameState)
	{
		for (APlayerState* Participant : GameState->PlayerArray)
		{
			if (Participant && VeyraTeams::TeamOf(Participant) == Event.Team)
			{
				Shop->RefillCharges(*Participant);
			}
		}
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
