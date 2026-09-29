// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tools/VeyraVisionToolComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Targeting/VeyraTargeting.h"
#include "TimerManager.h"
#include "Tuning/VeyraVisionTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraVisionLog.h"
#include "VeyraVisionSubsystem.h"

const TCHAR* LexToString(EVeyraVisionToolRejection Rejection)
{
	switch (Rejection)
	{
	case EVeyraVisionToolRejection::None:
		return TEXT("none");
	case EVeyraVisionToolRejection::NoVanguard:
		return TEXT("no living Vanguard");
	case EVeyraVisionToolRejection::CrowdControlled:
		return TEXT("crowd controlled");
	case EVeyraVisionToolRejection::NoCharge:
		return TEXT("no ward charge");
	case EVeyraVisionToolRejection::InvalidPoint:
		return TEXT("not a usable point");
	}
	return TEXT("unknown");
}

UVeyraVisionToolComponent::UVeyraVisionToolComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);
}

void UVeyraVisionToolComponent::InitializeComponent()
{
	Super::InitializeComponent();
	// Every Vanguard begins with Persistent Ward and all its charges (§3), from the moment it exists.
	if (GetOwnerRole() == ROLE_Authority)
	{
		RefillWardCharges();
	}
}

void UVeyraVisionToolComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RechargeTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void UVeyraVisionToolComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Only its owner needs its tool's state; a teammate learns of a ward by seeing it.
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_OwnerOnly;
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraVisionToolComponent, Equipped, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraVisionToolComponent, WardCharges, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraVisionToolComponent, NextChargeAt, Params);
}

EVeyraVisionToolRejection UVeyraVisionToolComponent::Use(const FVector& Point)
{
	APlayerState* Participant = Cast<APlayerState>(GetOwner());
	const APawn* Body = Participant ? Participant->GetPawn() : nullptr;
	if (!Body || !VeyraTargeting::IsAlive(Participant))
	{
		return EVeyraVisionToolRejection::NoVanguard;
	}
	if (Point.ContainsNaN())
	{
		return EVeyraVisionToolRejection::InvalidPoint;
	}
	// What stops a Vanguard casting stops it using its tool, as for an item's Active (ADR-016 §6).
	const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Participant);
	if (AbilitySystem && EnumHasAnyFlags(VeyraCombat::GetActionBlocks(*AbilitySystem), EVeyraActionBlocks::Cast))
	{
		return EVeyraVisionToolRejection::CrowdControlled;
	}
	switch (Equipped)
	{
	case EVeyraVisionTool::PersistentWard:
	{
		if (WardCharges <= 0)
		{
			return EVeyraVisionToolRejection::NoCharge;
		}
		// A point beyond its reach is brought back within it, as a cast's is (ADR-008 §9).
		const FVeyraPersistentWardTuning& Tuning = UVeyraVisionTuningSubsystem::Get().PersistentWard;
		const FVector From = Body->GetActorLocation();
		FVector2D Offset(Point.X - From.X, Point.Y - From.Y);
		if (Offset.Size() > Tuning.PlacementRange)
		{
			Offset = Offset.GetSafeNormal() * Tuning.PlacementRange;
		}
		// It stands on the ground its Vanguard stands on.
		const double Ground = From.Z - Body->GetSimpleCollisionHalfHeight();
		const FVector Where(From.X + Offset.X, From.Y + Offset.Y, Ground + Tuning.BodyHalfHeight);
		UVeyraVisionSubsystem* Vision = GetWorld()->GetSubsystem<UVeyraVisionSubsystem>();
		if (!Vision || !Vision->PlaceWard(*Participant, Where))
		{
			return EVeyraVisionToolRejection::InvalidPoint;
		}
		SetWardCharges(WardCharges - 1);
		ScheduleRecharge();
		return EVeyraVisionToolRejection::None;
	}
	}
	return EVeyraVisionToolRejection::InvalidPoint;
}

void UVeyraVisionToolComponent::RefillWardCharges()
{
	if (Equipped != EVeyraVisionTool::PersistentWard)
	{
		return;
	}
	GetWorld()->GetTimerManager().ClearTimer(RechargeTimer);
	SetWardCharges(UVeyraVisionTuningSubsystem::Get().WardCharges.Max);
	SetNextChargeAt(-1.0);
}

void UVeyraVisionToolComponent::ScheduleRecharge()
{
	// One charge comes back at a time, while fewer than the most are carried (§4).
	UWorld* World = GetWorld();
	if (!World || World->GetTimerManager().IsTimerActive(RechargeTimer) || WardCharges >= UVeyraVisionTuningSubsystem::Get().WardCharges.Max)
	{
		return;
	}
	const double Seconds = UVeyraVisionTuningSubsystem::Get().PersistentWard.RechargeSeconds;
	World->GetTimerManager().SetTimer(RechargeTimer, FTimerDelegate::CreateUObject(this, &UVeyraVisionToolComponent::OnWardCharged),
		static_cast<float>(Seconds), /*bLoop*/ false);
	SetNextChargeAt(World->GetTimeSeconds() + Seconds);
}

void UVeyraVisionToolComponent::OnWardCharged()
{
	// Inside its own callback the finished timer still counts as active; forget it, so the next can start.
	RechargeTimer.Invalidate();
	SetWardCharges(FMath::Min(WardCharges + 1, UVeyraVisionTuningSubsystem::Get().WardCharges.Max));
	SetNextChargeAt(-1.0);
	ScheduleRecharge();
}

void UVeyraVisionToolComponent::SetWardCharges(int32 NewCharges)
{
	if (WardCharges != NewCharges)
	{
		WardCharges = NewCharges;
		MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraVisionToolComponent, WardCharges, this);
	}
}

void UVeyraVisionToolComponent::SetNextChargeAt(double NewNextChargeAt)
{
	if (NextChargeAt != NewNextChargeAt)
	{
		NextChargeAt = NewNextChargeAt;
		MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraVisionToolComponent, NextChargeAt, this);
	}
}
