// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraSelfBuffAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Engine/World.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Shapes/VeyraShapes.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Teams/VeyraTeam.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraCombatVerbs.h"

bool UVeyraSelfBuffAbility::Defines(const FVeyraContentId& Ability) const
{
	return UVeyraAbilitiesTuningSubsystem::FindSelfBuff(Ability) != nullptr;
}

double UVeyraSelfBuffAbility::GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraSelfBuffAbilityTuning* Buff = UVeyraAbilitiesTuningSubsystem::FindSelfBuff(Ability);
	return Buff ? VeyraAbilityRules::ValueAtRank(Buff->Cast.ResourceCostByRank, Rank) : 0.0;
}

double UVeyraSelfBuffAbility::GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraSelfBuffAbilityTuning* Buff = UVeyraAbilitiesTuningSubsystem::FindSelfBuff(Ability);
	return Buff ? VeyraAbilityRules::ValueAtRank(Buff->Cast.CooldownSecondsByRank, Rank) : 0.0;
}

const FVeyraCastTuning* UVeyraSelfBuffAbility::GetCastTuning(const FVeyraContentId& Ability) const
{
	const FVeyraSelfBuffAbilityTuning* Buff = UVeyraAbilitiesTuningSubsystem::FindSelfBuff(Ability);
	return Buff ? &Buff->Cast : nullptr;
}

bool UVeyraSelfBuffAbility::EndsEarlyOnRecast(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability) const
{
	const FVeyraSelfBuffAbilityTuning* Buff = UVeyraAbilitiesTuningSubsystem::FindSelfBuff(Ability);
	if (!Buff || Buff->Recast != EVeyraRecast::EndsEarly)
	{
		return false;
	}
	if (IsAuraRunning() && AuraCaster.Get() == &Caster)
	{
		return true;
	}
	// Any form of its stance standing counts.
	const AActor* Owner = Caster.GetOwner();
	const UVeyraStatusComponent* Statuses = Owner ? Owner->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
	for (const FVeyraContentId& Form : FormsOf(Caster, Ability))
	{
		const FVeyraSelfBuffAbilityTuning* FormBuff = UVeyraAbilitiesTuningSubsystem::FindSelfBuff(Form);
		if (Statuses && FormBuff && Statuses->GetLedger().Entries.ContainsByPredicate([FormBuff](const FVeyraStatusEntry& Entry) { return FormBuff->Statuses.Contains(Entry.Id); }))
		{
			return true;
		}
	}
	return false;
}

TArray<FVeyraContentId> UVeyraSelfBuffAbility::FormsOf(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability) const
{
	TArray<FVeyraContentId> Forms = { Ability };
	const AActor* Owner = Caster.GetOwner();
	const UVeyraAbilityLoadoutComponent* Loadout = Owner ? Owner->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
	const FVeyraLoadoutEntry* Entry = Loadout ? Loadout->FindAbility(Ability) : nullptr;
	const auto IsStance = [](const FVeyraContentId& Id) {
		const FVeyraSelfBuffAbilityTuning* Buff = UVeyraAbilitiesTuningSubsystem::FindSelfBuff(Id);
		return Buff && Buff->Recast == EVeyraRecast::EndsEarly;
	};
	if (!Entry || !IsStance(Ability))
	{
		return Forms;
	}
	const FVeyraLoadoutEntry* Own = Loadout->FindOwnSlot(Entry->Slot);
	const FVeyraSlotOverride* Override = Loadout->FindOverride(Entry->Slot);
	for (const FVeyraLoadoutEntry* Form : { Own, Override ? &Override->Entry : nullptr })
	{
		if (Form && IsStance(Form->Ability))
		{
			Forms.AddUnique(Form->Ability);
		}
	}
	return Forms;
}

void UVeyraSelfBuffAbility::EndForms(UAbilitySystemComponent& Caster, TConstArrayView<FVeyraContentId> Forms)
{
	for (const FVeyraContentId& Form : Forms)
	{
		if (const FVeyraSelfBuffAbilityTuning* Buff = UVeyraAbilitiesTuningSubsystem::FindSelfBuff(Form))
		{
			for (const FVeyraContentId& Status : Buff->Statuses)
			{
				VeyraCombat::RemoveStatus(Caster, Status);
			}
		}
	}
}

