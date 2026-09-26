// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraGameMode.h"

#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "Tuning/VeyraTuning.h"
#include "VeyraAbilitiesVerbs.h"
#include "VeyraCombatVerbs.h"
#include "VeyraGameState.h"
#include "VeyraJoinRules.h"
#include "VeyraMatchLog.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"
#include "VeyraTeamStart.h"
#include "VeyraVanguardCharacter.h"
#include "VeyraVanguardController.h"

namespace
{
	/** The sides that field Vanguards. */
	constexpr EVeyraTeam Sides[] = { EVeyraTeam::A, EVeyraTeam::B };

	// Until the backend supplies a roster (M4), a developer server is told how many humans to wait for.
	TAutoConsoleVariable<int32> CVarExpectedPlayers(
		TEXT("veyra.Match.ExpectedPlayers"), 0,
		TEXT("Humans a developer match waits for during loading when the server URL has no VeyraExpectedPlayers option. ")
		TEXT("0 waits for the loading timeout."),
		ECVF_Default);
}

AVeyraGameMode::AVeyraGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameStateClass = AVeyraGameState::StaticClass();
	PlayerStateClass = AVeyraPlayerState::StaticClass();
	PlayerControllerClass = AVeyraPlayerController::StaticClass();
	VanguardClass = AVeyraVanguardCharacter::StaticClass();

	// Players never get an engine-spawned pawn or spectator: their Vanguard is spawned by this class.
	DefaultPawnClass = nullptr;
	SpectatorClass = nullptr;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AVeyraGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	ExpectedPlayers = UGameplayStatics::GetIntOption(Options, ExpectedPlayersOption, CVarExpectedPlayers.GetValueOnGameThread());
}

void AVeyraGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (ErrorMessage.IsEmpty())
	{
		ErrorMessage = VeyraJoinRules::CheckDirectConnect();
	}
	if (ErrorMessage.IsEmpty())
	{
		ErrorMessage = VeyraJoinRules::CheckTuningHash(Options, VeyraTuning::GetCompositeHash());
	}
	if (ErrorMessage.IsEmpty() && IsFull())
	{
		ErrorMessage = TEXT("The match is full.");
	}
	if (!ErrorMessage.IsEmpty())
	{
		UE_LOG(LogVeyraMatch, Warning, TEXT("Refused a connection from %s: %s"), *Address, *ErrorMessage);
	}
}

void AVeyraGameMode::StartPlay()
{
	Super::StartPlay();

	GetVeyraGameState().SetPhase(EVeyraMatchPhase::Loading);
	GetWorldTimerManager().SetTimer(LoadingTimeout, this, &AVeyraGameMode::OnLoadingTimedOut,
		static_cast<float>(UVeyraMatchTuningSubsystem::Get().Phases.LoadingTimeoutSeconds));
	SetActorTickEnabled(true);
	if (UVeyraCombatEventSubsystem* Events = GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>())
	{
		DeathHandle = Events->OnDeath.AddUObject(this, &AVeyraGameMode::OnDeath);
	}
	// Game/Scripts/Smoke.ps1 waits for this line.
	UE_LOG(LogVeyraMatch, Display, TEXT("Match server ready: loading, waiting for %d player(s)."), ExpectedPlayers);
}

void AVeyraGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UVeyraCombatEventSubsystem* Events = GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>())
	{
		Events->OnDeath.Remove(DeathHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void AVeyraGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Only loading ticks: it ends once the map is ready and every expected human has joined, or the
	// timeout has passed.
	if (GetVeyraGameState().GetPhase() == EVeyraMatchPhase::Loading && IsMapReady() && (bLoadingTimedOut || HaveExpectedPlayersJoined()))
	{
		BeginPreparation();
	}
}

EVeyraOrderRejection AVeyraGameMode::CheckOrdersAllowed() const
{
	if (GetWorld()->IsPaused())
	{
		return EVeyraOrderRejection::Paused;
	}
	// Preparation should allow movement inside the fountain (Match Flow Bible §1, stage 3). Until
	// the map has fountain areas, it refuses orders instead.
	return GetVeyraGameState().GetPhase() == EVeyraMatchPhase::Live ? EVeyraOrderRejection::None : EVeyraOrderRejection::WrongPhase;
}

EVeyraOrderRejection AVeyraGameMode::HandleMoveOrder(AVeyraPlayerController& Player, const FVector& Destination)
{
	const EVeyraOrderRejection Allowed = CheckOrdersAllowed();
	if (Allowed != EVeyraOrderRejection::None)
	{
		return Allowed;
	}
	if (Destination.ContainsNaN() || !FMath::IsFinite(Destination.X) || !FMath::IsFinite(Destination.Y) || !FMath::IsFinite(Destination.Z))
	{
		return EVeyraOrderRejection::InvalidOrder;
	}
	const AVeyraPlayerState* PlayerState = Player.GetPlayerState<AVeyraPlayerState>();
	AVeyraVanguardController* Controller = PlayerState ? PlayerState->GetVanguardController() : nullptr;
	return Controller ? Controller->MoveToDestination(Destination) : EVeyraOrderRejection::NoVanguard;
}

EVeyraCastRejection AVeyraGameMode::HandleCastOrder(AVeyraPlayerController& Player, EVeyraAbilitySlot Slot, const FVeyraCastTarget& Target)
{
	switch (CheckOrdersAllowed())
	{
	case EVeyraOrderRejection::None:
		break;
	case EVeyraOrderRejection::Paused:
		return EVeyraCastRejection::Paused;
	default:
		return EVeyraCastRejection::WrongPhase;
	}
	const AVeyraPlayerState* PlayerState = Player.GetPlayerState<AVeyraPlayerState>();
	UAbilitySystemComponent* AbilitySystem = PlayerState ? PlayerState->GetAbilitySystemComponent() : nullptr;
	return AbilitySystem ? VeyraAbilities::TryCast(*AbilitySystem, Slot, Target) : EVeyraCastRejection::UnknownAbility;
}

bool AVeyraGameMode::PauseMatch(APlayerController& Requester)
{
	// The engine's world pause stops every gameplay clock on the server; the GameState tells clients.
	if (GetVeyraGameState().IsMatchPaused() || !SetPause(&Requester))
	{
		return false;
	}
	GetVeyraGameState().SetMatchPaused(true);
	UE_LOG(LogVeyraMatch, Log, TEXT("Match paused by %s."), *GetNameSafe(Requester.PlayerState));
	return true;
}

bool AVeyraGameMode::ResumeMatch()
{
	if (!GetVeyraGameState().IsMatchPaused() || !ClearPause())
	{
		return false;
	}
	GetVeyraGameState().SetMatchPaused(false);
	UE_LOG(LogVeyraMatch, Log, TEXT("Match resumed."));
	return true;
}

void AVeyraGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Not the engine's version, which would spawn a default pawn for the player.
	AVeyraPlayerState* PlayerState = NewPlayer ? NewPlayer->GetPlayerState<AVeyraPlayerState>() : nullptr;
	if (!PlayerState)
	{
		UE_LOG(LogVeyraMatch, Error, TEXT("%s has no Veyra PlayerState and cannot join the match."), *GetNameSafe(NewPlayer));
		return;
	}
	AssignTeam(*PlayerState);
	if (GetVeyraGameState().GetPhase() != EVeyraMatchPhase::Loading)
	{
		SpawnVanguard(*PlayerState);
	}
}

bool AVeyraGameMode::PlayerCanRestart_Implementation(APlayerController* /*Player*/)
{
	// Respawning belongs to the match, never to a client request.
	return false;
}

bool AVeyraGameMode::CanSpectate_Implementation(APlayerController* /*Viewer*/, APlayerState* /*ViewTarget*/)
{
	return false;
}

AVeyraGameState& AVeyraGameMode::GetVeyraGameState() const
{
	return *CastChecked<AVeyraGameState>(GameState);
}

bool AVeyraGameMode::IsFull() const
{
	int32 Participants = 0;
	for (const EVeyraTeam Side : Sides)
	{
		Participants += CountTeamMembers(Side);
	}
	return Participants >= static_cast<int32>(UE_ARRAY_COUNT(Sides)) * UVeyraMatchTuningSubsystem::Get().Teams.MaxTeamSize;
}

AVeyraPlayerState* AVeyraGameMode::AddBotParticipant(const FString& Name)
{
	if (IsFull())
	{
		UE_LOG(LogVeyraMatch, Warning, TEXT("Cannot add %s: the match is full."), *Name);
		return nullptr;
	}

	// Unlike a human's, a bot's controller carries the participant's PlayerState itself.
	FActorSpawnParameters Parameters;
	Parameters.ObjectFlags |= RF_Transient;
	Parameters.bDeferConstruction = true;
	AVeyraVanguardController* Controller = GetWorld()->SpawnActor<AVeyraVanguardController>(Parameters);
	if (!Controller)
	{
		return nullptr;
	}
	Controller->bWantsPlayerState = true;
	Controller->FinishSpawning(FTransform::Identity);

	AVeyraPlayerState* PlayerState = Controller->GetPlayerState<AVeyraPlayerState>();
	if (!PlayerState)
	{
		UE_LOG(LogVeyraMatch, Error, TEXT("%s got no Veyra PlayerState and cannot join the match."), *Name);
		Controller->Destroy();
		return nullptr;
	}
	PlayerState->SetIsABot(true);
	PlayerState->SetPlayerName(Name);
	PlayerState->SetVanguardController(Controller);
	AssignTeam(*PlayerState);
	if (GetVeyraGameState().GetPhase() != EVeyraMatchPhase::Loading)
	{
		SpawnVanguard(*PlayerState);
	}
	return PlayerState;
}

