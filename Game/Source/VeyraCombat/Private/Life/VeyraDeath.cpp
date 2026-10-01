// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Life/VeyraDeath.h"

#include "AbilitySystemComponent.h"
#include "Attribution/VeyraAttributionComponent.h"
#include "CombatState/VeyraCombatStateComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Life/VeyraKillCredit.h"
#include "Life/VeyraLifeComponent.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
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

	// §18: credit and assists are read before death clears the victim's records.
	const double Now = Owner->GetWorld()->GetTimeSeconds();
	FVeyraDeathEvent Death;
	Death.Victim = &Victim;
	Death.Killer = Killer;
	Death.DiedAtSeconds = Now;
	// A unit's body is a pawn; a participant without one has none.
	if (const APawn* Body = Cast<APawn>(Victim.GetAvatarActor()))
	{
		Death.Location = Body->GetActorLocation();
	}
	const AActor* KillerUnit = Killer ? Killer->GetOwner() : nullptr;
	const bool bKillerIsEnemyVanguard = KillerUnit && VeyraUnits::IsVanguard(KillerUnit) && VeyraTargeting::AreHostile(KillerUnit, Owner);
	if (UVeyraAttributionComponent* Attribution = Owner->FindComponentByClass<UVeyraAttributionComponent>())
	{
		Death.Contributions = Attribution->GetContributions();
		UAbilitySystemComponent* Credited = VeyraKillCredit::Resolve(Killer, bKillerIsEnemyVanguard, Death.Contributions, Now,
			UVeyraCombatTuningSubsystem::Get().KillCredit.WindowSeconds);
		Death.CreditedKiller = Credited;
		// Kills and assists are Vanguard terms (§18): other victims have contributors, not assisters.
		if (VeyraUnits::IsVanguard(Owner))
		{
			for (UAbilitySystemComponent* Assister : Attribution->GetAssisters(Credited, Now))
			{
				Death.Assisters.Add(Assister);
			}
		}
		Attribution->Clear();
	}
	else if (bKillerIsEnemyVanguard)
	{
		Death.CreditedKiller = Killer;
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

	// A takedown is a kill or an assist on an enemy Vanguard (§18; ADR-011 §6). It extends the
	// statuses that say so (ADR-009 §1); killing a Fluxborn or a structure is no takedown.
	for (UAbilitySystemComponent* Participant : VeyraKillCredit::TakedownParticipants(Death))
	{
		const AActor* ParticipantUnit = Participant ? Participant->GetOwner() : nullptr;
		if (UVeyraStatusComponent* Statuses = ParticipantUnit ? ParticipantUnit->FindComponentByClass<UVeyraStatusComponent>() : nullptr)
		{
			Statuses->ExtendForTakedown();
		}
	}

	const UAbilitySystemComponent* CreditedKiller = Death.CreditedKiller.Get();
	UE_LOG(LogVeyraCombat, Log, TEXT("%s died (killed by %s, credited to %s, %d assist(s))."), *GetNameSafe(Owner), *GetNameSafe(KillerUnit),
		*GetNameSafe(CreditedKiller ? CreditedKiller->GetOwner() : nullptr), Death.Assisters.Num());
	if (UVeyraCombatEventSubsystem* Events = Owner->GetWorld() ? Owner->GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnDeath.Broadcast(Death);
	}
}
}
