// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraPlayerController.h"

#include "Camera/VeyraCameraPreferences.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Input/VeyraControlPreferences.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Camera/VeyraCameraRig.h"
#include "Developer/VeyraDeveloperCommandRoute.h"
#include "Engine/Console.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Ending/VeyraMatchEnding.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerState.h"
#include "Input/VeyraCameraSettings.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Pings/VeyraPingRules.h"
#include "Chat/VeyraChatRules.h"
#include "Chat/VeyraChatSubsystem.h"
#include "Pings/VeyraPingSubsystem.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Structures/VeyraStructure.h"
#include "Rewards/VeyraEconomyTuningSubsystem.h"
#include "Shop/VeyraShopSubsystem.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraGameMode.h"
#include "VeyraGameState.h"
#include "VeyraMatchLog.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardCharacter.h"
#include "Votes/VeyraVoteSubsystem.h"
#include "VeyraSettingsSubsystem.h"

AVeyraPlayerController::AVeyraPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The controller possesses nothing, so it chooses its own view target: its camera rig.
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
	ServerIssueAttackMoveOrder(Destination, ControlPreferences().AttackMoveTarget);
}

bool AVeyraPlayerController::CancelPendingCast()
{
	const bool bWasWaiting = bAttackMoveWaiting;
	bAttackMoveWaiting = false;
	return CastInput.Cancel().Step != EVeyraCastStep::Nothing || bWasWaiting;
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

namespace
{
	/** The All Chat preference (Chat & Communication Bible §2; ADR-029 §4). The registry's tests keep it real. */
	const FVeyraContentId& AllChatSetting()
	{
		static const FVeyraContentId Id = FVeyraContentId::FromText(TEXT("communication_all_chat")).GetValue();
		return Id;
	}
}

void AVeyraPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// The local player's own view (ADR-020 §1); nothing else sees it.
	if (IsLocalController())
	{
		FActorSpawnParameters Parameters;
		Parameters.Owner = this;
		Parameters.ObjectFlags |= RF_Transient;
		CameraRig = GetWorld()->SpawnActor<AVeyraCameraRig>(Parameters);
		// The mode the player left the camera in last match (SET-5).
		CameraRig->SetMode(CameraPreferences().DefaultMode);
		if (APawn* Vanguard = GetVanguard())
		{
			OnVanguardSet(PlayerState, Vanguard, nullptr);
		}
	}

	// Only a local controller has run SetupInputComponent and built its mapping context.
	UEnhancedInputLocalPlayerSubsystem* Subsystem = IsLocalController() ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()) : nullptr;
	if (Subsystem && Input.MappingContext)
	{
		Subsystem->AddMappingContext(Input.MappingContext, /*Priority*/ 0);
	}
	// A binding changed in Settings, in the shell or during the match, applies at once (ADR-024 §6).
	if (UVeyraSettingsSubsystem* Settings = IsLocalController() ? UVeyraSettingsSubsystem::Get(this) : nullptr)
	{
		SettingsHandle = Settings->GetStore().OnChanged.AddUObject(this, &AVeyraPlayerController::OnPlayerSettingChanged);
	}
	// As the player joins the match, and again whenever they change it.
	ReportAllChat();
}

const UVeyraInputSettings& AVeyraPlayerController::GetKeys() const
{
	return PlayerKeys ? *PlayerKeys : *GetDefault<UVeyraInputSettings>();
}

void AVeyraPlayerController::OnPlayerSettingChanged(const FVeyraContentId& Id)
{
	const UVeyraSettingsSubsystem* Settings = UVeyraSettingsSubsystem::Get(this);
	if (Settings && Settings->GetStore().GetRegistry().Bindings.Contains(Id))
	{
		RefreshKeys();
	}
	if (Id == AllChatSetting())
	{
		ReportAllChat();
	}
}

void AVeyraPlayerController::RefreshKeys()
{
	// A fresh copy takes the developer's keys from the class defaults; the player's go over them.
	PlayerKeys = NewObject<UVeyraInputSettings>(this, NAME_None, RF_Transient);
	if (const UVeyraSettingsSubsystem* Settings = UVeyraSettingsSubsystem::Get(this))
	{
		VeyraSettings::ApplyBindings(*PlayerKeys, Settings->GetStore());
	}
	if (!Input.MappingContext)
	{
		// The first time, Build maps the actions.
		return;
	}
	UInputMappingContext* Previous = Input.MappingContext;
	Input.MappingContext = VeyraInput::MapKeys(*PlayerKeys, Input, *this);
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = IsLocalController() ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()) : nullptr)
	{
		Subsystem->RemoveMappingContext(Previous);
		Subsystem->AddMappingContext(Input.MappingContext, /*Priority*/ 0);
	}
}

void AVeyraPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UVeyraSettingsSubsystem* Settings = UVeyraSettingsSubsystem::Get(this))
	{
		Settings->GetStore().OnChanged.Remove(SettingsHandle);
	}
	SettingsHandle.Reset();
	if (CameraRig)
	{
		CameraRig->Destroy();
		CameraRig = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AVeyraPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	RefreshKeys();
	Input = VeyraInput::Build(GetKeys(), *this);
	if (UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(InputComponent))
	{
		Enhanced->BindAction(Input.MoveOrder, ETriggerEvent::Started, this, &AVeyraPlayerController::OnMoveOrderStarted);
		Enhanced->BindAction(Input.MoveOrder, ETriggerEvent::Triggered, this, &AVeyraPlayerController::OnMoveOrderHeld);
		Enhanced->BindAction(Input.AttackMove, ETriggerEvent::Triggered, this, &AVeyraPlayerController::OnAttackMovePressed);
		Enhanced->BindAction(Input.Recall, ETriggerEvent::Triggered, this, &AVeyraPlayerController::OnRecallPressed);
		Enhanced->BindAction(Input.VoteYes, ETriggerEvent::Triggered, this, &AVeyraPlayerController::OnVoteYesPressed);
		Enhanced->BindAction(Input.VoteNo, ETriggerEvent::Triggered, this, &AVeyraPlayerController::OnVoteNoPressed);
		Enhanced->BindAction(Input.MasteryEmote, ETriggerEvent::Triggered, this, &AVeyraPlayerController::OnMasteryEmotePressed);
		// Each ability's key reports its press and its release, which its casting mode reads (ADR-041 §1).
		const auto BindAbility = [this, Enhanced](EVeyraAbilitySlot Slot) {
			Enhanced->BindAction(Input.GetAbilityAction(Slot), ETriggerEvent::Started, this, &AVeyraPlayerController::OnAbilityPressed, Slot);
			Enhanced->BindAction(Input.GetAbilityAction(Slot), ETriggerEvent::Completed, this, &AVeyraPlayerController::OnAbilityReleased, Slot);
		};
		for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::All)
		{
			BindAbility(Slot);
		}
		for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::Items)
		{
			BindAbility(Slot);
		}
		for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::Spells)
		{
			BindAbility(Slot);
		}
		BindAbility(EVeyraAbilitySlot::VisionTool);
	}
}

void AVeyraPlayerController::OnMoveOrderStarted()
{
	// It cancels a waiting cast or attack-move, and still gives its order (ADR-041 §1, §4).
	CastInput.Cancel();
	bAttackMoveWaiting = false;
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
	// Attack Move: its key, then a click (Settings Bible §1.3). Whatever cast waited gives way.
	CastInput.Cancel();
	bAttackMoveWaiting = true;
}

void AVeyraPlayerController::AttackMoveToCursor()
{
	// A point on the minimap is where the order goes (Settings Bible §3.2), as a right click there moves.
	if (const TOptional<FVector> OnMap = MinimapPointUnderCursor(EMinimapClick::Ping))
	{
		IssueAttackMoveOrder(OnMap.GetValue());
		return;
	}
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

void AVeyraPlayerController::OnMasteryEmotePressed()
{
	RequestMasteryEmote();
}

void AVeyraPlayerController::RequestMasteryEmote()
{
	ServerMasteryEmote();
}

void AVeyraPlayerController::ServerMasteryEmote_Implementation()
{
	AVeyraPlayerState* Participant = GetPlayerState<AVeyraPlayerState>();
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	if (!Participant || !GameState)
	{
		return;
	}
	// World time, so a pause holds both the emote and its cooldown.
	const double Now = GameState->GetServerWorldTimeSeconds();
	const FVeyraMasteryEmoteTuning& Emote = UVeyraMatchTuningSubsystem::Get().MasteryEmote;
	if (!Participant->TryShowMasteryEmote(Now, Emote))
	{
		return;
	}
	UE_LOG(LogVeyraMatch, Log, TEXT("%s shows the mastery emote: Mastery %d, tier %d."), *Participant->GetPlayerName(), Participant->GetMasteryLevel(),
		Participant->GetEmoteTier());
}

void AVeyraPlayerController::OnVoteYesPressed()
{
	CastVote(true);
}

void AVeyraPlayerController::OnVoteNoPressed()
{
	CastVote(false);
}

void AVeyraPlayerController::ClientAbsenceWarning_Implementation(bool bAfk)
{
	bWarnedAfk = bAfk;
	UE_LOG(LogVeyraMatch, Log, TEXT("%s"), bAfk ? TEXT("The server counts this player AFK; the Vanguard walks to safety.") : TEXT("This player is back in control."));
}

AActor* AVeyraPlayerController::FindEnemyUnderCursor() const
{
	return VeyraCursorPicks::Enemy(UnitsUnderCursor(), IsTargetingVanguardsOnly());
}

TArray<FVeyraCursorUnit> AVeyraPlayerController::UnitsUnderCursor() const
{
	TArray<FVeyraCursorUnit> Under;
	FVector Origin;
	FVector Direction;
	if (!DeprojectMousePositionToWorld(Origin, Direction))
	{
		return Under;
	}
	// As the cursor's own trace would, but past each unit it meets, until the ground or a wall stops it.
	FCollisionQueryParams Query(SCENE_QUERY_STAT(VeyraCursorUnits), /*bTraceComplex*/ false);
	const FVector End = Origin + Direction * HitResultTraceDistance;
	FHitResult Hit;
	while (GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, ECC_Pawn, Query))
	{
		AActor* Actor = Hit.GetActor();
		const TOptional<EVeyraUnitKind> Kind = VeyraUnits::KindOf(Actor);
		if (!Kind)
		{
			break;
		}
		Under.Add(FVeyraCursorUnit{ Actor, Kind.GetValue(), VeyraTargeting::AreHostile(PlayerState, Actor) });
		Query.AddIgnoredActor(Actor);
	}
	return Under;
}