void UVeyraSelfBuffAbility::EndEarly(UAbilitySystemComponent& Caster, const FVeyraContentId& Ability)
{
	// Every form of the stance ends with it.
	EndForms(Caster, FormsOf(Caster, Ability));
	StopAura();
}

FVeyraChannelPlan UVeyraSelfBuffAbility::Deliver(const FVeyraCast& Cast)
{
	const FVeyraSelfBuffAbilityTuning* Buff = UVeyraAbilitiesTuningSubsystem::FindSelfBuff(Cast.Ability);
	UAbilitySystemComponent* Caster = Cast.Caster.Get();
	UWorld* World = GetWorld();
	if (!Buff || !Caster || !World)
	{
		return FVeyraChannelPlan();
	}
	// Taking one form of a stance ends the others', so their bonuses never stack.
	TArray<FVeyraContentId> Others = FormsOf(*Caster, Cast.Ability);
	Others.Remove(Cast.Ability);
	EndForms(*Caster, Others);
	// A form an override holds lasts no longer than the override (ADR-018 §1).
	const UVeyraAbilityLoadoutComponent* Slots = Caster->GetOwner() ? Caster->GetOwner()->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
	const FVeyraLoadoutEntry* Entry = Slots ? Slots->FindAbility(Cast.Ability) : nullptr;
	const FVeyraSlotOverride* Holding = Entry ? Slots->FindOverride(Entry->Slot) : nullptr;
	const bool bHeldForAWhile = Holding && Holding->Entry.Ability == Cast.Ability && Holding->EndsAt > 0.0;
	const double HeldFor = bHeldForAWhile ? FMath::Max(0.0, Holding->EndsAt - World->GetTimeSeconds()) : 0.0;
	for (const FVeyraContentId& StatusId : Buff->Statuses)
	{
		if (TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId))
		{
			if (bHeldForAWhile)
			{
				Status->DurationSeconds = FMath::Min(Status->DurationSeconds, HeldFor);
			}
			VeyraCombat::ApplyStatus(*Caster, *Caster, Status.GetValue());
		}
	}
	for (const FVeyraShieldTuning& Shield : Buff->Shields)
	{
		// §51: the amount is built from the rank and the caster's stats, then the shield is created.
		VeyraCombat::GrantShield(*Caster, *Caster, VeyraEffectDelivery::ShieldGrant(*Caster, Shield, Cast.Rank));
	}
	for (const FVeyraAuraTuning& Aura : Buff->Aura)
	{
		AuraCaster = Caster;
		AuraAbility = Cast.Ability;
		AuraEndsAt = World->GetTimeSeconds() + Aura.DurationSeconds;
		World->GetTimerManager().SetTimer(AuraTimer, FTimerDelegate::CreateUObject(this, &UVeyraSelfBuffAbility::RefreshAura),
			static_cast<float>(Aura.RefreshSeconds), /*bLoop*/ true);
		RefreshAura();
	}
	for (const FVeyraHealTuning& Heal : Buff->Heal)
	{
		DeliverHeal(*Caster, Heal);
	}
	// While it lasts, its variants hold their slots (ADR-018 §1).
	if (UVeyraAbilityLoadoutComponent* Loadout = Caster->GetOwner() ? Caster->GetOwner()->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr)
	{
		for (const FVeyraVariantTuning& Variant : Buff->Variants)
		{
			FVeyraOverrideSpec Spec;
			Spec.Ability = Variant.Ability;
			Spec.DurationSeconds = Variant.DurationSeconds;
			Spec.Use = EVeyraOverrideUse::WhileActive;
			Spec.bSharesCooldown = Variant.Cooldown == EVeyraVariantCooldown::Shared;
			Loadout->Override(*Caster, Variant.Slot, Spec);
		}
	}
	return FVeyraChannelPlan();
}

void UVeyraSelfBuffAbility::DeliverHeal(UAbilitySystemComponent& Caster, const FVeyraHealTuning& Heal) const
{
	const int32 Level = GetCasterLevel(Caster);
	const double Amount = VeyraAbilityRules::AtLevel(Heal.Amount, Heal.AmountPerLevel, Level);
	TArray<UAbilitySystemComponent*, TInlineAllocator<2>> Healed = { &Caster };
	if (UAbilitySystemComponent* Ally = FindMostWoundedAlly(Caster, Heal.AllyRange))
	{
		Healed.Add(Ally);
	}
	for (UAbilitySystemComponent* Unit : Healed)
	{
		// Never above Max Health (Combat Bible §6); the caster's healing, for statistics (ADR-017 §1).
		VeyraCombat::RestoreHealthFrom(Caster, *Unit, Amount);
		for (const FVeyraContentId& StatusId : Heal.Statuses)
		{
			if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId, Level))
			{
				VeyraCombat::ApplyStatus(Caster, *Unit, Status.GetValue());
			}
		}
	}
}