int32 AVeyraGameMode::CountTeamMembers(EVeyraTeam Team) const
{
	int32 Count = 0;
	for (const APlayerState* Member : GameState->PlayerArray)
	{
		Count += VeyraTeams::TeamOf(Member) == Team ? 1 : 0;
	}
	return Count;
}

void AVeyraGameMode::AssignTeam(AVeyraPlayerState& PlayerState) const
{
	// The smaller side, or Team A on a tie. PreLogin has already refused anyone beyond capacity.
	const EVeyraTeam Team = CountTeamMembers(EVeyraTeam::B) < CountTeamMembers(EVeyraTeam::A) ? EVeyraTeam::B : EVeyraTeam::A;
	PlayerState.SetVeyraTeam(Team);
	UE_LOG(LogVeyraMatch, Log, TEXT("%s joins Team %s."), *PlayerState.GetPlayerName(), Team == EVeyraTeam::A ? TEXT("A") : TEXT("B"));
}

AActor* AVeyraGameMode::FindTeamStart(EVeyraTeam Team) const
{
	for (TActorIterator<AVeyraTeamStart> It(GetWorld()); It; ++It)
	{
		if (It->GetVeyraTeam() == Team)
		{
			return *It;
		}
	}
	return nullptr;
}

bool AVeyraGameMode::IsMapReady() const
{
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Navigation || Navigation->IsNavigationBuildInProgress())
	{
		return false;
	}
	for (const EVeyraTeam Side : Sides)
	{
		const AActor* Start = FindTeamStart(Side);
		FNavLocation Walkable;
		if (!Start || !Navigation->ProjectPointToNavigation(Start->GetActorLocation(), Walkable,
			FVector(UVeyraMatchTuningSubsystem::Get().Orders.DestinationProjectionExtent)))
		{
			return false;
		}
	}
	return true;
}

bool AVeyraGameMode::HaveExpectedPlayersJoined()
{
	return ExpectedPlayers > 0 && GetNumPlayers() >= ExpectedPlayers;
}

void AVeyraGameMode::OnLoadingTimedOut()
{
	bLoadingTimedOut = true;
	UE_CLOG(!IsMapReady(), LogVeyraMatch, Error, TEXT("Loading timed out, but the map is not ready: it needs a start for each side on built navigation."));
}

void AVeyraGameMode::BeginPreparation()
{
	SetActorTickEnabled(false);
	GetWorldTimerManager().ClearTimer(LoadingTimeout);
	GetVeyraGameState().SetPhase(EVeyraMatchPhase::Preparation);
	UE_LOG(LogVeyraMatch, Log, TEXT("Preparation begins with %d player(s)."), GetNumPlayers());

	// Participants are the players given a side when they joined. Other PlayerStates, such as the
	// replay recorder's spectator, get no Vanguard.
	for (APlayerState* Member : GameState->PlayerArray)
	{
		AVeyraPlayerState* PlayerState = Cast<AVeyraPlayerState>(Member);
		if (PlayerState && PlayerState->GetVeyraTeam() != EVeyraTeam::None)
		{
			SpawnVanguard(*PlayerState);
		}
	}
	GetWorldTimerManager().SetTimer(PreparationTimer, this, &AVeyraGameMode::BeginLive,
		static_cast<float>(UVeyraMatchTuningSubsystem::Get().Phases.PreparationSeconds));
}

void AVeyraGameMode::BeginLive()
{
	GetVeyraGameState().SetPhase(EVeyraMatchPhase::Live);
	UE_LOG(LogVeyraMatch, Log, TEXT("The match is live."));
}