bool AVeyraPlayerController::IsTargetingVanguardsOnly() const
{
	return ControlPreferences().bTargetVanguardsToggles ? bTargetVanguardsToggled : IsInputKeyDown(GetKeys().TargetVanguardsOnlyKey);
}

bool AVeyraPlayerController::ShouldSelfCast(EVeyraAbilitySlot Slot, TConstArrayView<FVeyraCursorUnit> Under) const
{
	const FVeyraAbilitiesTuning& Tuning = UVeyraAbilitiesTuningSubsystem::Get();
	const FVeyraContentId Ability = AbilityIn(Slot);
	if (!Ability.IsValid() || !VeyraAbilityRules::AcceptsAllyTarget(Tuning, Ability))
	{
		return false;
	}
	if (IsInputKeyDown(GetKeys().SelfCastKey))
	{
		return true;
	}
	if (!ControlPreferences().SmartSelfCast.Contains(Slot))
	{
		return false;
	}
	// From the body that casts it: the Vanguard, or the Echo it commands (ADR-050 §6).
	const APawn* Body = GetCommandedBody();
	const FVeyraCastTuning* Cast = VeyraAbilityRules::FindCast(Tuning, Ability);
	return !Body || !Cast || VeyraCursorPicks::SmartSelfCasts(*Body, Under, Cast->CastRange);
}

FVeyraContentId AVeyraPlayerController::AbilityIn(EVeyraAbilitySlot Slot) const
{
	const UVeyraAbilityLoadoutComponent* Loadout = PlayerState ? PlayerState->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
	const FVeyraLoadoutEntry* Entry = Loadout ? Loadout->FindSlot(Slot) : nullptr;
	return Entry ? Entry->Ability : FVeyraContentId();
}

FVeyraSlotNow AVeyraPlayerController::SlotNow(EVeyraAbilitySlot Slot) const
{
	FVeyraSlotNow Now;
	const APawn* Body = GetVanguard();
	Now.bCasterAlive = Body && VeyraTargeting::IsAlive(Body);
	Now.Ability = AbilityIn(Slot);
	const UVeyraAbilityLoadoutComponent* Loadout = PlayerState ? PlayerState->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
	const UVeyraCooldownComponent* Cooldowns = PlayerState ? PlayerState->FindComponentByClass<UVeyraCooldownComponent>() : nullptr;
	Now.bLocked = Loadout && Loadout->IsLocked(Slot);
	if (Loadout && Cooldowns && Now.Ability.IsValid())
	{
		// A shared cooldown counts under the ability it is shared with, as the HUD shows it.
		Now.CooldownSeconds = Cooldowns->GetRemainingSecondsNow(Loadout->CooldownIdOf(Now.Ability));
	}
	return Now;
}

TOptional<FVector> AVeyraPlayerController::MinimapPointUnderCursor(EMinimapClick Purpose) const
{
	FVector2D Mouse;
	return MinimapHitTest && GetMousePosition(Mouse.X, Mouse.Y) ? MinimapHitTest(Mouse, Purpose) : TOptional<FVector>();
}

void AVeyraPlayerController::MoveToCursor(bool bSteer)
{
	// A right click on the minimap moves the Vanguard to where it points (Settings Bible §3.2).
	if (const TOptional<FVector> OnMap = MinimapPointUnderCursor(EMinimapClick::Move))
	{
		LastHeldMoveOrderTime = GetWorld()->GetRealTimeSeconds();
		IssueMoveOrder(OnMap.GetValue());
		return;
	}
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
	if (!VeyraAbilitySlots::IsItemSlot(Slot) && !VeyraAbilitySlots::IsSpellSlot(Slot) && !VeyraAbilitySlots::IsVisionToolSlot(Slot)
		&& IsInputKeyDown(GetKeys().RankUpModifierKey))
	{
		RequestRankUp(Slot);
		return;
	}
	const bool bPreview = IsInputKeyDown(GetKeys().ShowCastRangeKey);
	bAttackMoveWaiting = false;
	ApplyCastStep(CastInput.Press(Slot, ControlPreferences().CastModeOf(Slot), bPreview, AbilityIn(Slot)));
}

