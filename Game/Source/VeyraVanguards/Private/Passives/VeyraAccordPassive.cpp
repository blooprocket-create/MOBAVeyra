// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Passives/VeyraAccordPassive.h"

#include "AbilitySystemComponent.h"
#include "Abilities/VeyraGameplayAbility.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Companions/VeyraCompanion.h"
#include "Companions/VeyraCompanionSubsystem.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraAbilitiesVerbs.h"
#include "VeyraCombatVerbs.h"
#include "VeyraVanguardsLog.h"

void UVeyraAccordPassive::Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId)
{
	Super::Start(Owner, InPassiveId);
	UWorld* World = GetWorld();
	const FVeyraAccordTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindAccord(InPassiveId);
	// Its companion forms beside it once it has a body (ADR-034 §3).
	if (UVeyraCompanionSubsystem* Keeper = World ? World->GetSubsystem<UVeyraCompanionSubsystem>() : nullptr; Keeper && Tuning)
	{
		Keeper->Summon(Owner, Tuning->Companion);
	}
	if (UVeyraCombatEventSubsystem* Events = World ? World->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		DealtHandle = Events->OnDamageDealt.AddUObject(this, &UVeyraAccordPassive::OnDamageDealt);
	}
}

void UVeyraAccordPassive::Stop()
{
	if (UVeyraCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnDamageDealt.Remove(DealtHandle);
	}
	DealtHandle.Reset();
	Bound.Reset();
	Super::Stop();
}

void UVeyraAccordPassive::OnDamageDealt(const FVeyraDamageDealtEvent& Event)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	UAbilitySystemComponent* Source = Event.Source.Get();
	UAbilitySystemComponent* Target = Event.Target.Get();
	const FVeyraAccordTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindAccord(PassiveId);
	UWorld* World = GetWorld();
	if (bStriking || !Owner || !Source || !Target || !Tuning || !World || VeyraUnits::IsStructure(Target->GetAvatarActor()))
	{
		return;
	}
	const UVeyraCompanionSubsystem* Keeper = World->GetSubsystem<UVeyraCompanionSubsystem>();
	const AVeyraCompanion* Companion = Keeper ? Keeper->Find(*Owner) : nullptr;
	UAbilitySystemComponent* CompanionAbilities = Companion ? Companion->GetAbilitySystemComponent() : nullptr;
	const bool bByOwner = Source == Owner;
	const bool bByCompanion = CompanionAbilities && Source == CompanionAbilities;
	if (!bByOwner && !bByCompanion)
	{
		return;
	}
	// The companion's bite marks what it hits, as Witchfire reacts to (Roster Bible §10).
	if (bByCompanion)
	{
		if (const TOptional<FVeyraStatusSpec> Mark = UVeyraAbilitiesTuningSubsystem::FindStatus(Tuning->Mark))
		{
			VeyraCombat::ApplyStatus(*CompanionAbilities, *Target, Mark.GetValue());
		}
	}
	const double Now = World->GetTimeSeconds();
	// Records of the long-forgotten go: their times are out of the window and their cooldowns over.
	for (auto It = Bound.CreateIterator(); It; ++It)
	{
		const FBound& Each = It.Value();
		const bool bStale = Now >= Each.ReadyAt && (!Each.OwnerAt.IsSet() || Now - Each.OwnerAt.GetValue() > Tuning->WindowSeconds)
			&& (!Each.CompanionAt.IsSet() || Now - Each.CompanionAt.GetValue() > Tuning->WindowSeconds);
		if (!It.Key().IsValid() || bStale)
		{
			It.RemoveCurrent();
		}
	}
	FBound& Record = Bound.FindOrAdd(Target);
	(bByOwner ? Record.OwnerAt : Record.CompanionAt) = Now;
	const bool bBoth = Record.OwnerAt.IsSet() && Record.CompanionAt.IsSet() && Now - Record.OwnerAt.GetValue() <= Tuning->WindowSeconds
		&& Now - Record.CompanionAt.GetValue() <= Tuning->WindowSeconds;
	if (!bBoth || Now < Record.ReadyAt || !VeyraTargeting::IsAlive(Target->GetAvatarActor()))
	{
		return;
	}
	// Accord: magic from its owner, stronger while the owner holds the boost; a proc, which starts nothing new.
	Record.ReadyAt = Now + Tuning->PerTargetSeconds;
	Record.OwnerAt.Reset();
	Record.CompanionAt.Reset();
	const bool bBoosted = VeyraCombat::HasStatusFrom(Owner->GetAvatarActor(), Tuning->BoostStatus, *Owner);
	const double Amount = (VeyraAbilityRules::AtLevel(Tuning->DamageAmount, Tuning->DamagePerLevel, UVeyraGameplayAbility::GetCasterLevel(*Owner))
		+ Owner->GetNumericAttribute(UVeyraOffenceSet::GetMagicPowerAttribute()) * Tuning->MagicPowerRatio) * (bBoosted ? Tuning->BoostMultiplier : 1.0);
	FVeyraRawDamageEvent Accord;
	Accord.Components.Add({ EVeyraDamageType::Magic, Amount });
	Accord.Delivery = EVeyraDamageDelivery::Proc;
	bStriking = true;
	VeyraCombat::DealDamage(*Owner, *Target, Accord);
	bStriking = false;
	++AccordCount;
	VeyraAbilities::ShortenCooldown(*Owner, Tuning->RefundSlot, Tuning->RefundSeconds);
	UE_LOG(LogVeyraVanguards, Verbose, TEXT("%s's Accord strikes %s for %g."), *GetNameSafe(Owner->GetOwner()), *GetNameSafe(Target->GetAvatarActor()), Amount);
}
