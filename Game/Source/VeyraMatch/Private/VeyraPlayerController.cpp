// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraPlayerController.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerState.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraGameMode.h"
#include "VeyraMatchLog.h"
#include "VeyraVanguardCharacter.h"

AVeyraPlayerController::AVeyraPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The controller possesses nothing, so it chooses its own view target: the Vanguard.
	bAutoManageActiveCameraTarget = false;
	bShowMouseCursor = true;
}

void AVeyraPlayerController::IssueMoveOrder(const FVector& Destination)
{
	ServerIssueMoveOrder(Destination);
}

void AVeyraPlayerController::SteerMoveOrder(const FVector& Destination)
{
	ServerSteerMoveOrder(Destination);
}

void AVeyraPlayerController::IssueAttackOrder(AActor* Target)
{
	ServerIssueAttackOrder(Target);
}

void AVeyraPlayerController::IssueAttackMoveOrder(const FVector& Destination)
{
	ServerIssueAttackMoveOrder(Destination);
}

void AVeyraPlayerController::IssueCastOrder(EVeyraAbilitySlot Slot, AActor* Target)
{
	FVeyraCastTarget CastTarget;
	CastTarget.Actor = Target;
	ServerIssueCastOrder(Slot, CastTarget);
}

void AVeyraPlayerController::IssueCastOrder(EVeyraAbilitySlot Slot, const FVeyraCastTarget& Target)
{
	ServerIssueCastOrder(Slot, Target);
}

void AVeyraPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Only a local controller has run SetupInputComponent and built its mapping context.
	UEnhancedInputLocalPlayerSubsystem* Subsystem = IsLocalController() ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()) : nullptr;
	if (Subsystem && Input.MappingContext)
	{
		Subsystem->AddMappingContext(Input.MappingContext, /*Priority*/ 0);
	}
}

void AVeyraPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	Input = VeyraInput::Build(*GetDefault<UVeyraInputSettings>(), *this);
	if (UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(InputComponent))
	{
		Enhanced->BindAction(Input.MoveOrder, ETriggerEvent::Started, this, &AVeyraPlayerController::OnMoveOrderStarted);
		Enhanced->BindAction(Input.MoveOrder, ETriggerEvent::Triggered, this, &AVeyraPlayerController::OnMoveOrderHeld);
		Enhanced->BindAction(Input.AttackMove, ETriggerEvent::Triggered, this, &AVeyraPlayerController::OnAttackMovePressed);
		for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::All)
		{
			Enhanced->BindAction(Input.GetAbilityAction(Slot), ETriggerEvent::Triggered, this, &AVeyraPlayerController::OnAbilityPressed, Slot);
		}
	}
}

void AVeyraPlayerController::OnMoveOrderStarted()
{
	// On an enemy the button attacks it; anywhere else it moves (Settings Bible §1).
	AActor* Enemy = FindEnemyUnderCursor();
	bMoveOrderPressAttacked = Enemy != nullptr;
	if (Enemy)
	{
		IssueAttackOrder(Enemy);
		return;
	}
	MoveToCursor(/*bSteer*/ false);
}

void AVeyraPlayerController::OnMoveOrderHeld()
{
	// Holding the button keeps steering toward the cursor, paced below the server's order limit.
	if (!bMoveOrderPressAttacked
		&& GetWorld()->GetRealTimeSeconds() - LastHeldMoveOrderTime >= GetDefault<UVeyraInputSettings>()->HeldMoveOrderIntervalSeconds)
	{
		MoveToCursor(/*bSteer*/ true);
	}
}

void AVeyraPlayerController::OnAttackMovePressed()
{
	FHitResult Ground;
	if (GetHitResultUnderCursor(ECC_Visibility, /*bTraceComplex*/ false, Ground))
	{
		IssueAttackMoveOrder(Ground.Location);
	}
}

AActor* AVeyraPlayerController::FindEnemyUnderCursor() const
{
	FHitResult Unit;
	if (!GetHitResultUnderCursor(ECC_Pawn, /*bTraceComplex*/ false, Unit))
	{
		return nullptr;
	}
	AActor* Candidate = Unit.GetActor();
	return VeyraUnits::KindOf(Candidate).IsSet() && VeyraTargeting::AreHostile(PlayerState, Candidate) ? Candidate : nullptr;
}

