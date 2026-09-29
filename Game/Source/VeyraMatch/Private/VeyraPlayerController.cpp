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
#include "Shop/VeyraShopSubsystem.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraGameMode.h"
#include "VeyraMatchLog.h"
#include "VeyraPlayerState.h"
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

	FAutoConsoleCommandWithWorld SiegeCommand(TEXT("Veyra.Dev.Siege"),
		TEXT("Development builds: asks the server to destroy the next enemy structure in siege order, the Prime Well last (ADR-011 §15)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) {
			if (AVeyraPlayerController* Controller = ConsoleController(World))
			{
				Controller->RequestDeveloperSiege();
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

void AVeyraPlayerController::RequestRecall()
{
	ServerRecall();
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
		Enhanced->BindAction(Input.Recall, ETriggerEvent::Triggered, this, &AVeyraPlayerController::OnRecallPressed);
		for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::All)
		{
			Enhanced->BindAction(Input.GetAbilityAction(Slot), ETriggerEvent::Triggered, this, &AVeyraPlayerController::OnAbilityPressed, Slot);
		}
		for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::Items)
		{
			Enhanced->BindAction(Input.GetAbilityAction(Slot), ETriggerEvent::Triggered, this, &AVeyraPlayerController::OnAbilityPressed, Slot);
		}
		for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::Spells)
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

void AVeyraPlayerController::OnRecallPressed()
{
	RequestRecall();
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
	// With the rank-up modifier held, a kit slot's key spends a skill point on it instead.
	if (!VeyraAbilitySlots::IsItemSlot(Slot) && !VeyraAbilitySlots::IsSpellSlot(Slot) && IsInputKeyDown(GetDefault<UVeyraInputSettings>()->RankUpModifierKey))
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
	// An item slot's key uses its item (ADR-012 §1): a consumable through the shop, an Active as a cast.
	const int32 ItemIndex = VeyraAbilitySlots::ItemIndexOf(Slot);
	const EVeyraItemUse Use = PlayerState && ItemIndex != INDEX_NONE ? UVeyraShopSubsystem::GetUse(*PlayerState, ItemIndex) : EVeyraItemUse::None;
	if (Use == EVeyraItemUse::Consumable)
	{
		// This order has taken its allowance already.
		ApplyShopRequest([ItemIndex](UVeyraShopSubsystem& Shop, APlayerState& Participant) { return Shop.UseConsumable(Participant, ItemIndex); });
		return;
	}
	AVeyraGameMode* GameMode = GetWorld()->GetAuthGameMode<AVeyraGameMode>();
	EVeyraCastRejection Rejection = EVeyraCastRejection::UnknownAbility;
	if (ItemIndex == INDEX_NONE || Use == EVeyraItemUse::Active)
	{
		Rejection = GameMode ? GameMode->HandleCastOrder(GetPlayerState<AVeyraPlayerState>(), Slot, Target) : EVeyraCastRejection::WrongPhase;
	}
	if (Rejection == EVeyraCastRejection::None && Use == EVeyraItemUse::Active)
	{
		UVeyraShopSubsystem::NoteActiveUsed(*PlayerState, ItemIndex);
	}
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

void AVeyraPlayerController::RequestEndCustomMatch()
{
	ServerRequestEndCustomMatch();
}

void AVeyraPlayerController::ServerRequestEndCustomMatch_Implementation()
{
	AVeyraGameMode* GameMode = GetWorld()->GetAuthGameMode<AVeyraGameMode>();
	const EVeyraEndCustomMatchRefusal Refusal = GameMode ? GameMode->HandleEndCustomMatch(*this) : EVeyraEndCustomMatchRefusal::NotCustomMatch;
	if (Refusal != EVeyraEndCustomMatchRefusal::None)
	{
		ClientEndCustomMatchRefused(Refusal);
	}
}

void AVeyraPlayerController::ClientEndCustomMatchRefused_Implementation(EVeyraEndCustomMatchRefusal Refusal)
{
	LastEndCustomMatchRefusal = Refusal;
	++EndCustomMatchRefusalCount;
	UE_LOG(LogVeyraMatch, Verbose, TEXT("The server refused to end the custom match: %s."), LexToString(Refusal));
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

void AVeyraPlayerController::RequestBuyItem(const FVeyraContentId& Item)
{
	ServerBuyItem(Item);
}

void AVeyraPlayerController::RequestSellItem(int32 Slot)
{
	ServerSellItem(Slot);
}

void AVeyraPlayerController::RequestUndoPurchase()
{
	ServerUndoPurchase();
}

void AVeyraPlayerController::RequestCancelPurchase(int32 Index)
{
	ServerCancelPurchase(Index);
}

void AVeyraPlayerController::ServerBuyItem_Implementation(FVeyraContentId Item)
{
	RunShopRequest([&Item](UVeyraShopSubsystem& Shop, APlayerState& Participant) { return Shop.Buy(Participant, Item); });
}

void AVeyraPlayerController::ServerSellItem_Implementation(int32 Slot)
{
	RunShopRequest([Slot](UVeyraShopSubsystem& Shop, APlayerState& Participant) { return Shop.Sell(Participant, Slot); });
}

void AVeyraPlayerController::ServerUndoPurchase_Implementation()
{
	RunShopRequest([](UVeyraShopSubsystem& Shop, APlayerState& Participant) { return Shop.Undo(Participant); });
}

void AVeyraPlayerController::ServerCancelPurchase_Implementation(int32 Index)
{
	RunShopRequest([Index](UVeyraShopSubsystem& Shop, APlayerState& Participant) { return Shop.Cancel(Participant, Index); });
}

void AVeyraPlayerController::RequestSwapFluxSpell(int32 Slot, const FVeyraContentId& Spell)
{
	ServerSwapFluxSpell(Slot, Spell);
}

void AVeyraPlayerController::ServerSwapFluxSpell_Implementation(int32 Slot, FVeyraContentId Spell)
{
	RunShopRequest([Slot, &Spell](UVeyraShopSubsystem& Shop, APlayerState& Participant) { return Shop.SwapFluxSpell(Participant, Slot, Spell); });
}

void AVeyraPlayerController::RunShopRequest(TFunctionRef<EVeyraShopRefusal(UVeyraShopSubsystem& Shop, APlayerState& Participant)> Request)
{
	if (!TakeOrderAllowance())
	{
		RejectOrder(EVeyraOrderRejection::TooFrequent);
		return;
	}
	ApplyShopRequest(Request);
}

void AVeyraPlayerController::ApplyShopRequest(TFunctionRef<EVeyraShopRefusal(UVeyraShopSubsystem& Shop, APlayerState& Participant)> Request)
{
	const AVeyraGameMode* GameMode = GetWorld()->GetAuthGameMode<AVeyraGameMode>();
	UVeyraShopSubsystem* Shop = GetWorld()->GetSubsystem<UVeyraShopSubsystem>();
	const bool bAllowed = GameMode && GameMode->CheckShopAllowed() == EVeyraOrderRejection::None;
	const EVeyraShopRefusal Refusal = bAllowed && Shop && PlayerState ? Request(*Shop, *PlayerState) : EVeyraShopRefusal::NotNow;
	if (Refusal != EVeyraShopRefusal::None)
	{
		UE_LOG(LogVeyraMatch, Verbose, TEXT("Refused a shop request from %s: %s."), *GetNameSafe(PlayerState), LexToString(Refusal));
		ClientShopRefused(Refusal);
	}
}

void AVeyraPlayerController::ClientShopRefused_Implementation(EVeyraShopRefusal Refusal)
{
	LastShopRefusal = Refusal;
	++ShopRefusalCount;
	UE_LOG(LogVeyraMatch, Verbose, TEXT("The shop refused a request: %s."), LexToString(Refusal));
}

void AVeyraPlayerController::RequestDeveloperExperience(int32 Amount)
{
	ServerRequestDeveloperExperience(Amount);
}

void AVeyraPlayerController::RequestDeveloperLevels(int32 Levels)
{
	ServerRequestDeveloperLevels(Levels);
}

void AVeyraPlayerController::RequestDeveloperSiege()
{
	ServerRequestDeveloperSiege();
}

void AVeyraPlayerController::ServerRequestDeveloperSiege_Implementation()
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogVeyraMatch, Warning, TEXT("Refused a developer siege from %s: Shipping builds destroy structures only through play."), *GetNameSafe(PlayerState));
#else
	AVeyraGameMode* GameMode = GetWorld()->GetAuthGameMode<AVeyraGameMode>();
	const bool bFell = GameMode && GameMode->HandleDeveloperSiege(*this);
	UE_LOG(LogVeyraMatch, Log, TEXT("%s asked for a developer siege: %s."), *GetNameSafe(PlayerState), bFell ? TEXT("a structure fell") : TEXT("nothing fell"));
#endif
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
	double Needed = -Progression->GetExperience();
	for (int32 Level = Progression->GetLevel(); Level < Target; ++Level)
	{
		Needed += Tuning.Experience.ToNextLevel[Level - 1];
	}
	if (Needed > 0.0)
	{
		const int32 Gained = Progression->AddExperience(Needed);
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
	const EVeyraOrderRejection Rejection = GameMode ? GameMode->HandleAttackOrder(GetPlayerState<AVeyraPlayerState>(), Target) : EVeyraOrderRejection::WrongPhase;
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
	const EVeyraOrderRejection Rejection = GameMode ? GameMode->HandleAttackMoveOrder(GetPlayerState<AVeyraPlayerState>(), Destination) : EVeyraOrderRejection::WrongPhase;
	if (Rejection != EVeyraOrderRejection::None)
	{
		RejectOrder(Rejection);
	}
}

void AVeyraPlayerController::ServerRecall_Implementation()
{
	if (!TakeOrderAllowance())
	{
		RejectOrder(EVeyraOrderRejection::TooFrequent);
		return;
	}
	AVeyraGameMode* GameMode = GetWorld()->GetAuthGameMode<AVeyraGameMode>();
	const EVeyraOrderRejection Rejection = GameMode ? GameMode->HandleRecallOrder(GetPlayerState<AVeyraPlayerState>()) : EVeyraOrderRejection::WrongPhase;
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
	const EVeyraOrderRejection Rejection = GameMode ? GameMode->HandleMoveOrder(GetPlayerState<AVeyraPlayerState>(), Destination) : EVeyraOrderRejection::WrongPhase;
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
