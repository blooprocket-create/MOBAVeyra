// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Life/VeyraDeath.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Life/VeyraLifeComponent.h"
#include "VeyraCombatLog.h"

namespace VeyraDeath
{
void FinalizeDeath(UAbilitySystemComponent& Victim, UAbilitySystemComponent* Killer)
{
	AActor* Owner = Victim.GetOwner();
	UVeyraLifeComponent* Life = Owner ? Owner->FindComponentByClass<UVeyraLifeComponent>() : nullptr;
	if (!Life)
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("%s reached 0 Health but has no UVeyraLifeComponent, so its death was not recorded."), *GetNameSafe(Owner));
		return;
	}
	if (!Life->SetState(EVeyraLifeState::Dead))
	{
		return;
	}

	// §44: temporary effects end at death. Effects with a duration are temporary; infinite ones are
	// permanent progression and stay.
	TArray<FActiveGameplayEffectHandle> Temporary;
	for (const FActiveGameplayEffectHandle& Handle : Victim.GetActiveEffects(FGameplayEffectQuery()))
	{
		const FActiveGameplayEffect* Active = Victim.GetActiveGameplayEffect(Handle);
		if (Active && Active->Spec.Def && Active->Spec.Def->DurationPolicy == EGameplayEffectDurationType::HasDuration)
		{
			Temporary.Add(Handle);
		}
	}
	for (const FActiveGameplayEffectHandle& Handle : Temporary)
	{
		Victim.RemoveActiveGameplayEffect(Handle);
	}

	UE_LOG(LogVeyraCombat, Log, TEXT("%s died (killed by %s)."), *GetNameSafe(Owner), *GetNameSafe(Killer ? Killer->GetOwner() : nullptr));
	if (UVeyraCombatEventSubsystem* Events = Owner->GetWorld() ? Owner->GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnDeath.Broadcast(FVeyraDeathEvent{ &Victim, Killer });
	}
}
}
