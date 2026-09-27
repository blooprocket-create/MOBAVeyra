// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Life/VeyraDeath.h"

#include "AbilitySystemComponent.h"
#include "Attribution/VeyraAttributionComponent.h"
#include "CombatState/VeyraCombatStateComponent.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Life/VeyraLifeComponent.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Units/VeyraUnit.h"
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

	// §18: the assisters are read before death clears the victim's records.
	FVeyraDeathEvent Death{ &Victim, Killer };
	if (UVeyraAttributionComponent* Attribution = Owner->FindComponentByClass<UVeyraAttributionComponent>())
	{
		for (UAbilitySystemComponent* Assister : Attribution->GetAssisters(Killer, Owner->GetWorld()->GetTimeSeconds()))
		{
			Death.Assisters.Add(Assister);
		}
		Attribution->Clear();
	}
	// §28: death clears Combat State.
	if (UVeyraCombatStateComponent* CombatState = Owner->FindComponentByClass<UVeyraCombatStateComponent>())
	{
		CombatState->Clear();
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

	// A takedown is a kill or an assist by an enemy Vanguard (§18). It extends the statuses that say
	// so (ADR-009 §1).
	TArray<UAbilitySystemComponent*, TInlineAllocator<5>> Takedown;
	const AActor* KillerUnit = Killer ? Killer->GetOwner() : nullptr;
	if (KillerUnit && VeyraUnits::IsVanguard(KillerUnit) && VeyraTargeting::AreHostile(KillerUnit, Owner))
	{
		Takedown.Add(Killer);
	}
	for (const TWeakObjectPtr<UAbilitySystemComponent>& Assister : Death.Assisters)
	{
		Takedown.Add(Assister.Get());
	}
	for (UAbilitySystemComponent* Participant : Takedown)
	{
		const AActor* ParticipantUnit = Participant ? Participant->GetOwner() : nullptr;
		if (UVeyraStatusComponent* Statuses = ParticipantUnit ? ParticipantUnit->FindComponentByClass<UVeyraStatusComponent>() : nullptr)
		{
			Statuses->ExtendForTakedown();
		}
	}

	UE_LOG(LogVeyraCombat, Log, TEXT("%s died (killed by %s, %d assist(s))."), *GetNameSafe(Owner), *GetNameSafe(KillerUnit), Death.Assisters.Num());
	if (UVeyraCombatEventSubsystem* Events = Owner->GetWorld() ? Owner->GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnDeath.Broadcast(Death);
	}
}
}
