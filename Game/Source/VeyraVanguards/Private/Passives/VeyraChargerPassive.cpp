// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Passives/VeyraChargerPassive.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraCombatVerbs.h"
#include "VeyraVanguardsLog.h"

void UVeyraChargerPassive::Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId)
{
	Super::Start(Owner, InPassiveId);
	if (UVeyraCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		DeathHandle = Events->OnDeath.AddUObject(this, &UVeyraChargerPassive::OnDeath);
	}
}

void UVeyraChargerPassive::Stop()
{
	if (UVeyraCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnDeath.Remove(DeathHandle);
	}
	DeathHandle.Reset();
	Super::Stop();
}

void UVeyraChargerPassive::OnDeath(const FVeyraDeathEvent& Death)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const UAbilitySystemComponent* Victim = Death.Victim.Get();
	const AActor* Fallen = Victim ? Victim->GetAvatarActor() : nullptr;
	const AActor* Body = Owner ? Owner->GetAvatarActor() : nullptr;
	const FVeyraChargerTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindCharger(PassiveId);
	// Any side's Fluxborn, never wildlife, a Vanguard or a structure (Roster Bible §4).
	if (!Tuning || !Body || !Fallen || VeyraUnits::KindOf(Fallen) != EVeyraUnitKind::Fluxborn || !VeyraTargeting::IsAlive(Body))
	{
		return;
	}
	const FVector Where = Death.Location.IsSet() ? Death.Location.GetValue() : Fallen->GetActorLocation();
	if (FVector::Dist2D(Where, Body->GetActorLocation()) > Tuning->Radius)
	{
		return;
	}
	// While its owner holds the boost, as Overcharge gives it, each death gives more.
	const UVeyraStatusComponent* Statuses = Owner->GetOwner() ? Owner->GetOwner()->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
	const bool bBoosted = Statuses && Statuses->GetLedger().Entries.ContainsByPredicate([Tuning](const FVeyraStatusEntry& Entry) { return Entry.Id == Tuning->BoostStatus; });
	const double Multiplier = bBoosted ? Tuning->BoostMultiplier : 1.0;
	VeyraCombat::RestoreResource(*Owner, Tuning->ChargePerDeath * Multiplier);
	UE_LOG(LogVeyraVanguards, Verbose, TEXT("%s's %s gains %g Charge."), *GetNameSafe(Owner->GetOwner()), *PassiveId.ToString(), Tuning->ChargePerDeath * Multiplier);
}
