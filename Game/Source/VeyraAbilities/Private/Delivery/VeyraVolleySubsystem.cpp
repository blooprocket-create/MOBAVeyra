// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Delivery/VeyraVolleySubsystem.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Teams/VeyraTeam.h"
#include "Tuning/VeyraAbilitiesTuning.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraAbilitiesLog.h"
#include "VeyraCombatVerbs.h"

void UVeyraVolleySubsystem::Open(UAbilitySystemComponent& Caster, EVeyraAbilitySlot Slot, const FVeyraContentId& Ability,
	const FVeyraVolleyAbilityTuning& Tuning, const FVector& Direction, int32 CasterLevel)
{
	UWorld* World = GetWorld();
	UVeyraAbilityLoadoutComponent* Loadout = Caster.GetOwner() ? Caster.GetOwner()->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
	if (!World || !Loadout)
	{
		return;
	}
	Close(Caster);
	if (!DisplacedHandle.IsValid())
	{
		if (UVeyraCombatEventSubsystem* Events = World->GetSubsystem<UVeyraCombatEventSubsystem>())
		{
			DisplacedHandle = Events->OnDisplaced.AddUObject(this, &UVeyraVolleySubsystem::OnDisplaced);
		}
	}
	FLane& Lane = Lanes.AddDefaulted_GetRef();
	Lane.Caster = &Caster;
	Lane.Slot = Slot;
	Lane.Shot = Tuning.Shot;
	Lane.Direction = Direction.GetSafeNormal2D().IsNearlyZero() ? FVector::ForwardVector : Direction.GetSafeNormal2D();
	Lane.HalfAngleDegrees = Tuning.LaneHalfAngleDegrees;
	Lane.ShotsLeft = Tuning.Shots;
	if (!Tuning.Bonus.IsEmpty())
	{
		Lane.BonusStatus = Tuning.Bonus[0].Status;
		Lane.BonusLeft = Tuning.Bonus[0].MaxShots;
	}
	// The stance lasts as the lane does; closing early ends it.
	for (const FVeyraContentId& Id : Tuning.CasterStatuses)
	{
		if (TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(Id, CasterLevel))
		{
			Status->DurationSeconds = Tuning.DurationSeconds;
			VeyraCombat::ApplyStatus(Caster, Caster, Status.GetValue());
			Lane.CasterStatuses.Add(Id);
		}
	}
	FVeyraOverrideSpec Hold;
	Hold.Ability = Tuning.Shot;
	Hold.DurationSeconds = Tuning.DurationSeconds;
	Hold.Use = EVeyraOverrideUse::WhileActive;
	Loadout->Override(Caster, Slot, Hold);
	World->GetTimerManager().SetTimer(Lane.Timer, FTimerDelegate::CreateWeakLambda(this, [this, Weak = TWeakObjectPtr<UAbilitySystemComponent>(&Caster)] {
		if (UAbilitySystemComponent* Owner = Weak.Get())
		{
			Close(*Owner);
		}
	}), static_cast<float>(Tuning.DurationSeconds), /*bLoop*/ false);
	UE_LOG(LogVeyraAbilities, Verbose, TEXT("%s opened %s's lane: %d shot(s) for %g s."), *GetNameSafe(Caster.GetOwner()), *Ability.ToString(),
		Lane.ShotsLeft, Tuning.DurationSeconds);
}

FVector UVeyraVolleySubsystem::AimWithin(const UAbilitySystemComponent& Caster, const FVeyraContentId& Shot, const FVector& Direction) const
{
	const FLane* Lane = Find(Caster);
	const FVector Aim = Direction.GetSafeNormal2D();
	if (!Lane || Lane->Shot != Shot || Aim.IsNearlyZero())
	{
		return Direction;
	}
	// Within the lane's angle it goes where aimed; beyond it, along the nearer edge.
	const double Off = FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(Lane->Direction, Aim).Z, FVector::DotProduct(Lane->Direction, Aim)));
	const double Kept = FMath::Clamp(Off, -Lane->HalfAngleDegrees, Lane->HalfAngleDegrees);
	return Lane->Direction.RotateAngleAxis(Kept, FVector::UpVector);
}

