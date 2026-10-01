// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Delivery/VeyraLingeringArea.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Movement/VeyraMovementFields.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Targeting/VeyraTargeting.h"
#include "TimerManager.h"
#include "Units/VeyraUnit.h"
#include "VeyraCombatVerbs.h"

AVeyraLingeringArea::AVeyraLingeringArea()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	// Always relevant, so replays record it; Vision's fog gate decides which clients receive it (ADR-016 §3).
	bAlwaysRelevant = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AVeyraLingeringArea::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraLingeringArea, Shape, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraLingeringArea, Direction, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraLingeringArea, EndsAt, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraLingeringArea, EndWarningSeconds, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraLingeringArea, Team, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraLingeringArea, Ability, Params);
}

void AVeyraLingeringArea::Arm(UAbilitySystemComponent& InCaster, const FVeyraEffectFrame& Placement, const FVeyraShape& InShape,
	FVeyraLingerStatuses InStatuses, FVeyraLingerEffects InEffects, double DurationSeconds, double PulseSeconds, const FVeyraContentId& InAbility)
{
	Caster = &InCaster;
	Statuses = MoveTemp(InStatuses);
	Effects = MoveTemp(InEffects);
	Shape = InShape;
	Direction = Placement.Direction;
	EndsAt = GetWorld()->GetTimeSeconds() + DurationSeconds;
	EndWarningSeconds = Effects.End.IsEmpty() ? 0.0 : Effects.EndWarningSeconds;
	Team = VeyraTeams::TeamOf(InCaster.GetOwner());
	Ability = InAbility;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraLingeringArea, Shape, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraLingeringArea, Direction, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraLingeringArea, EndsAt, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraLingeringArea, EndWarningSeconds, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraLingeringArea, Team, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraLingeringArea, Ability, this);
	FTimerManager& Timers = GetWorldTimerManager();
	Timers.SetTimer(PulseTimer, FTimerDelegate::CreateUObject(this, &AVeyraLingeringArea::Pulse), static_cast<float>(PulseSeconds), /*bLoop*/ true);
	Timers.SetTimer(EndTimer, FTimerDelegate::CreateUObject(this, &AVeyraLingeringArea::End), static_cast<float>(DurationSeconds), /*bLoop*/ false);
	// As it lands it gives its statuses; its zones have done the rest.
	GiveStatuses();
}

void AVeyraLingeringArea::HoldField(double Pull)
{
	UVeyraMovementFieldSubsystem* Fields = GetWorld() ? GetWorld()->GetSubsystem<UVeyraMovementFieldSubsystem>() : nullptr;
	if (Fields && FieldHandle == 0)
	{
		FieldHandle = Fields->Add(FVeyraMovementField{ GetActorLocation(), Shape.Radius, Team, Pull });
	}
}

void AVeyraLingeringArea::BuildShields(const FVeyraShieldTuning& Shield, int32 Rank, double DelayAfterDamageSeconds)
{
	BuiltShield = Shield;
	BuiltShieldRank = Rank;
	BuildDelaySeconds = DelayAfterDamageSeconds;
	// It hears of damage from now on, to hold its shield back from one just hurt.
	if (UVeyraCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr; Events && !DamageHandle.IsValid())
	{
		DamageHandle = Events->OnDamageDealt.AddUObject(this, &AVeyraLingeringArea::OnDamageDealt);
	}
	TopUpShields();
}

void AVeyraLingeringArea::OnDamageDealt(const FVeyraDamageDealtEvent& Event)
{
	if (const UAbilitySystemComponent* Target = Event.Target.Get(); Target && Event.Total() > 0.0 && GetWorld())
	{
		LastDamagedAt.Add(Target, GetWorld()->GetTimeSeconds());
	}
}

