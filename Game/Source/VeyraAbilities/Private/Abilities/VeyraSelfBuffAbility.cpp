// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraSelfBuffAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Delivery/VeyraAreaDelivery.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
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

EVeyraCastRejection UVeyraSelfBuffAbility::CheckTarget(const AActor& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const
{
	const FVeyraSelfBuffAbilityTuning* Buff = UVeyraAbilitiesTuningSubsystem::FindSelfBuff(Ability);
	if (!Buff)
	{
		return EVeyraCastRejection::UnknownAbility;
	}
	if (Buff->Recipient != EVeyraBuffRecipient::CasterOrAlly)
	{
		return EVeyraCastRejection::None;
	}
	// An allied Vanguard it names must be alive and within range; naming anything else, or nothing,
	// buffs the caster, as League's smart self-cast does (ADR-027 §4, §9).
	switch (VeyraTargeting::CheckAllyTarget(Caster, Target.Actor, Buff->Cast.CastRange))
	{
	case EVeyraTargetValidity::Dead:
		return EVeyraCastRejection::TargetDead;
	case EVeyraTargetValidity::OutOfRange:
		return EVeyraCastRejection::OutOfRange;
	default:
		return EVeyraCastRejection::None;
	}
}

UAbilitySystemComponent* UVeyraSelfBuffAbility::RecipientOf(const FVeyraCast& Cast, const FVeyraSelfBuffAbilityTuning& Buff, UAbilitySystemComponent& Caster)
{
	const AActor* Body = Caster.GetAvatarActor();
	AActor* Named = Cast.TargetActor.Get();
	if (Buff.Recipient != EVeyraBuffRecipient::CasterOrAlly || !Body || !Named)
	{
		return &Caster;
	}
	// Its range was checked as the cast began; an ally that stepped away since still takes it.
	const EVeyraTargetValidity Validity = VeyraTargeting::CheckAllyTarget(*Body, Named, Buff.Cast.CastRange);
	UAbilitySystemComponent* Ally = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Named);
	return Ally && (Validity == EVeyraTargetValidity::Valid || Validity == EVeyraTargetValidity::OutOfRange) ? Ally : &Caster;
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
	// The caster, or the ally the cast names (ADR-027 §4); the buff stays the caster's, built from its stats.
	UAbilitySystemComponent* Recipient = RecipientOf(Cast, *Buff, *Caster);
	for (const FVeyraContentId& StatusId : Buff->Statuses)
	{
		if (TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId))
		{
			if (bHeldForAWhile)
			{
				Status->DurationSeconds = FMath::Min(Status->DurationSeconds, HeldFor);
			}
			VeyraCombat::ApplyStatus(*Caster, *Recipient, Status.GetValue());
		}
	}
	// Its zones land once on its recipient, facing away from the caster, as ROOM TO BREATHE pushes enemies out (ADR-027 §8).
	const AActor* Around = Recipient->GetAvatarActor();
	if (!Buff->RecipientZones.IsEmpty() && Around)
	{
		const AActor* Body = Caster->GetAvatarActor();
		FVeyraEffectFrame Frame;
		Frame.Origin = Around->GetActorLocation();
		const FVector Away = Body ? (Frame.Origin - Body->GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
		Frame.Direction = Away.IsNearlyZero() ? Around->GetActorForwardVector().GetSafeNormal2D() : Away;
		VeyraAreaDelivery::Resolve(*World, *Caster, Frame, VeyraAreaDelivery::PrepareZones(*Caster, Buff->RecipientZones, Cast.Rank),
			FVeyraAbilityHitSource{ Cast.Ability, Cast.CastId });
	}
	for (const FVeyraShieldTuning& Shield : Buff->Shields)
	{
		// §51: the amount is built from the rank and the caster's stats, then the shield is created.
		VeyraEffectDelivery::GrantShield(*Caster, *Recipient, Shield, Cast.Rank);
	}
	for (const FVeyraAuraTuning& Aura : Buff->Aura)
	{
		AuraCaster = Caster;
		AuraHolder = Recipient;
		AuraAbility = Cast.Ability;
		AuraEndsAt = World->GetTimeSeconds() + Aura.DurationSeconds;
		// Bound to the caster, not to this ability: GAS clears an ability's own timers as its cast ends,
		// and the aura outlasts the cast.
		TWeakObjectPtr<UVeyraSelfBuffAbility> Self(this);
		World->GetTimerManager().SetTimer(AuraTimer, FTimerDelegate::CreateWeakLambda(Caster, [Self]() {
			if (UVeyraSelfBuffAbility* Ability = Self.Get())
			{
				Ability->RefreshAura();
			}
		}), static_cast<float>(Aura.RefreshSeconds), /*bLoop*/ true);
		RefreshAura();
	}
	for (const FVeyraHealTuning& Heal : Buff->Heal)
	{
		DeliverHeal(*Caster, *Recipient, Heal);
	}
	for (const FVeyraTemporaryHealthTuning& Temporary : Buff->TemporaryHealth)
	{
		// Its amount by rank and a share of the caster's Max Health (Combat Bible §7).
		const double MaxHealth = Caster->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
		VeyraCombat::GrantTemporaryHealth(*Caster, *Recipient, VeyraAbilityRules::ValueAtRank(Temporary.AmountByRank, Cast.Rank) + MaxHealth * Temporary.MaxHealthRatio,
			Temporary.DurationSeconds);
	}
	if (!Buff->EndPayload.IsEmpty())
	{
		StartPayload(*Caster, Cast.Ability);
	}
	// Its recipient's attacks offer an impact for a while, its damage from the caster's power now (ADR-027 §3).
	UVeyraBasicAttackComponent* Attacks = Recipient->GetOwner() ? Recipient->GetOwner()->FindComponentByClass<UVeyraBasicAttackComponent>() : nullptr;
	for (const FVeyraBuffAttackImpactTuning& Timed : Buff->AttackSecondaryImpact)
	{
		if (Attacks)
		{
			Attacks->OfferImpactWhileLasting(VeyraEffectDelivery::SecondaryImpact(*Caster, Timed.Impact, Cast.Rank), Timed.Seconds);
		}
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

void UVeyraSelfBuffAbility::DeliverHeal(UAbilitySystemComponent& Caster, UAbilitySystemComponent& Recipient, const FVeyraHealTuning& Heal) const
{
	const int32 Level = GetCasterLevel(Caster);
	const double Amount = VeyraAbilityRules::AtLevel(Heal.Amount, Heal.AmountPerLevel, Level);
	TArray<UAbilitySystemComponent*, TInlineAllocator<2>> Healed = { &Recipient };
	if (UAbilitySystemComponent* Ally = FindMostWoundedAlly(Recipient, Heal.AllyRange))
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

UAbilitySystemComponent* UVeyraSelfBuffAbility::FindMostWoundedAlly(const UAbilitySystemComponent& Unit, double Range) const
{
	UWorld* World = GetWorld();
	const AActor* Body = Unit.GetAvatarActor();
	const EVeyraTeam Side = VeyraTeams::TeamOf(Unit.GetOwner());
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
	// It follows its holder, the caster or the ally it buffs (ADR-027 §4); its statuses are the caster's.
	const UAbilitySystemComponent* Holder = AuraHolder.Get();
	const AActor* Body = Holder ? Holder->GetAvatarActor() : nullptr;
	const FVeyraSelfBuffAbilityTuning* Buff = UVeyraAbilitiesTuningSubsystem::FindSelfBuff(AuraAbility);
	// The aura ends with its time, or with its holder.
	if (!World || !Caster || !Body || !Buff || Buff->Aura.IsEmpty() || World->GetTimeSeconds() >= AuraEndsAt || !VeyraTargeting::IsAlive(Body))
	{
		StopAura();
		return;
	}
	const FVeyraAuraTuning& Aura = Buff->Aura[0];
	FVeyraShape Circle;
	Circle.Kind = EVeyraShapeKind::Circle;
	Circle.Radius = Aura.Radius;
	const EVeyraTeam Side = VeyraTeams::TeamOf(Holder->GetOwner());
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
	if (Aura.EnemyStatuses.IsEmpty())
	{
		return;
	}
	// The living enemy units in range take its enemy statuses; Combat refuses them for structures and wards.
	const TArray<AActor*> Enemies = VeyraShapes::GatherUnits(*World, FVeyraPlacedShape{ Circle, Body->GetActorLocation(), Body->GetActorForwardVector() },
		[Body](const AActor& Unit) { return VeyraTargeting::AreHostile(Body, &Unit) && VeyraTargeting::IsAlive(&Unit); });
	for (AActor* Enemy : Enemies)
	{
		UAbilitySystemComponent* Target = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Enemy);
		for (const FVeyraContentId& StatusId : Aura.EnemyStatuses)
		{
			const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId);
			if (Target && Status.IsSet())
			{
				VeyraCombat::ApplyStatus(*Caster, *Target, Status.GetValue());
			}
		}
	}
}

void UVeyraSelfBuffAbility::StartPayload(UAbilitySystemComponent& Caster, const FVeyraContentId& Ability)
{
	UWorld* World = GetWorld();
	const FVeyraSelfBuffAbilityTuning* Buff = UVeyraAbilitiesTuningSubsystem::FindSelfBuff(Ability);
	UVeyraCombatEventSubsystem* Events = World ? World->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr;
	if (!World || !Buff || Buff->EndPayload.IsEmpty() || !Events)
	{
		return;
	}
	StopPayload();
	PayloadCaster = &Caster;
	PayloadAbility = Ability;
	PayloadHits = 0;
	HostileDamageHandle = Events->OnHostileDamage.AddUObject(this, &UVeyraSelfBuffAbility::OnHostileDamage);
	// Bound to the caster, as the aura's is: the payload comes after the cast has ended.
	TWeakObjectPtr<UVeyraSelfBuffAbility> Self(this);
	World->GetTimerManager().SetTimer(PayloadTimer, FTimerDelegate::CreateWeakLambda(&Caster, [Self]() {
		if (UVeyraSelfBuffAbility* Ability = Self.Get())
		{
			Ability->FirePayload();
		}
	}), static_cast<float>(Buff->EndPayload[0].AfterSeconds), /*bLoop*/ false);
}

void UVeyraSelfBuffAbility::OnHostileDamage(const FVeyraHostileDamageEvent& Event)
{
	// Each hostile hit its caster takes while the buff lasts agitates it (Roster Bible §5).
	if (PayloadCaster.IsValid() && Event.Target.Get() == PayloadCaster.Get())
	{
		++PayloadHits;
	}
}

void UVeyraSelfBuffAbility::FirePayload()
{
	UWorld* World = GetWorld();
	UAbilitySystemComponent* Caster = PayloadCaster.Get();
	const AActor* Body = Caster ? Caster->GetAvatarActor() : nullptr;
	const FVeyraSelfBuffAbilityTuning* Buff = UVeyraAbilitiesTuningSubsystem::FindSelfBuff(PayloadAbility);
	const int32 Hits = PayloadHits;
	StopPayload();
	if (!World || !Body || !Buff || Buff->EndPayload.IsEmpty() || !VeyraTargeting::IsAlive(Body))
	{
		return;
	}
	const FVeyraEndPayloadTuning& Payload = Buff->EndPayload[0];
	// A payload that needs hits comes only after that many (Roster Bible §1: Countersteer's counter).
	if (Hits < Payload.MinHits)
	{
		return;
	}
	TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(Payload.Status, GetCasterLevel(*Caster));
	if (!Status.IsSet())
	{
		return;
	}
	// Longer for each hit it took, up to its most (ADR-018 §6).
	Status->DurationSeconds = FMath::Min(Payload.BaseSeconds + Payload.SecondsPerHit * Hits, Payload.MaxSeconds);
	FVeyraShape Circle;
	Circle.Kind = EVeyraShapeKind::Circle;
	Circle.Radius = Payload.Radius;
	const TArray<AActor*> Enemies = VeyraShapes::GatherUnits(*World, FVeyraPlacedShape{ Circle, Body->GetActorLocation(), Body->GetActorForwardVector() },
		[Body](const AActor& Unit) { return VeyraTargeting::AreHostile(Body, &Unit) && VeyraTargeting::IsAlive(&Unit); });
	for (AActor* Enemy : Enemies)
	{
		// A discrete hit on each, which a Spell Shield blocks (Combat Bible §19; ADR-025 §4).
		UAbilitySystemComponent* Target = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Enemy);
		if (Target && !VeyraCombat::BlockAbilityHit(*Target, *Caster))
		{
			VeyraCombat::ApplyStatus(*Caster, *Target, Status.GetValue());
		}
	}
}

void UVeyraSelfBuffAbility::StopPayload()
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(PayloadTimer);
	}
	if (UVeyraCombatEventSubsystem* Events = World ? World->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnHostileDamage.Remove(HostileDamageHandle);
	}
	HostileDamageHandle.Reset();
	PayloadCaster.Reset();
	PayloadAbility = FVeyraContentId();
	PayloadHits = 0;
}

void UVeyraSelfBuffAbility::StopAura()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AuraTimer);
	}
	AuraCaster = nullptr;
	AuraHolder = nullptr;
	AuraAbility = FVeyraContentId();
}

bool UVeyraSelfBuffAbility::IsAuraRunning() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(AuraTimer);
}

bool UVeyraSelfBuffAbility::IsOffensive(const FVeyraContentId& Ability) const
{
	// It acts on its caster, or on an ally it names, unless its zones hit the enemies around them.
	const FVeyraSelfBuffAbilityTuning* Buff = UVeyraAbilitiesTuningSubsystem::FindSelfBuff(Ability);
	return Buff && !Buff->RecipientZones.IsEmpty();
}