void AVeyraGameMode::SpawnVanguard(AVeyraPlayerState& PlayerState)
{
	AActor* Start = FindTeamStart(PlayerState.GetVeyraTeam());
	UAbilitySystemComponent* AbilitySystem = PlayerState.GetAbilitySystemComponent();
	if (!Start || !AbilitySystem)
	{
		UE_LOG(LogVeyraMatch, Error, TEXT("Cannot spawn a Vanguard for %s: its side has no start."), *PlayerState.GetPlayerName());
		return;
	}

	if (!PlayerState.HasInitializedStats() && !InitializeCombatant(PlayerState, *AbilitySystem))
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AVeyraVanguardCharacter* Vanguard = GetWorld()->SpawnActor<AVeyraVanguardCharacter>(VanguardClass, Start->GetActorTransform(), SpawnParameters);
	if (!Vanguard)
	{
		UE_LOG(LogVeyraMatch, Error, TEXT("Failed to spawn a Vanguard for %s."), *PlayerState.GetPlayerName());
		return;
	}

	AVeyraVanguardController* Controller = PlayerState.GetVanguardController();
	if (!Controller)
	{
		FActorSpawnParameters ControllerParameters;
		ControllerParameters.ObjectFlags |= RF_Transient;
		Controller = GetWorld()->SpawnActor<AVeyraVanguardController>(ControllerParameters);
		PlayerState.SetVanguardController(Controller);
	}
	Controller->Possess(Vanguard);
	// An AI controller has no PlayerState of its own, so possession leaves the pawn without one.
	Vanguard->SetPlayerState(&PlayerState);
}

bool AVeyraGameMode::InitializeCombatant(AVeyraPlayerState& PlayerState, UAbilitySystemComponent& AbilitySystem)
{
	const FVeyraDeveloperLoadoutTuning& Loadout = UVeyraMatchTuningSubsystem::Get().DeveloperLoadout;
	UVeyraAbilityLoadoutComponent* Abilities = PlayerState.FindComponentByClass<UVeyraAbilityLoadoutComponent>();
	if (!VeyraCombat::InitializeVitals(AbilitySystem, Loadout.MaxHealth) || !VeyraCombat::InitializeResource(AbilitySystem, Loadout.MaxResource)
		|| !VeyraCombat::InitializeMoveSpeed(AbilitySystem, Loadout.MoveSpeed) || !Abilities
		|| !Abilities->Grant(AbilitySystem, EVeyraAbilitySlot::Q, Loadout.AbilityQ))
	{
		UE_LOG(LogVeyraMatch, Error, TEXT("Could not prepare %s for the match; see the errors above."), *PlayerState.GetPlayerName());
		return false;
	}
	PlayerState.MarkStatsInitialized();
	return true;
}

void AVeyraGameMode::OnDeath(const FVeyraDeathEvent& Death)
{
	const UAbilitySystemComponent* Victim = Death.Victim.Get();
	AVeyraPlayerState* PlayerState = Victim ? Cast<AVeyraPlayerState>(Victim->GetOwner()) : nullptr;
	if (!PlayerState)
	{
		return;
	}

	// The body leaves the map; the PlayerState, with its cooldowns and permanent effects, stays. The
	// death arrives from inside the damage that caused it, so the body goes on the next tick. The
	// timer manager ignores a delay of 0, so an immediate respawn follows the body out on that tick.
	const double Delay = UVeyraMatchTuningSubsystem::Get().Respawn.DelaySeconds;
	UE_LOG(LogVeyraMatch, Log, TEXT("%s died; respawning in %g s."), *PlayerState->GetPlayerName(), Delay);
	const TWeakObjectPtr<AVeyraPlayerState> Participant(PlayerState);
	const TWeakObjectPtr<APawn> Body(PlayerState->GetPawn());
	const bool bRespawnAtOnce = Delay <= 0.0;
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Participant, Body, bRespawnAtOnce] {
		if (APawn* OldBody = Body.Get())
		{
			OldBody->Destroy();
		}
		if (bRespawnAtOnce)
		{
			Respawn(Participant);
		}
	}));
	if (!bRespawnAtOnce)
	{
		FTimerHandle Timer;
		GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &AVeyraGameMode::Respawn, Participant), static_cast<float>(Delay), /*bLoop*/ false);
	}
}

void AVeyraGameMode::Respawn(TWeakObjectPtr<AVeyraPlayerState> PlayerState)
{
	UAbilitySystemComponent* AbilitySystem = PlayerState.IsValid() ? PlayerState->GetAbilitySystemComponent() : nullptr;
	if (!AbilitySystem || !VeyraCombat::Revive(*AbilitySystem))
	{
		return;
	}
	UE_LOG(LogVeyraMatch, Log, TEXT("%s respawns."), *PlayerState->GetPlayerName());
	SpawnVanguard(*PlayerState);
}
