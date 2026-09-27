// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraPlayerController.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraGameMode.h"
#include "VeyraMatchLog.h"
#include "VeyraVanguardCharacter.h"

#if !UE_BUILD_SHIPPING
namespace
{
	/** The world's local player's controller, which a console command speaks for. */
	AVeyraPlayerController* ConsoleController(const UWorld* World)
	{
		return World ? Cast<AVeyraPlayerController>(World->GetFirstPlayerController()) : nullptr;
	}

	// Developer XP until minions give it (ADR-008 §6). The server refuses them in Shipping.
	FAutoConsoleCommandWithWorldAndArgs GrantExperienceCommand(TEXT("Veyra.Dev.GrantXp"),
		TEXT("Development builds: asks the server to give your Vanguard this much XP. Usage: Veyra.Dev.GrantXp <amount>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) {
			AVeyraPlayerController* Controller = ConsoleController(World);
			if (Controller && Args.Num() == 1)
			{
				Controller->RequestDeveloperExperience(FCString::Atoi(*Args[0]));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GrantLevelsCommand(TEXT("Veyra.Dev.GrantLevels"),
		TEXT("Development builds: asks the server for enough XP to raise your Vanguard this many levels. Usage: Veyra.Dev.GrantLevels <levels>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) {
			AVeyraPlayerController* Controller = ConsoleController(World);
			if (Controller && Args.Num() == 1)
			{
				Controller->RequestDeveloperLevels(FCString::Atoi(*Args[0]));
			}
		}));
}
#endif

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
	// With the rank-up modifier held, the slot's key spends a skill point on it instead.
	if (IsInputKeyDown(GetDefault<UVeyraInputSettings>()->RankUpModifierKey))
	{
		RequestRankUp(Slot);
		return;
	}

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

void AVeyraPlayerController::RequestRankUp(EVeyraAbilitySlot Slot)
{
	ServerRankUp(Slot);
}

void AVeyraPlayerController::ServerRankUp_Implementation(EVeyraAbilitySlot Slot)
{
	if (!TakeOrderAllowance())
	{
		RejectOrder(EVeyraOrderRejection::TooFrequent);
		return;
	}
	const AVeyraGameMode* GameMode = GetWorld()->GetAuthGameMode<AVeyraGameMode>();
	const EVeyraOrderRejection Allowed = GameMode ? GameMode->CheckRankUpAllowed() : EVeyraOrderRejection::WrongPhase;
	if (Allowed != EVeyraOrderRejection::None)
	{
		RejectOrder(Allowed);
		return;
	}
	UVeyraProgressionComponent* Progression = PlayerState ? PlayerState->FindComponentByClass<UVeyraProgressionComponent>() : nullptr;
	const EVeyraRankRefusal Refusal = Progression ? Progression->AllocateRank(Slot) : EVeyraRankRefusal::NotInitialized;
	if (Refusal != EVeyraRankRefusal::None)
	{
		UE_LOG(LogVeyraMatch, Verbose, TEXT("Refused a rank-up from %s: %s."), *GetNameSafe(PlayerState), LexToString(Refusal));
		ClientRankUpRefused(Refusal);
	}
}

void AVeyraPlayerController::ClientRankUpRefused_Implementation(EVeyraRankRefusal Refusal)
{
	LastRankUpRefusal = Refusal;
	++RankUpRefusalCount;
	UE_LOG(LogVeyraMatch, Verbose, TEXT("The server refused a rank-up: %s."), LexToString(Refusal));
}

void AVeyraPlayerController::RequestDeveloperExperience(int32 Amount)
{
	ServerRequestDeveloperExperience(Amount);
}

void AVeyraPlayerController::RequestDeveloperLevels(int32 Levels)
{
	ServerRequestDeveloperLevels(Levels);
}

void AVeyraPlayerController::ServerRequestDeveloperExperience_Implementation(int32 Amount)
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogVeyraMatch, Warning, TEXT("Refused a developer XP request from %s: Shipping builds grant XP only through play."), *GetNameSafe(PlayerState));
#else
	UVeyraProgressionComponent* Progression = FindDeveloperProgression();
	if (Progression && Amount > 0)
	{
		const int32 Gained = Progression->AddExperience(Amount);
		UE_LOG(LogVeyraMatch, Log, TEXT("%s took %d developer XP and gained %d level(s)."), *GetNameSafe(PlayerState), Amount, Gained);
	}
#endif
}

void AVeyraPlayerController::ServerRequestDeveloperLevels_Implementation(int32 Levels)
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogVeyraMatch, Warning, TEXT("Refused a developer level request from %s: Shipping builds grant XP only through play."), *GetNameSafe(PlayerState));
#else
	UVeyraProgressionComponent* Progression = FindDeveloperProgression();
	if (!Progression || Levels <= 0)
	{
		return;
	}
	// The XP from here to the level Levels above this one, or to the cap.
	const FVeyraProgressionTuning& Tuning = UVeyraProgressionTuningSubsystem::Get();
	const int32 Target = FMath::Min(Progression->GetLevel() + Levels, Tuning.MaxLevel);
	int64 Needed = -static_cast<int64>(Progression->GetExperience());
	for (int32 Level = Progression->GetLevel(); Level < Target; ++Level)
	{
		Needed += Tuning.Experience.ToNextLevel[Level - 1];
	}
	if (Needed > 0)
	{
		const int32 Gained = Progression->AddExperience(static_cast<int32>(FMath::Min<int64>(Needed, MAX_int32)));
		UE_LOG(LogVeyraMatch, Log, TEXT("%s took developer XP for %d level(s)."), *GetNameSafe(PlayerState), Gained);
	}
#endif
}

UVeyraProgressionComponent* AVeyraPlayerController::FindDeveloperProgression() const
{
	const AVeyraGameMode* GameMode = GetWorld()->GetAuthGameMode<AVeyraGameMode>();
	if (!GameMode || GameMode->CheckRankUpAllowed() != EVeyraOrderRejection::None)
	{
		return nullptr;
	}
	UVeyraProgressionComponent* Progression = PlayerState ? PlayerState->FindComponentByClass<UVeyraProgressionComponent>() : nullptr;
	return Progression && Progression->IsInitialized() ? Progression : nullptr;
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
