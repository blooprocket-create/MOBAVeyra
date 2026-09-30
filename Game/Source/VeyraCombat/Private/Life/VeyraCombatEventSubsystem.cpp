// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Life/VeyraCombatEventSubsystem.h"

#include "AbilitySystemComponent.h"

void UVeyraCombatEventSubsystem::ResolveDamage(const FVeyraDamageResolution& Resolution)
{
	OnDamageResolved.Broadcast(Resolution);
	if (Dealing.IsEmpty() || Dealing.Last().Target != Resolution.Target || Dealing.Last().Source != Resolution.Source)
	{
		return;
	}
	double Cost = Resolution.HealthLost + Resolution.TemporaryHealthSpent;
	for (const FVeyraShieldShare& Share : Resolution.Shields)
	{
		Cost += Share.Absorbed;
	}
	FVeyraDamageComponents& Dealt = Dealing.Last().Dealt;
	if (FVeyraDamageComponent* Same = Dealt.FindByPredicate([&Resolution](const FVeyraDamageComponent& Each) { return Each.Type == Resolution.Type; }))
	{
		Same->Amount += Cost;
	}
	else
	{
		Dealt.Add({ Resolution.Type, Cost });
	}
}

void UVeyraCombatEventSubsystem::BeginDealing(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, EVeyraDamageDelivery Delivery)
{
	FVeyraDamageDealtEvent& Instance = Dealing.AddDefaulted_GetRef();
	Instance.Source = &Source;
	Instance.Target = &Target;
	Instance.Delivery = Delivery;
}

FVeyraDamageDealtEvent UVeyraCombatEventSubsystem::EndDealing()
{
	return Dealing.IsEmpty() ? FVeyraDamageDealtEvent() : Dealing.Pop(EAllowShrinking::No);
}
