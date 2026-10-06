// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Feedback/VeyraCombatTextRules.h"

#include "AbilitySystemComponent.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Targeting/VeyraTargeting.h"
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
	// What the player's own Vanguard took it received, Self-Damage included; what it or its units struck, it dealt.
	const bool bReceived = Target == &Player;
	const bool bDealt = !bReceived && AnswersTo(Source, Player) && VeyraTargeting::AreHostile(Source->GetOwner(), Target->GetOwner());
	if (!bDealt && !bReceived)
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

TOptional<FVeyraCombatTextLine> ForGold(double Amount, EVeyraGoldReason Reason, const FVeyraGoldSource& From, AActor* Own)
{
	switch (Reason)
	{
	// Gold that comes on its own, or back from the shop, is no moment of play.
	case EVeyraGoldReason::Starting:
	case EVeyraGoldReason::Passive:
	case EVeyraGoldReason::Sale:
	case EVeyraGoldReason::Undo:
	case EVeyraGoldReason::Developer:
		return {};
	default:
		break;
	}
	if (!(Amount > 0.0))
	{
		return {};
	}
	FVeyraCombatTextLine Line;
	Line.Kind = EVeyraCombatTextKind::Gold;
	Line.Amount = static_cast<float>(Amount);
	if (From.Where.IsSet())
	{
		Line.Unit = const_cast<AActor*>(From.Unit.Get());
		Line.bFixed = true;
		Line.Where = From.Where.GetValue();
		return Line;
	}
	if (!Own)
	{
		return {};
	}
	Line.Unit = Own;
	return Line;
}
}