void UVeyraVolleySubsystem::NoteShot(UAbilitySystemComponent& Caster, const FVeyraContentId& Shot)
{
	FLane* Lane = Find(Caster);
	if (!Lane || Lane->Shot != Shot)
	{
		return;
	}
	if (--Lane->ShotsLeft <= 0)
	{
		Close(Caster);
	}
}

int32 UVeyraVolleySubsystem::GetShotsLeft(const UAbilitySystemComponent& Caster) const
{
	const FLane* Lane = Find(Caster);
	return Lane ? Lane->ShotsLeft : 0;
}

void UVeyraVolleySubsystem::Close(UAbilitySystemComponent& Caster)
{
	const int32 Index = Lanes.IndexOfByPredicate([&Caster](const FLane& Lane) { return Lane.Caster.Get() == &Caster; });
	if (Index == INDEX_NONE)
	{
		return;
	}
	FLane Lane = MoveTemp(Lanes[Index]);
	Lanes.RemoveAt(Index);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Lane.Timer);
	}
	if (UVeyraAbilityLoadoutComponent* Loadout = Caster.GetOwner() ? Caster.GetOwner()->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr)
	{
		const FVeyraLoadoutEntry* Held = Loadout->FindSlot(Lane.Slot);
		if (Held && Held->Ability == Lane.Shot)
		{
			Loadout->EndOverride(Caster, Lane.Slot);
		}
	}
	for (const FVeyraContentId& Id : Lane.CasterStatuses)
	{
		VeyraCombat::RemoveStatus(Caster, Id);
	}
}

void UVeyraVolleySubsystem::OnDisplaced(const FVeyraDisplacementEvent& Event)
{
	const UAbilitySystemComponent* Source = Event.Source.Get();
	const UAbilitySystemComponent* Target = Event.Target.Get();
	const UVeyraStatusComponent* Marks = Target && Target->GetOwner() ? Target->GetOwner()->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
	if (!Source || !Marks)
	{
		return;
	}
	for (FLane& Lane : Lanes)
	{
		const UAbilitySystemComponent* Caster = Lane.Caster.Get();
		// An ally's displacement, not the caster's own, of an enemy the caster marked.
		const bool bAlly = Caster && Source != Caster && VeyraTeams::TeamOf(Source->GetOwner()) == VeyraTeams::TeamOf(Caster->GetOwner());
		if (bAlly && Lane.BonusLeft > 0 && Lane.BonusStatus.IsValid() && Marks->HasFrom(Lane.BonusStatus, *Caster))
		{
			--Lane.BonusLeft;
			++Lane.ShotsLeft;
			UE_LOG(LogVeyraAbilities, Verbose, TEXT("%s's lane earned a shot: %d left."), *GetNameSafe(Caster->GetOwner()), Lane.ShotsLeft);
		}
	}
}

UVeyraVolleySubsystem::FLane* UVeyraVolleySubsystem::Find(const UAbilitySystemComponent& Caster)
{
	return Lanes.FindByPredicate([&Caster](const FLane& Lane) { return Lane.Caster.Get() == &Caster; });
}

const UVeyraVolleySubsystem::FLane* UVeyraVolleySubsystem::Find(const UAbilitySystemComponent& Caster) const
{
	return Lanes.FindByPredicate([&Caster](const FLane& Lane) { return Lane.Caster.Get() == &Caster; });
}

void UVeyraVolleySubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (UVeyraCombatEventSubsystem* Events = World->GetSubsystem<UVeyraCombatEventSubsystem>())
		{
			Events->OnDisplaced.Remove(DisplacedHandle);
		}
		for (FLane& Lane : Lanes)
		{
			World->GetTimerManager().ClearTimer(Lane.Timer);
		}
	}
	Lanes.Reset();
	Super::Deinitialize();
}
