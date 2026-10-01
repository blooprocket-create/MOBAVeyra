// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Passives/VeyraHauntPassive.h"

#include "AbilitySystemComponent.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Teams/VeyraTeam.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraCombatVerbs.h"

namespace VeyraHaunt
{
	// A passive has no ranks; its one amounts apply at every level.
	constexpr int32 PassiveRank = 1;
}

void UVeyraHauntPassive::Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId)
{
	Super::Start(Owner, InPassiveId);
	if (UVeyraCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		DamageHandle = Events->OnHostileDamage.AddUObject(this, &UVeyraHauntPassive::OnHostileDamage);
	}
}

void UVeyraHauntPassive::Stop()
{
	if (UVeyraCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnHostileDamage.Remove(DamageHandle);
	}
	DamageHandle.Reset();
	NextHauntAt.Reset();
	Super::Stop();
}

void UVeyraHauntPassive::OnHostileDamage(const FVeyraHostileDamageEvent& Event)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraHauntTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindHaunt(PassiveId);
	// A companion's damage is its owner's (ADR-034 §1).
	UAbilitySystemComponent* Source = Event.Responsible.IsValid() ? Event.Responsible.Get() : Event.Source.Get();
	UAbilitySystemComponent* Target = Event.Target.Get();
	const UWorld* World = GetWorld();
	const AActor* Body = Owner ? Owner->GetAvatarActor() : nullptr;
	if (!Tuning || !Source || !Target || !World || !Body || Source == Owner || !VeyraTargeting::IsAlive(Body))
	{
		return;
	}
	// Only an enemy Vanguard's damage wakes the spirit.
	const EVeyraTeam Side = VeyraTeams::TeamOf(Owner->GetOwner());
	if (VeyraTeams::TeamOf(Source->GetOwner()) == Side || !VeyraUnits::IsVanguard(Source->GetAvatarActor()))
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	if (Target == Owner)
	{
		// It started this with him: Haunted, once per its own cooldown (Roster Bible §5).
		const double* Next = NextHauntAt.Find(Source);
		const TOptional<FVeyraStatusSpec> Haunt = UVeyraAbilitiesTuningSubsystem::FindStatus(Tuning->HauntStatus);
		if ((!Next || Now >= *Next) && Haunt.IsSet() && VeyraCombat::ApplyStatus(*Owner, *Source, Haunt.GetValue()))
		{
			NextHauntAt.Add(Source, Now + Tuning->PerEnemyCooldownSeconds);
		}
		return;
	}
	// A Haunted enemy that damages one of his allies near him instead: the spirit lashes out, and the Haunt is spent.
	const AActor* Victim = Target->GetAvatarActor();
	const UVeyraStatusComponent* Marks = Source->GetOwner() ? Source->GetOwner()->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
	const bool bHaunted = Marks && Marks->HasFrom(Tuning->HauntStatus, *Owner);
	const bool bAllyNear = Victim && VeyraTeams::TeamOf(Target->GetOwner()) == Side && VeyraUnits::IsVanguard(Victim)
		&& FVector::Dist2D(Victim->GetActorLocation(), Body->GetActorLocation()) <= Tuning->AllyRadius;
	if (!bHaunted || !bAllyNear)
	{
		return;
	}
	VeyraCombat::RemoveStatus(*Source, Tuning->HauntStatus);
	FVeyraRawDamageEvent Lash;
	Lash.Components.Add({ Tuning->Damage.Type, VeyraEffectDelivery::DamageAmount(*Owner, Tuning->Damage, VeyraHaunt::PassiveRank) });
	Lash.Delivery = EVeyraDamageDelivery::Proc;
	VeyraCombat::DealDamage(*Owner, *Source, Lash);
	for (const FVeyraContentId& StatusId : Tuning->Statuses)
	{
		if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId))
		{
			VeyraCombat::ApplyStatus(*Owner, *Source, Status.GetValue());
		}
	}
}
