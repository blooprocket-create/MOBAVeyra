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
#include "Teams/VeyraTeam.h"
#include "Terrain/VeyraGround.h"
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
	case EVeyraVisionToolRejection::CoolingDown:
		return TEXT("cooling down");
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
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraVisionToolComponent, SweeperReadyAt, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraVisionToolComponent, QuickSightReadyAt, Params);
}

double UVeyraVisionToolComponent::GetReadyAt(EVeyraVisionTool Tool) const
{
	switch (Tool)
	{
	case EVeyraVisionTool::PersistentWard:
		return NextChargeAt;
	case EVeyraVisionTool::Sweeper:
		return SweeperReadyAt;
	case EVeyraVisionTool::QuickSight:
		return QuickSightReadyAt;
	}
	return 0.0;
}

void UVeyraVisionToolComponent::Equip(EVeyraVisionTool Tool)
{
	if (Equipped != Tool)
	{
		Equipped = Tool;
		MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraVisionToolComponent, Equipped, this);
	}
	if (Tool == EVeyraVisionTool::PersistentWard)
	{
		RefillWardCharges();
		return;
	}
	// Charges come back only while Persistent Ward is in the slot (§4).
	GetWorld()->GetTimerManager().ClearTimer(RechargeTimer);
	SetNextChargeAt(-1.0);
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
		// It stands on the ground where it is placed, however far that rises or falls from its Vanguard's (ADR-040 §4);
		// without ground there, on the ground its Vanguard stands on.
		const FVector Feet(From.X + Offset.X, From.Y + Offset.Y, From.Z - Body->GetSimpleCollisionHalfHeight());
		FVector Surface = Feet;
		VeyraGround::Under(*GetWorld(), Feet, Surface);
		const FVector Where = Surface + FVector::UpVector * Tuning.BodyHalfHeight;
		UVeyraVisionSubsystem* Vision = GetWorld()->GetSubsystem<UVeyraVisionSubsystem>();
		if (!Vision || !Vision->PlaceWard(*Participant, Where))
		{
			return EVeyraVisionToolRejection::InvalidPoint;
		}
		SetWardCharges(WardCharges - 1);
		ScheduleRecharge();
		return EVeyraVisionToolRejection::None;
	}
	case EVeyraVisionTool::Sweeper:
	{
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now < SweeperReadyAt)
		{
			return EVeyraVisionToolRejection::CoolingDown;
		}
		const FVeyraSweeperTuning& Tuning = UVeyraVisionTuningSubsystem::Get().Sweeper;
		UVeyraVisionSubsystem* Vision = GetWorld()->GetSubsystem<UVeyraVisionSubsystem>();
		if (!Vision)
		{
			return EVeyraVisionToolRejection::InvalidPoint;
		}
		Vision->AddTrueSight(VeyraTeams::TeamOf(Participant), *Body, Tuning.Radius, Tuning.DurationSeconds);
		SweeperReadyAt = Now + Tuning.CooldownSeconds;
		MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraVisionToolComponent, SweeperReadyAt, this);
		return EVeyraVisionToolRejection::None;
	}
	case EVeyraVisionTool::QuickSight:
	{
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now < QuickSightReadyAt)
		{
			return EVeyraVisionToolRejection::CoolingDown;
		}
		// A point beyond its reach is brought back within it, as a cast's is (ADR-008 §9).
		const FVeyraQuickSightTuning& Tuning = UVeyraVisionTuningSubsystem::Get().QuickSight;
		const FVector From = Body->GetActorLocation();
		FVector2D Offset(Point.X - From.X, Point.Y - From.Y);
		if (Offset.Size() > Tuning.Range)
		{
			Offset = Offset.GetSafeNormal() * Tuning.Range;
		}
		UVeyraVisionSubsystem* Vision = GetWorld()->GetSubsystem<UVeyraVisionSubsystem>();
		if (!Vision)
		{
			return EVeyraVisionToolRejection::InvalidPoint;
		}
		Vision->RevealArea(VeyraTeams::TeamOf(Participant), VeyraGround::Carried(*GetWorld(), From, FVector2D(From.X + Offset.X, From.Y + Offset.Y)), Tuning.Radius,
			Tuning.DurationSeconds);
		QuickSightReadyAt = Now + Tuning.CooldownSeconds;
		MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraVisionToolComponent, QuickSightReadyAt, this);
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