void AVeyraPlayerController::OnAbilityReleased(EVeyraAbilitySlot Slot)
{
	ApplyCastStep(CastInput.Release(Slot));
}

void AVeyraPlayerController::TickCastInput()
{
	if (WasInputKeyJustPressed(GetKeys().TargetVanguardsOnlyKey))
	{
		// Read only while its mode is Toggle.
		bTargetVanguardsToggled = !bTargetVanguardsToggled;
	}
	// Attack Move Click: one press toward the cursor (Settings Bible §1.3).
	if (WasInputKeyJustPressed(GetKeys().AttackMoveClickKey))
	{
		bAttackMoveWaiting = false;
		AttackMoveToCursor();
	}
	// A waiting Attack Move takes the Select Click; a ping's click stays the ping's.
	if (bAttackMoveWaiting && WasInputKeyJustPressed(GetKeys().SelectKey) && !IsPinging())
	{
		bAttackMoveWaiting = false;
		AttackMoveToCursor();
		return;
	}
	const TOptional<FVeyraCastIndicator>& Shown = CastInput.GetIndicator();
	if (!Shown)
	{
		return;
	}
	const UVeyraInputSettings& Keys = GetKeys();
	if (Shown->bPreviewOnly)
	{
		if (!IsInputKeyDown(Keys.ShowCastRangeKey))
		{
			ApplyCastStep(CastInput.EndPreview());
		}
		return;
	}
	// An ability that can no longer be cast gives up its waiting cast (ADR-041 §1).
	if (CastInput.Recheck(SlotNow(Shown->Slot)).Step != EVeyraCastStep::Nothing)
	{
		return;
	}
	// A click that pings, or lands on the minimap, is theirs and leaves the cast waiting.
	if (WasInputKeyJustPressed(Keys.SelectKey) && !IsPinging() && !MinimapPointUnderCursor(EMinimapClick::Ping))
	{
		ApplyCastStep(CastInput.Confirm());
	}
}

void AVeyraPlayerController::ApplyCastStep(const FVeyraCastOutcome& Outcome)
{
	if (Outcome.Step == EVeyraCastStep::CastNow)
	{
		CastAtCursor(Outcome.Slot);
	}
}

void AVeyraPlayerController::CastAtCursor(EVeyraAbilitySlot Slot)
{
	// Each ability uses what it needs of the unit and the ground under the cursor, and the server decides
	// whether it is valid.
	FVeyraCastTarget Target;
	const TArray<FVeyraCursorUnit> Under = UnitsUnderCursor();
	const FVeyraContentId Ability = AbilityIn(Slot);
	const bool bNamesAlly = Ability.IsValid() && VeyraAbilityRules::AcceptsAllyTarget(UVeyraAbilitiesTuningSubsystem::Get(), Ability);
	Target.Actor = ShouldSelfCast(Slot, Under) ? GetCommandedBody() : VeyraCursorPicks::ForCast(Under, IsTargetingVanguardsOnly(), bNamesAlly);
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
	// The vision tool's key orders Vision's tool, which is no ability (ADR-016 §6).
	if (VeyraAbilitySlots::IsVisionToolSlot(Slot))
	{
		const EVeyraOrderRejection Refusal = !Target.bHasLocation ? EVeyraOrderRejection::InvalidOrder
			: GameMode ? GameMode->HandleVisionToolOrder(GetPlayerState<AVeyraPlayerState>(), Target.Location) : EVeyraOrderRejection::WrongPhase;
		if (Refusal != EVeyraOrderRejection::None)
		{
			RejectOrder(Refusal);
		}
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

void AVeyraPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraPlayerController, TeamVote, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraPlayerController, CommandedUnit, Params);
}

APawn* AVeyraPlayerController::GetCommandedBody() const
{
	return CommandedUnit ? CommandedUnit.Get() : GetVanguard();
}

void AVeyraPlayerController::SetCommandedUnit(APawn* Unit)
{
	if (CommandedUnit == Unit)
	{
		return;
	}
	CommandedUnit = Unit;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraPlayerController, CommandedUnit, this);
}

const FVeyraVoteState& AVeyraPlayerController::GetOpenVote() const
{
	const AVeyraGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AVeyraGameState>() : nullptr;
	return GameState && GameState->GetVote().bOpen ? GameState->GetVote() : TeamVote;
}

void AVeyraPlayerController::SetTeamVote(const FVeyraVoteState& InVote)
{
	if (TeamVote == InVote)
	{
		return;
	}
	TeamVote = InVote;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraPlayerController, TeamVote, this);
}

void AVeyraPlayerController::RequestVote(EVeyraVoteKind Kind)
{
	ServerRequestVote(Kind);
}

