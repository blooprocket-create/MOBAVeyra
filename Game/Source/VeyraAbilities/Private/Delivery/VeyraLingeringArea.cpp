// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Delivery/VeyraLingeringArea.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
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
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraLingeringArea, Team, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraLingeringArea, Ability, Params);
}

void AVeyraLingeringArea::Arm(UAbilitySystemComponent& InCaster, const FVeyraEffectFrame& Placement, const FVeyraShape& InShape,
	FVeyraLingerStatuses InStatuses, double DurationSeconds, double PulseSeconds, const FVeyraContentId& InAbility)
{
	Caster = &InCaster;
	Statuses = MoveTemp(InStatuses);
	Shape = InShape;
	Direction = Placement.Direction;
	EndsAt = GetWorld()->GetTimeSeconds() + DurationSeconds;
	Team = VeyraTeams::TeamOf(InCaster.GetOwner());
	Ability = InAbility;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraLingeringArea, Shape, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraLingeringArea, Direction, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraLingeringArea, EndsAt, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraLingeringArea, Team, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraLingeringArea, Ability, this);
	FTimerManager& Timers = GetWorldTimerManager();
	Timers.SetTimer(PulseTimer, FTimerDelegate::CreateUObject(this, &AVeyraLingeringArea::Pulse), static_cast<float>(PulseSeconds), /*bLoop*/ true);
	Timers.SetTimer(EndTimer, FTimerDelegate::CreateWeakLambda(this, [this] { Destroy(); }), static_cast<float>(DurationSeconds), /*bLoop*/ false);
	Pulse();
}

void AVeyraLingeringArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(PulseTimer);
	GetWorldTimerManager().ClearTimer(EndTimer);
	Super::EndPlay(EndPlayReason);
}

void AVeyraLingeringArea::Pulse()
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
