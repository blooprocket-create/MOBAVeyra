// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Delivery/VeyraDelayedArea.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AVeyraDelayedArea::AVeyraDelayedArea()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	// Always relevant, so replays record it; Vision's fog gate decides which clients receive it (ADR-016 §3).
	bAlwaysRelevant = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AVeyraDelayedArea::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraDelayedArea, Shapes, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraDelayedArea, Direction, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraDelayedArea, ResolvesAt, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraDelayedArea, Team, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraDelayedArea, Ability, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraDelayedArea, CastId, Params);
}

void AVeyraDelayedArea::Arm(UAbilitySystemComponent& InCaster, const FVeyraEffectFrame& Placement, TArray<FVeyraPreparedZone> InZones,
	double DelaySeconds, const FVeyraContentId& InAbility, int32 InCastId, TOptional<FVeyraPreparedLinger> InLinger)
{
	Caster = &InCaster;
	Zones = MoveTemp(InZones);
	Linger = MoveTemp(InLinger);
	bOriginIsCaster = Placement.bOriginIsCaster;
	Shapes.Reset();
	for (const FVeyraPreparedZone& Zone : Zones)
	{
		Shapes.Add(Zone.Shape);
	}
	Direction = Placement.Direction;
	ResolvesAt = GetWorld()->GetTimeSeconds() + DelaySeconds;
	Team = VeyraTeams::TeamOf(InCaster.GetOwner());
	Ability = InAbility;
	CastId = InCastId;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraDelayedArea, Shapes, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraDelayedArea, Direction, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraDelayedArea, ResolvesAt, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraDelayedArea, Team, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraDelayedArea, Ability, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraDelayedArea, CastId, this);
	GetWorldTimerManager().SetTimer(ResolveTimer, FTimerDelegate::CreateUObject(this, &AVeyraDelayedArea::Resolve), static_cast<float>(DelaySeconds), /*bLoop*/ false);
}

void AVeyraDelayedArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ResolveTimer);
	Super::EndPlay(EndPlayReason);
}

void AVeyraDelayedArea::Resolve()
{
	if (UAbilitySystemComponent* Source = Caster.Get())
	{
		FVeyraEffectFrame Placement;
		Placement.Origin = GetActorLocation();
		Placement.Direction = Direction;
		Placement.bOriginIsCaster = bOriginIsCaster;
		VeyraAreaDelivery::Resolve(*GetWorld(), *Source, Placement, Zones, FVeyraAbilityHitSource{ Ability, CastId });
		if (Linger.IsSet())
		{
			VeyraAreaDelivery::ArmLinger(*GetWorld(), *Source, Placement, Linger.GetValue());
		}
	}
	Destroy();
}