void AVeyraPlayerController::CastVote(bool bYes)
{
	ServerCastVote(bYes);
}

bool AVeyraPlayerController::TickEndPan(double /*DeltaSeconds*/)
{
	const AVeyraGameState* GameState = GetWorld()->GetGameState<AVeyraGameState>();
	if (!GameState || GameState->GetPhase() != EVeyraMatchPhase::Ended)
	{
		return false;
	}
	// The one game-driven pan (ADR-020 §1): play is over, so the player's camera input waits.
	const double Now = GetWorld()->GetRealTimeSeconds();
	FEndPan& Pan = EndPan.IsSet() ? EndPan.GetValue() : EndPan.Emplace();
	if (!Pan.To.IsSet())
	{
		Pan.From = CameraRig->GetFocus();
		Pan.StartedAt = Now;
		// The Well's fall may reach this client a moment after the match's end does.
		if (const AVeyraStructure* Fallen = VeyraMatchEnding::FindFallenPrimeWell(*GetWorld()))
		{
			Pan.To = FVector(Fallen->GetActorLocation().X, Fallen->GetActorLocation().Y, Pan.From.Z);
		}
	}
	const double PanSeconds = GetDefault<UVeyraCameraSettings>()->EndPanSeconds;
	CameraRig->LookAt(Pan.To.IsSet() ? VeyraCamera::PanToward(Pan.From, Pan.To.GetValue(), Now - Pan.StartedAt, PanSeconds) : Pan.From);
	return true;
}

bool AVeyraPlayerController::IsPinging() const
{
	const UVeyraInputSettings& Keys = GetKeys();
	return IsInputKeyDown(Keys.PingKey) || IsInputKeyDown(Keys.DangerPingKey);
}

void AVeyraPlayerController::TickPings()
{
	VeyraPings::Forget(Pings, FPlatformTime::Seconds(), UVeyraMatchTuningSubsystem::Get().Pings);
	const UVeyraInputSettings& Keys = GetKeys();
	if (!IsPinging() || !WasInputKeyJustPressed(Keys.PingClickKey))
	{
		return;
	}
	// On the minimap, where it points; otherwise the ground under the cursor.
	TOptional<FVector> Point = MinimapPointUnderCursor(EMinimapClick::Ping);
	FHitResult Ground;
	if (!Point.IsSet() && GetHitResultUnderCursor(ECC_Visibility, /*bTraceComplex*/ false, Ground))
	{
		Point = Ground.Location;
	}
	if (Point.IsSet())
	{
		RequestPing(Point.GetValue(), IsInputKeyDown(Keys.DangerPingKey) ? EVeyraPingKind::Danger : EVeyraPingKind::Look);
	}
}

void AVeyraPlayerController::RequestPing(const FVector& Point, EVeyraPingKind Kind)
{
	ServerPing(Point, Kind);
}

void AVeyraPlayerController::ServerPing_Implementation(FVector Point, EVeyraPingKind Kind)
{
	UVeyraPingSubsystem* PingOwner = GetWorld()->GetSubsystem<UVeyraPingSubsystem>();
	const AVeyraPlayerState* Participant = GetPlayerState<AVeyraPlayerState>();
	const EVeyraPingRefusal Refusal = PingOwner && Participant ? PingOwner->Ping(*Participant, Point, Kind) : EVeyraPingRefusal::NotAPlayer;
	if (Refusal != EVeyraPingRefusal::None)
	{
		ClientPingRefused(Refusal);
	}
}

void AVeyraPlayerController::ClientPinged_Implementation(const FVeyraPing& Ping)
{
	Pings.Add({ Ping, FPlatformTime::Seconds() });
}

void AVeyraPlayerController::ClientPingRefused_Implementation(EVeyraPingRefusal Refusal)
{
	LastPingRefusal = Refusal;
}

void AVeyraPlayerController::RequestChat(EVeyraChatChannel Channel, const FString& Text)
{
	ServerChat(Channel, Text);
}

void AVeyraPlayerController::ServerChat_Implementation(EVeyraChatChannel Channel, const FString& Text)
{
	UVeyraChatSubsystem* ChatOwner = GetWorld()->GetSubsystem<UVeyraChatSubsystem>();
	const AVeyraPlayerState* Participant = GetPlayerState<AVeyraPlayerState>();
	const EVeyraChatRefusal Refusal = ChatOwner && Participant ? ChatOwner->Send(*Participant, Channel, Text) : EVeyraChatRefusal::NotAPlayer;
	if (Refusal != EVeyraChatRefusal::None)
	{
		ClientChatRefused(Refusal);
	}
}

void AVeyraPlayerController::ClientChatted_Implementation(const FVeyraChatMessage& Message)
{
	FVeyraReceivedChat Line;
	Line.Message = Message;
	KeepChat(MoveTemp(Line));
}