void AVeyraLingeringArea::TopUpShields()
{
	UAbilitySystemComponent* Source = Caster.Get();
	const UWorld* World = GetWorld();
	if (!BuiltShield.IsSet() || !Source || !World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	for (AActor* Unit : VeyraShapes::GatherUnits(*World, GetPlacedShape(), [](const AActor&) { return true; }))
	{
		UAbilitySystemComponent* Inside = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Unit);
		// Its caster, and the living allied Vanguards inside.
		const bool bReaches = Inside && VeyraTargeting::IsAlive(Unit)
			&& (Inside == Source || (VeyraTeams::TeamOf(Unit) == Team && VeyraUnits::IsVanguard(Unit)));
		const double* Hurt = bReaches ? LastDamagedAt.Find(Inside) : nullptr;
		if (!bReaches || (Hurt && Now - *Hurt < BuildDelaySeconds))
		{
			continue;
		}
		VeyraEffectDelivery::GrantShield(*Source, *Inside, BuiltShield.GetValue(), BuiltShieldRank);
	}
}

void AVeyraLingeringArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UVeyraCombatEventSubsystem* Events = DamageHandle.IsValid() && GetWorld() ? GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnDamageDealt.Remove(DamageHandle);
		DamageHandle.Reset();
	}
	GetWorldTimerManager().ClearTimer(PulseTimer);
	GetWorldTimerManager().ClearTimer(EndTimer);
	Super::EndPlay(EndPlayReason);
}

void AVeyraLingeringArea::Destroyed()
{
	// Its field goes with it, however it ends: its time, or a recast that rips it up.
	if (UVeyraMovementFieldSubsystem* Fields = FieldHandle != 0 && GetWorld() ? GetWorld()->GetSubsystem<UVeyraMovementFieldSubsystem>() : nullptr)
	{
		Fields->Remove(FieldHandle);
		FieldHandle = 0;
	}
	Super::Destroyed();
}

void AVeyraLingeringArea::Pulse()
{
	GiveStatuses();
	TopUpShields();
	Deal(Effects.Pulse, /*bTick*/ true);
}

void AVeyraLingeringArea::End()
{
	Deal(Effects.End, /*bTick*/ false);
	Destroy();
}

void AVeyraLingeringArea::Deal(TConstArrayView<FVeyraPreparedZone> Zones, bool bTick)
{
	UAbilitySystemComponent* Source = Caster.Get();
	UWorld* World = GetWorld();
	if (Zones.IsEmpty() || !Source || !World)
	{
		return;
	}
	// Measured from its centre, so a displacement toward the origin draws the units in.
	FVeyraEffectFrame Frame;
	Frame.Origin = GetActorLocation();
	Frame.Direction = Direction;
	// A pulse is a tick, which passes a Spell Shield as damage over time does; the end is a hit it blocks (ADR-025 §4).
	FVeyraAbilityHitSource HitSource{ Ability, Effects.CastId };
	HitSource.bSkipSpellShield = bTick;
	VeyraAreaDelivery::Resolve(*World, *Source, Frame, Zones, HitSource);
}

void AVeyraLingeringArea::GiveStatuses()
{
	UAbilitySystemComponent* Source = Caster.Get();
	const UWorld* World = GetWorld();
	if (!Source || !World)
	{
		return;
	}
	for (AActor* Unit : VeyraShapes::GatherUnits(*World, GetPlacedShape(), [](const AActor&) { return true; }))
	{
		UAbilitySystemComponent* Inside = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Unit);
		if (!Inside)
		{
			continue;
		}
		const bool bAlly = VeyraTeams::TeamOf(Unit) == Team;
		// An Untargetable enemy takes nothing new from it (Combat Bible §10).
		if (!bAlly && Inside != Source && VeyraTargeting::IsUntargetable(*Unit))
		{
			continue;
		}
		const TArray<FVeyraStatusSpec>* Given = Inside == Source ? &Statuses.Caster
			: bAlly													  ? (VeyraUnits::IsVanguard(Unit) ? &Statuses.Allies : nullptr)
																	  : &Statuses.Enemies;
		if (!Given)
		{
			continue;
		}
		for (const FVeyraStatusSpec& Status : *Given)
		{
			VeyraCombat::ApplyStatus(*Source, *Inside, Status);
		}
	}
}
