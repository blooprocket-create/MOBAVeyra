// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Feedback/VeyraCombatTextLink.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Feedback/VeyraCombatTextRules.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Targeting/VeyraVisibility.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"

FVeyraCombatTextLink::~FVeyraCombatTextLink()
{
	Stop();
}

void FVeyraCombatTextLink::Start(UWorld& World)
{
	UVeyraCombatEventSubsystem* Subsystem = World.GetSubsystem<UVeyraCombatEventSubsystem>();
	if (!Subsystem)
	{
		return;
	}
	Events = Subsystem;
	DamageHandle = Subsystem->OnDamageDealt.AddRaw(this, &FVeyraCombatTextLink::OnDamageDealt);
	HealingHandle = Subsystem->OnHealthRestored.AddRaw(this, &FVeyraCombatTextLink::OnHealthRestored);
	ShieldHandle = Subsystem->OnShieldGranted.AddRaw(this, &FVeyraCombatTextLink::OnShieldGranted);
}

void FVeyraCombatTextLink::Stop()
{
	if (UVeyraCombatEventSubsystem* Subsystem = Events.Get())
	{
		Subsystem->OnDamageDealt.Remove(DamageHandle);
		Subsystem->OnHealthRestored.Remove(HealingHandle);
		Subsystem->OnShieldGranted.Remove(ShieldHandle);
	}
	Events.Reset();
}

void FVeyraCombatTextLink::OnDamageDealt(const FVeyraDamageDealtEvent& Event)
{
	// The player its dealer answers to, whatever unit struck for it, and the player it struck.
	UAbilitySystemComponent* Dealer = VeyraCombat::ResponsibleFor(Event.Source.Get());
	UAbilitySystemComponent* Receiver = Event.Target.Get();
	for (UAbilitySystemComponent* Player : { Dealer, Receiver != Dealer ? Receiver : nullptr })
	{
		if (Player)
		{
			for (const FVeyraCombatTextLine& Line : VeyraCombatTextRouting::ForDamage(Event, *Player))
			{
				Send(Player, Line);
			}
		}
	}
}

void FVeyraCombatTextLink::OnHealthRestored(const FVeyraHealthRestored& Event)
{
	UAbilitySystemComponent* Healer = VeyraCombat::ResponsibleFor(Event.Provider.Get());
	UAbilitySystemComponent* Healed = Event.Target.Get();
	for (UAbilitySystemComponent* Player : { Healer, Healed != Healer ? Healed : nullptr })
	{
		if (const TOptional<FVeyraCombatTextLine> Line = Player ? VeyraCombatTextRouting::ForHealing(Event, *Player) : TOptional<FVeyraCombatTextLine>())
		{
			Send(Player, Line.GetValue());
		}
	}
}

void FVeyraCombatTextLink::OnShieldGranted(const FVeyraShieldGranted& Event)
{
	UAbilitySystemComponent* Granter = VeyraCombat::ResponsibleFor(Event.Provider.Get());
	UAbilitySystemComponent* Shielded = Event.Target.Get();
	for (UAbilitySystemComponent* Player : { Granter, Shielded != Granter ? Shielded : nullptr })
	{
		if (const TOptional<FVeyraCombatTextLine> Line = Player ? VeyraCombatTextRouting::ForShield(Event, *Player) : TOptional<FVeyraCombatTextLine>())
		{
			Send(Player, Line.GetValue());
		}
	}
}

void FVeyraCombatTextLink::Send(UAbilitySystemComponent* Player, FVeyraCombatTextLine Line) const
{
	const AVeyraPlayerState* Participant = Player ? Cast<AVeyraPlayerState>(Player->GetOwner()) : nullptr;
	AVeyraPlayerController* Controller = Participant ? Cast<AVeyraPlayerController>(Participant->GetPlayerController()) : nullptr;
	if (!Controller || !Line.Unit)
	{
		return;
	}
	// Combat text never reveals a unit in the fog: its unit must be seen, and the other end travels only while it is.
	const EVeyraTeam Team = Participant->GetVeyraTeam();
	if (!VeyraVisibility::IsVisibleToTeam(Team, *Line.Unit))
	{
		return;
	}
	if (Line.Other && !VeyraVisibility::IsVisibleToTeam(Team, *Line.Other))
	{
		Line.Other = nullptr;
	}
	Controller->ClientCombatText(Line);
}