void AVeyraPlayerController::ClientChatRefused_Implementation(EVeyraChatRefusal Refusal)
{
	LastChatRefusal = Refusal;
	FVeyraReceivedChat Line;
	Line.Notice = EVeyraChatNotice::Refused;
	Line.Refusal = Refusal;
	KeepChat(MoveTemp(Line));
}

void AVeyraPlayerController::NoteChat(EVeyraChatNotice Notice, const FString& Subject)
{
	FVeyraReceivedChat Line;
	Line.Notice = Notice;
	if (Notice == EVeyraChatNotice::UnknownCommand)
	{
		Line.Message.Text = Subject;
	}
	else
	{
		Line.Message.SenderName = Subject;
	}
	KeepChat(MoveTemp(Line));
}

void AVeyraPlayerController::KeepChat(FVeyraReceivedChat&& Line)
{
	Line.ReceivedAt = FPlatformTime::Seconds();
	const AVeyraGameState* Match = GetWorld() ? GetWorld()->GetGameState<AVeyraGameState>() : nullptr;
	Line.MatchSeconds = Match ? Match->GetMatchClockSeconds() : 0.0;
	Chat.Add(MoveTemp(Line));
	VeyraChat::Forget(Chat, UVeyraMatchTuningSubsystem::Get().Chat);
}

void AVeyraPlayerController::ReportAllChat()
{
	if (const UVeyraSettingsSubsystem* Settings = IsLocalController() ? UVeyraSettingsSubsystem::Get(this) : nullptr; Settings && Settings->IsReady())
	{
		RequestAllChat(Settings->GetStore().IsOn(AllChatSetting()));
	}
}

void AVeyraPlayerController::RequestMute(int32 PlayerId, bool bMuted)
{
	// Kept here for the scoreboard's mute toggles; the server enforces it at delivery (ADR-029 §3).
	if (bMuted)
	{
		ChatMuted.Add(PlayerId);
	}
	else
	{
		ChatMuted.Remove(PlayerId);
	}
	ServerMuteChat(PlayerId, bMuted);
}

void AVeyraPlayerController::ServerMuteChat_Implementation(int32 PlayerId, bool bMuted)
{
	UVeyraChatSubsystem* ChatOwner = GetWorld()->GetSubsystem<UVeyraChatSubsystem>();
	if (const AVeyraPlayerState* Participant = GetPlayerState<AVeyraPlayerState>(); ChatOwner && Participant)
	{
		ChatOwner->SetMuted(*Participant, PlayerId, bMuted);
	}
}

void AVeyraPlayerController::RequestAllChat(bool bOn)
{
	ServerAllChat(bOn);
}

void AVeyraPlayerController::ServerAllChat_Implementation(bool bOn)
{
	UVeyraChatSubsystem* ChatOwner = GetWorld()->GetSubsystem<UVeyraChatSubsystem>();
	if (const AVeyraPlayerState* Participant = GetPlayerState<AVeyraPlayerState>(); ChatOwner && Participant)
	{
		ChatOwner->SetAllChat(*Participant, bOn);
	}
}

void AVeyraPlayerController::ServerRequestVote_Implementation(EVeyraVoteKind Kind)
{
	UVeyraVoteSubsystem* Votes = GetWorld()->GetSubsystem<UVeyraVoteSubsystem>();
	const AVeyraPlayerState* Participant = GetPlayerState<AVeyraPlayerState>();
	const EVeyraVoteRefusal Refusal = Votes && Participant ? Votes->Request(*Participant, Kind) : EVeyraVoteRefusal::NotAVoter;
	if (Refusal != EVeyraVoteRefusal::None)
	{
		ClientVoteRefused(Refusal);
	}
}

void AVeyraPlayerController::ServerCastVote_Implementation(bool bYes)
{
	UVeyraVoteSubsystem* Votes = GetWorld()->GetSubsystem<UVeyraVoteSubsystem>();
	const AVeyraPlayerState* Participant = GetPlayerState<AVeyraPlayerState>();
	const EVeyraVoteRefusal Refusal = Votes && Participant ? Votes->CastBallot(*Participant, bYes) : EVeyraVoteRefusal::NotAVoter;
	if (Refusal != EVeyraVoteRefusal::None)
	{
		ClientVoteRefused(Refusal);
	}
}