void AVeyraPlayerController::MoveToCursor(bool bSteer)
{
	FHitResult Ground;
	if (GetHitResultUnderCursor(ECC_Visibility, /*bTraceComplex*/ false, Ground))
	{
		LastHeldMoveOrderTime = GetWorld()->GetRealTimeSeconds();
		if (bSteer)
		{
			SteerMoveOrder(Ground.Location);
		}
		else
		{
			IssueMoveOrder(Ground.Location);
		}
	}
}

void AVeyraPlayerController::OnAbilityPressed(EVeyraAbilitySlot Slot)
{
	// Quick Cast (Settings Bible §1.2): cast now, at the unit and the ground under the cursor. Each
	// ability uses what it needs, and the server decides whether it is valid.
	FVeyraCastTarget Target;
	FHitResult Unit;
	GetHitResultUnderCursor(ECC_Pawn, /*bTraceComplex*/ false, Unit);
	Target.Actor = Unit.GetActor();
	FHitResult Ground;
	if (GetHitResultUnderCursor(ECC_Visibility, /*bTraceComplex*/ false, Ground))
	{
		Target.bHasLocation = true;
		Target.Location = Ground.Location;
	}
	IssueCastOrder(Slot, Target);
}

void AVeyraPlayerController::ServerIssueCastOrder_Implementation(EVeyraAbilitySlot Slot, FVeyraCastTarget Target)
{
	if (!TakeOrderAllowance())
	{
		RejectOrder(EVeyraOrderRejection::TooFrequent);
		return;
	}
	AVeyraGameMode* GameMode = GetWorld()->GetAuthGameMode<AVeyraGameMode>();
	const EVeyraCastRejection Rejection = GameMode ? GameMode->HandleCastOrder(*this, Slot, Target) : EVeyraCastRejection::WrongPhase;
	if (Rejection != EVeyraCastRejection::None)
	{
		UE_LOG(LogVeyraMatch, Verbose, TEXT("Refused a cast from %s: %s."), *GetNameSafe(PlayerState), LexToString(Rejection));
		ClientCastRejected(Rejection);
	}
}

void AVeyraPlayerController::ClientCastRejected_Implementation(EVeyraCastRejection Rejection)
{
	LastCastRejection = Rejection;
	++CastRejectionCount;
	UE_LOG(LogVeyraMatch, Verbose, TEXT("The server refused a cast: %s."), LexToString(Rejection));
}

void AVeyraPlayerController::RequestDeveloperPause(bool bPause)
{
	ServerRequestDeveloperPause(bPause);
}

void AVeyraPlayerController::ServerRequestDeveloperPause_Implementation(bool bPause)
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogVeyraMatch, Warning, TEXT("Refused a developer pause request from %s: Shipping builds pause only by vote."), *GetNameSafe(PlayerState));
#else
	AVeyraGameMode* GameMode = GetWorld()->GetAuthGameMode<AVeyraGameMode>();
	if (GameMode && bPause)
	{
		GameMode->PauseMatch(*this);
	}
	else if (GameMode)
	{
		GameMode->ResumeMatch();
	}
#endif
}

void AVeyraPlayerController::RequestDeveloperEndMatch()
{
	ServerRequestDeveloperEndMatch();
}

void AVeyraPlayerController::ServerRequestDeveloperEndMatch_Implementation()
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogVeyraMatch, Warning, TEXT("Refused a developer end-match request from %s: Shipping builds end matches only by their rules."),
		*GetNameSafe(PlayerState));
#else
	if (AVeyraGameMode* GameMode = GetWorld()->GetAuthGameMode<AVeyraGameMode>())
	{
		UE_LOG(LogVeyraMatch, Log, TEXT("%s asked to end the match."), *GetNameSafe(PlayerState));
		GameMode->EndMatch(EVeyraMatchEndReason::DeveloperRequest);
	}
#endif
}

AVeyraVanguardCharacter* AVeyraPlayerController::GetVanguard() const
{
	return PlayerState ? Cast<AVeyraVanguardCharacter>(PlayerState->GetPawn()) : nullptr;
}

void AVeyraPlayerController::GetPlayerViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	if (IsLocalController())
	{
		Super::GetPlayerViewPoint(OutLocation, OutRotation);
		return;
	}

	const AActor* ViewPoint = GetVanguard();
	if (!ViewPoint)
	{
		ViewPoint = this;
	}
	OutLocation = ViewPoint->GetActorLocation();
	OutRotation = ViewPoint->GetActorRotation();
}

void AVeyraPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (PlayerState && IsLocalController())
	{
		PlayerState->OnPawnSet.AddUniqueDynamic(this, &AVeyraPlayerController::OnVanguardSet);
		if (APawn* Vanguard = PlayerState->GetPawn())
		{
			OnVanguardSet(PlayerState, Vanguard, nullptr);
		}
	}
}

void AVeyraPlayerController::OnVanguardSet(APlayerState* /*Participant*/, APawn* NewPawn, APawn* /*OldPawn*/)
{
	if (NewPawn)
	{
		SetViewTarget(NewPawn);
	}
}

void AVeyraPlayerController::ServerIssueMoveOrder_Implementation(FVector Destination)
{
	ApplyMoveOrder(Destination);
}

void AVeyraPlayerController::ServerIssueAttackOrder_Implementation(AActor* Target)
{
	if (!TakeOrderAllowance())
	{
		RejectOrder(EVeyraOrderRejection::TooFrequent);
		return;
	}
	AVeyraGameMode* GameMode = GetWorld()->GetAuthGameMode<AVeyraGameMode>();
	const EVeyraOrderRejection Rejection = GameMode ? GameMode->HandleAttackOrder(*this, Target) : EVeyraOrderRejection::WrongPhase;
	if (Rejection != EVeyraOrderRejection::None)
	{
		RejectOrder(Rejection);
	}
}

void AVeyraPlayerController::ServerIssueAttackMoveOrder_Implementation(FVector Destination)
{
	if (!TakeOrderAllowance())
	{
		RejectOrder(EVeyraOrderRejection::TooFrequent);
		return;
	}
	AVeyraGameMode* GameMode = GetWorld()->GetAuthGameMode<AVeyraGameMode>();
	const EVeyraOrderRejection Rejection = GameMode ? GameMode->HandleAttackMoveOrder(*this, Destination) : EVeyraOrderRejection::WrongPhase;
	if (Rejection != EVeyraOrderRejection::None)
	{
		RejectOrder(Rejection);
	}
}

void AVeyraPlayerController::ServerSteerMoveOrder_Implementation(FVector Destination)
{
	ApplyMoveOrder(Destination);
}

void AVeyraPlayerController::ApplyMoveOrder(const FVector& Destination)
{
	if (!TakeOrderAllowance())
	{
		RejectOrder(EVeyraOrderRejection::TooFrequent);
		return;
	}

	AVeyraGameMode* GameMode = GetWorld()->GetAuthGameMode<AVeyraGameMode>();
	const EVeyraOrderRejection Rejection = GameMode ? GameMode->HandleMoveOrder(*this, Destination) : EVeyraOrderRejection::WrongPhase;
	if (Rejection != EVeyraOrderRejection::None)
	{
		RejectOrder(Rejection);
	}
}

void AVeyraPlayerController::ClientOrderRejected_Implementation(EVeyraOrderRejection Rejection)
{
	LastOrderRejection = Rejection;
	++OrderRejectionCount;
	UE_LOG(LogVeyraMatch, Verbose, TEXT("The server refused an order: %s."), LexToString(Rejection));
}

void AVeyraPlayerController::RejectOrder(EVeyraOrderRejection Rejection)
{
	UE_LOG(LogVeyraMatch, Verbose, TEXT("Refused an order from %s: %s."), *GetNameSafe(PlayerState), LexToString(Rejection));
	ClientOrderRejected(Rejection);
}

bool AVeyraPlayerController::TakeOrderAllowance()
{
	// A token bucket holding one second of orders. Real time, so a pause never refills or drains it.
	const double Rate = UVeyraMatchTuningSubsystem::Get().Orders.MaxPerSecond;
	const double Now = GetWorld()->GetRealTimeSeconds();
	if (!bOrderAllowanceStarted)
	{
		OrderAllowance = Rate;
		bOrderAllowanceStarted = true;
	}
	else
	{
		OrderAllowance = FMath::Min(Rate, OrderAllowance + (Now - OrderAllowanceTime) * Rate);
	}
	OrderAllowanceTime = Now;

	constexpr double OneOrder = 1.0;
	if (OrderAllowance < OneOrder)
	{
		return false;
	}
	OrderAllowance -= OneOrder;
	return true;
}