UAbilitySystemComponent* UVeyraSelfBuffAbility::FindMostWoundedAlly(const UAbilitySystemComponent& Caster, double Range) const
{
	UWorld* World = GetWorld();
	const AActor* Body = Caster.GetAvatarActor();
	const EVeyraTeam Side = VeyraTeams::TeamOf(Caster.GetOwner());
	if (!World || !Body || !(Range > 0.0) || Side == EVeyraTeam::None)
	{
		return nullptr;
	}
	FVeyraShape Circle;
	Circle.Kind = EVeyraShapeKind::Circle;
	Circle.Radius = Range;
	const TArray<AActor*> Allies = VeyraShapes::GatherUnits(*World, FVeyraPlacedShape{ Circle, Body->GetActorLocation(), Body->GetActorForwardVector() },
		[Body, Side](const AActor& Unit) {
			return &Unit != Body && VeyraTeams::TeamOf(&Unit) == Side && VeyraUnits::IsVanguard(&Unit) && VeyraTargeting::IsAlive(&Unit);
		});
	// The ally that lacks the most of its Health; one at full Health needs none (League's Heal).
	UAbilitySystemComponent* MostWounded = nullptr;
	double LowestFraction = 1.0;
	for (AActor* Ally : Allies)
	{
		UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Ally);
		const double MaxHealth = AbilitySystem ? AbilitySystem->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) : 0.0;
		const double Fraction = MaxHealth > 0.0 ? AbilitySystem->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute()) / MaxHealth : 1.0;
		if (Fraction < LowestFraction)
		{
			LowestFraction = Fraction;
			MostWounded = AbilitySystem;
		}
	}
	return MostWounded;
}

void UVeyraSelfBuffAbility::RefreshAura()
{
	UWorld* World = GetWorld();
	UAbilitySystemComponent* Caster = AuraCaster.Get();
	const AActor* Body = Caster ? Caster->GetAvatarActor() : nullptr;
	const FVeyraSelfBuffAbilityTuning* Buff = UVeyraAbilitiesTuningSubsystem::FindSelfBuff(AuraAbility);
	// The aura ends with its time, or with its caster.
	if (!World || !Body || !Buff || Buff->Aura.IsEmpty() || World->GetTimeSeconds() >= AuraEndsAt || !VeyraTargeting::IsAlive(Body))
	{
		StopAura();
		return;
	}
	const FVeyraAuraTuning& Aura = Buff->Aura[0];
	FVeyraShape Circle;
	Circle.Kind = EVeyraShapeKind::Circle;
	Circle.Radius = Aura.Radius;
	const EVeyraTeam Side = VeyraTeams::TeamOf(Caster->GetOwner());
	const TArray<AActor*> Allies = VeyraShapes::GatherUnits(*World, FVeyraPlacedShape{ Circle, Body->GetActorLocation(), Body->GetActorForwardVector() },
		[Body, Side](const AActor& Unit) { return &Unit != Body && Side != EVeyraTeam::None && VeyraTeams::TeamOf(&Unit) == Side && VeyraUnits::IsVanguard(&Unit); });
	for (AActor* Ally : Allies)
	{
		UAbilitySystemComponent* Target = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Ally);
		for (const FVeyraContentId& StatusId : Aura.AllyStatuses)
		{
			const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId);
			if (Target && Status.IsSet())
			{
				VeyraCombat::ApplyStatus(*Caster, *Target, Status.GetValue());
			}
		}
	}
}

void UVeyraSelfBuffAbility::StopAura()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AuraTimer);
	}
	AuraCaster = nullptr;
	AuraAbility = FVeyraContentId();
}

bool UVeyraSelfBuffAbility::IsAuraRunning() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(AuraTimer);
}

bool UVeyraSelfBuffAbility::IsOffensive(const FVeyraContentId& /*Ability*/) const
{
	// It acts on its caster alone.
	return false;
}