void AVeyraPlayerController::ClientVoteRefused_Implementation(EVeyraVoteRefusal Refusal)
{
	LastVoteRefusal = Refusal;
	++VoteRefusalCount;
	UE_LOG(LogVeyraMatch, Verbose, TEXT("The server refused the vote: %s."), *StaticEnum<EVeyraVoteRefusal>()->GetNameStringByValue(static_cast<int64>(Refusal)));
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

void AVeyraPlayerController::RequestSwapVisionTool(EVeyraVisionTool Tool)
{
	ServerSwapVisionTool(Tool);
}

void AVeyraPlayerController::ServerSwapVisionTool_Implementation(EVeyraVisionTool Tool)
{
	RunShopRequest([Tool](UVeyraShopSubsystem& Shop, APlayerState& Participant) {
		UVeyraVisionToolComponent* Slot = Participant.FindComponentByClass<UVeyraVisionToolComponent>();
		if (!Slot)
		{
			return EVeyraShopRefusal::NotNow;
		}
		if (Slot->GetEquipped() == Tool)
		{
			return EVeyraShopRefusal::AlreadyEquipped;
		}
		// Every swap costs the same, returning to a tool too (Vision Bible §3); the shop takes the Gold, Vision equips.
		const EVeyraShopRefusal Refusal = Shop.ChargeAtFountain(Participant, UVeyraEconomyTuningSubsystem::Get().VisionTools.SwapCost, TEXT("a vision tool"));
		if (Refusal == EVeyraShopRefusal::None)
		{
			Slot->Equip(Tool);
		}
		return Refusal;
	});
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

void AVeyraPlayerController::RequestBuyback()
{
	ServerBuyback();
}

void AVeyraPlayerController::ServerBuyback_Implementation()
{
	if (!TakeOrderAllowance())
	{
		RejectOrder(EVeyraOrderRejection::TooFrequent);
		return;
	}
	AVeyraGameMode* GameMode = GetWorld()->GetAuthGameMode<AVeyraGameMode>();
	const EVeyraBuybackRefusal Refusal = GameMode ? GameMode->HandleBuybackOrder(GetPlayerState<AVeyraPlayerState>()) : EVeyraBuybackRefusal::Unavailable;
	if (Refusal != EVeyraBuybackRefusal::None)
	{
		UE_LOG(LogVeyraMatch, Verbose, TEXT("Refused a buyback from %s: %s."), *GetNameSafe(PlayerState), LexToString(Refusal));
		ClientBuybackRefused(Refusal);
	}
}

void AVeyraPlayerController::ClientBuybackRefused_Implementation(EVeyraBuybackRefusal Refusal)
{
	LastBuybackRefusal = Refusal;
	++BuybackRefusalCount;
	UE_LOG(LogVeyraMatch, Verbose, TEXT("The server refused a buyback: %s."), LexToString(Refusal));
}

void AVeyraPlayerController::RequestDeveloperCommand(const FString& Command, const TArray<FString>& Args)
{
	ServerRequestDeveloperCommand(Command, Args);
}

void AVeyraPlayerController::ServerRequestDeveloperCommand_Implementation(const FString& Command, const TArray<FString>& Args)
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogVeyraMatch, Warning, TEXT("Refused developer command %s from %s: Shipping builds run none."), *Command, *GetNameSafe(PlayerState));
#else
	const FString Reply = VeyraDeveloperCommandRoute::Run(*this, Command, Args);
	UE_LOG(LogVeyraMatch, Log, TEXT("%s ran Veyra.Dev.%s %s: %s"), *GetNameSafe(PlayerState), *Command, *FString::Join(Args, TEXT(" ")), *Reply);
	ClientDeveloperCommandReply(Reply);
#endif
}

void AVeyraPlayerController::ClientDeveloperCommandReply_Implementation(const FString& Reply)
{
	LastDeveloperCommandReply = Reply;
	++DeveloperCommandReplyCount;
	UE_LOG(LogVeyraMatch, Display, TEXT("Developer command: %s"), *Reply);

	// The console the command was typed in shows the reply too.
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (LocalPlayer && LocalPlayer->ViewportClient && LocalPlayer->ViewportClient->ViewportConsole)
	{
		LocalPlayer->ViewportClient->ViewportConsole->OutputText(Reply);
	}
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

	const AActor* ViewPoint = GetCommandedBody();
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
	if (!NewPawn)
	{
		return;
	}
	if (!CameraRig)
	{
		SetViewTarget(NewPawn);
		return;
	}
	// The camera goes to the first body, and to each respawn unless the player turned that off (SET-156);
	// between them it goes where the player takes it, as far as the camera's mode allows.
	const bool bRespawn = bHadVanguard;
	bHadVanguard = true;
	if (!bRespawn || CameraPreferences().bReturnOnRespawn)
	{
		CameraRig->CenterOn(NewPawn->GetActorLocation());
	}
	SetViewTarget(CameraRig);
}

FVeyraCameraPreferences AVeyraPlayerController::CameraPreferences() const
{
	const UVeyraSettingsSubsystem* Settings = UVeyraSettingsSubsystem::Get(this);
	return VeyraCameraPreferences::Resolve(*GetDefault<UVeyraCameraSettings>(), Settings ? &Settings->GetStore() : nullptr);
}

FVeyraControlPreferences AVeyraPlayerController::ControlPreferences() const
{
	const UVeyraSettingsSubsystem* Settings = UVeyraSettingsSubsystem::Get(this);
	return VeyraControlPreferences::Resolve(Settings ? &Settings->GetStore() : nullptr);
}

void AVeyraPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (IsLocalController())
	{
		TickPings();
		TickCastInput();
	}
	if (IsLocalController() && CameraRig)
	{
		TickCamera(DeltaTime);
	}
}

void AVeyraPlayerController::TickCamera(float DeltaTime)
{
	const UVeyraInputSettings& Keys = GetKeys();
	const FVeyraCameraPreferences View = CameraPreferences();
	if (TickEndPan(DeltaTime))
	{
		return;
	}
	if (WasInputKeyJustPressed(Keys.CameraModeKey))
	{
		CameraRig->SetMode(VeyraCamera::Next(CameraRig->GetMode()));
		// Kept for the next match.
		if (UVeyraSettingsSubsystem* Settings = UVeyraSettingsSubsystem::Get(this))
		{
			Settings->GetStore().Set(VeyraCameraPreferences::DefaultMode(), VeyraCameraPreferences::ModeName(CameraRig->GetMode()), /*bInLiveMatch*/ true);
		}
	}
	FVeyraCameraInput CameraInput;
	CameraInput.Pan.X = (IsInputKeyDown(Keys.CameraRightKey) ? 1.0 : 0.0) - (IsInputKeyDown(Keys.CameraLeftKey) ? 1.0 : 0.0);
	CameraInput.Pan.Y = (IsInputKeyDown(Keys.CameraUpKey) ? 1.0 : 0.0) - (IsInputKeyDown(Keys.CameraDownKey) ? 1.0 : 0.0);
	FVector2D Mouse;
	const bool bHasMouse = GetMousePosition(Mouse.X, Mouse.Y);
	const UGameViewportClient* Viewport = GetLocalPlayer() ? GetLocalPlayer()->ViewportClient : nullptr;
	// The edges pan only while the game's window has focus.
	if (bHasMouse && View.bEdgeScroll && Viewport && Viewport->Viewport && Viewport->Viewport->HasFocus())
	{
		FVector2D Size;
		Viewport->GetViewportSize(Size);
		CameraInput.EdgePan = VeyraCamera::EdgePan(Mouse, Size, View.EdgeScrollPixels);
	}
	CameraInput.EdgePan = VeyraCamera::DelayEdgePan(CameraInput.EdgePan, DeltaTime, View.EdgeDelaySeconds, EdgeHeldSeconds);
	// Dragging moves the ground with the cursor, so the view moves against it.
	if (bHasMouse && IsInputKeyDown(Keys.CameraDragKey))
	{
		if (LastDragMouse.IsSet())
		{
			const FVector2D Moved = Mouse - LastDragMouse.GetValue();
			CameraInput.Drag = FVector2D(-Moved.X, Moved.Y) * View.DragUnitsPerPixel;
		}
		LastDragMouse = Mouse;
	}
	else
	{
		LastDragMouse.Reset();
	}
	CameraInput.bHoldCenter = IsInputKeyDown(Keys.HoldToCenterKey);
	// Held on the minimap, the camera looks where it points; with a ping key held, the click pings instead.
	if (IsInputKeyDown(Keys.MinimapCameraKey) && !IsPinging())
	{
		if (const TOptional<FVector> OnMap = MinimapPointUnderCursor(EMinimapClick::Camera))
		{
			CameraRig->LookAt(FVector(OnMap->X, OnMap->Y, CameraRig->GetFocus().Z));
			return;
		}
	}
	// The camera follows the body the player's orders move: its Vanguard, or the Echo it commands (ADR-050 §6).
	if (const APawn* Commanded = GetCommandedBody())
	{
		CameraInput.Vanguard = Commanded->GetActorLocation();
	}
	else if (!View.bFreeWhileDead && CameraRig->GetMode() != EVeyraCameraMode::Free)
	{
		// Waiting to respawn, a Locked or Semi-Locked camera keeps its mode's ordinary control (SET-157).
		CameraInput.Pan = FVector2D::ZeroVector;
		CameraInput.EdgePan = FVector2D::ZeroVector;
		CameraInput.Drag = FVector2D::ZeroVector;
	}
	CameraRig->Step(CameraInput, DeltaTime, &View);
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

void AVeyraPlayerController::ServerIssueAttackMoveOrder_Implementation(FVector Destination, EVeyraAttackMoveTarget Preference)
{
	if (!TakeOrderAllowance())
	{
		RejectOrder(EVeyraOrderRejection::TooFrequent);
		return;
	}
	AVeyraGameMode* GameMode = GetWorld()->GetAuthGameMode<AVeyraGameMode>();
	const EVeyraOrderRejection Rejection = GameMode ? GameMode->HandleAttackMoveOrder(GetPlayerState<AVeyraPlayerState>(), Destination, Preference) : EVeyraOrderRejection::WrongPhase;
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
