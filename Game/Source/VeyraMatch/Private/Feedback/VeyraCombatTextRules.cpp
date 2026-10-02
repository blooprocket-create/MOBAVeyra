// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Feedback/VeyraCombatTextRules.h"

#include "AbilitySystemComponent.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "VeyraCombatVerbs.h"

namespace VeyraCombatTextRouting
{
namespace
{
	/** Whether Unit's outcome is Player's: Player itself, or a unit it owns, such as its companion or its Echo. */
	bool AnswersTo(UAbilitySystemComponent* Unit, const UAbilitySystemComponent& Player)
	{
		return Unit && VeyraCombat::ResponsibleFor(Unit) == &Player;
	}

	FVeyraCombatTextLine LineOf(EVeyraCombatTextKind Kind, UAbilitySystemComponent& At, UAbilitySystemComponent* Other, double Amount)
	{
		FVeyraCombatTextLine Line;
		Line.Kind = Kind;
		Line.Unit = At.GetAvatarActor();
		Line.Other = Other ? Other->GetAvatarActor() : nullptr;
		Line.Amount = static_cast<float>(Amount);
		return Line;
	}
}

TArray<FVeyraCombatTextLine> ForDamage(const FVeyraDamageDealtEvent& Event, const UAbilitySystemComponent& Player)
{
	TArray<FVeyraCombatTextLine> Lines;
	UAbilitySystemComponent* Source = Event.Source.Get();
	UAbilitySystemComponent* Target = Event.Target.Get();
	if (!Source || !Target)
	{
		return Lines;
	}
	const bool bDealt = AnswersTo(Source, Player);
	if (!bDealt && Target != &Player)
	{
		return Lines;
	}
	for (const FVeyraDamageComponent& Component : Event.Dealt)
	{
		if (Component.Amount > 0.0)
		{
			FVeyraCombatTextLine& Line = Lines.Add_GetRef(LineOf(bDealt ? EVeyraCombatTextKind::DamageDealt : EVeyraCombatTextKind::DamageReceived, *Target, Source, Component.Amount));
			Line.DamageType = Component.Type;
			Line.bCritical = Event.bCritical;
		}
	}
	return Lines;
}

TOptional<FVeyraCombatTextLine> ForHealing(const FVeyraHealthRestored& Event, const UAbilitySystemComponent& Player)
{
	UAbilitySystemComponent* Provider = Event.Provider.Get();
	UAbilitySystemComponent* Target = Event.Target.Get();
	// Only a unit's healing: regeneration, the fountain and a Well heal no one in particular (ADR-052 §7).
	if (!Provider || !Target || !(Event.Restored > 0.0) || (!AnswersTo(Provider, Player) && Target != &Player))
	{
		return {};
	}
	return LineOf(EVeyraCombatTextKind::Healing, *Target, Provider, Event.Restored);
}

TOptional<FVeyraCombatTextLine> ForShield(const FVeyraShieldGranted& Event, const UAbilitySystemComponent& Player)
{
	UAbilitySystemComponent* Provider = Event.Provider.Get();
	UAbilitySystemComponent* Target = Event.Target.Get();
	if (!Provider || !Target || !(Event.Added > 0.0) || (!AnswersTo(Provider, Player) && Target != &Player))
	{
		return {};
	}
	return LineOf(EVeyraCombatTextKind::Shielding, *Target, Provider, Event.Added);
}
}
