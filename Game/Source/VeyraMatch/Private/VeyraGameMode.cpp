// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraGameMode.h"

#include "Absence/VeyraAbsenceSubsystem.h"
#include "Votes/VeyraVoteSubsystem.h"
#include "AbilitySystemComponent.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraResourceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Battleground/VeyraBattlegroundLink.h"
#include "Echoes/VeyraEchoLink.h"
#include "Feedback/VeyraCombatTextLink.h"
#include "Echoes/VeyraEchoSubsystem.h"
#include "Buyback/VeyraBuybackComponent.h"
#include "Casting/VeyraCastStateComponent.h"
#include "Bots/VeyraMatchEvents.h"
#include "EngineUtils.h"
#include "GameFramework/GameSession.h"
#include "Gold/VeyraGoldComponent.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Join/VeyraMatchHostSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "NavigationSystem.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Recall/VeyraRecallComponent.h"
#include "Rewards/VeyraRewardSubsystem.h"
#include "Rules/VeyraMatchRules.h"
#include "Shop/VeyraShopSubsystem.h"
#include "Statistics/VeyraMatchStatisticsSubsystem.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "TimerManager.h"
#include "Tools/VeyraVisionToolComponent.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
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
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraVanguardCharacter.h"
#include "VeyraVanguardController.h"
#include "VeyraVanguards.h"

namespace
{
	/** The sides that field Vanguards. */
	constexpr EVeyraTeam Sides[] = { EVeyraTeam::A, EVeyraTeam::B };

	// A developer server without a roster is told how many humans to wait for.
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
	// Only a dedicated server hosts an assigned match. In-process play can also run standalone
	// worlds in the same engine, for example a client that left and returned to the default map.
	const UVeyraMatchHostSubsystem* Host = UVeyraMatchHostSubsystem::Get();
	if (GetNetMode() == NM_DedicatedServer && Host && Host->GetAssignment().IsSet())
	{
		// A hosted match waits for its whole roster.
		Roster = MakeUnique<FVeyraMatchRoster>(Host->GetAssignment().GetValue());
		ExpectedPlayers = Roster->Num();
		UE_CLOG(UGameplayStatics::HasOption(Options, ExpectedPlayersOption), LogVeyraMatch, Warning,
			TEXT("Ignoring %s: this server hosts match %s and waits for its %d rostered participant(s)."),
			ExpectedPlayersOption, *Roster->GetAssignment().MatchId, ExpectedPlayers);
		return;
	}
	ExpectedPlayers = UGameplayStatics::GetIntOption(Options, ExpectedPlayersOption, CVarExpectedPlayers.GetValueOnGameThread());
}

void AVeyraGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (ErrorMessage.IsEmpty() && GetVeyraGameState().GetPhase() == EVeyraMatchPhase::Ended)
	{
		ErrorMessage = TEXT("The match has ended.");
	}
	if (ErrorMessage.IsEmpty())
	{
		ErrorMessage = VeyraJoinRules::CheckDirectConnect(Roster.IsValid());
	}
	if (ErrorMessage.IsEmpty())
	{
		ErrorMessage = VeyraJoinRules::CheckTuningHash(Options, VeyraTuning::GetCompositeHash());
	}
	bool bReturning = false;
	if (ErrorMessage.IsEmpty() && Roster)
	{
		FString AccountId;
		ErrorMessage = VeyraJoinRules::CheckTicket(Options, *Roster, AccountId);
		bReturning = ErrorMessage.IsEmpty() && (Roster->HasJoined(AccountId) || FindKeptPlayerState(AccountId));
	}
	// A returning participant, or a late one whose seat the match kept, takes back its own seat.
	if (ErrorMessage.IsEmpty() && !bReturning && IsFull())
	{
		ErrorMessage = TEXT("The match is full.");
	}
	if (!ErrorMessage.IsEmpty())
	{
		UE_LOG(LogVeyraMatch, Warning, TEXT("Refused a connection from %s: %s"), *Address, *ErrorMessage);
	}
}

FString AVeyraGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options,
	const FString& Portal)
{
	FString ErrorMessage = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
#if !UE_BUILD_SHIPPING
	// A development client may ask for a Vanguard for itself (ADR-008 §8); Shipping ignores the option.
	const FString Requested = UGameplayStatics::ParseOption(Options, VeyraJoinRules::VanguardOption);
	AVeyraPlayerState* Requester = NewPlayerController ? NewPlayerController->GetPlayerState<AVeyraPlayerState>() : nullptr;
	if (ErrorMessage.IsEmpty() && Requester && !Requested.IsEmpty())
	{
		Requester->SetRequestedVanguardId(FVeyraContentId::FromText(Requested).Get(FVeyraContentId()));
	}
#endif
	if (!ErrorMessage.IsEmpty() || !Roster)
	{
		return ErrorMessage;
	}

	// PreLogin checked the ticket; check it again here, where the participant is claimed, in case
	// another login with the same ticket was admitted in between.
	FString AccountId;
	ErrorMessage = VeyraJoinRules::CheckTicket(Options, *Roster, AccountId);
	AVeyraPlayerState* PlayerState = NewPlayerController ? NewPlayerController->GetPlayerState<AVeyraPlayerState>() : nullptr;
	if (ErrorMessage.IsEmpty() && !PlayerState)
	{
		ErrorMessage = TEXT("The player has no Veyra PlayerState.");
	}
	if (!ErrorMessage.IsEmpty())
	{
		UE_LOG(LogVeyraMatch, Warning, TEXT("Refused a login: %s"), *ErrorMessage);
		return ErrorMessage;
	}
	// One who left, or who never came before the match began without it, takes back its seat.
	if (AVeyraPlayerState* Kept = FindKeptPlayerState(AccountId))
	{
		GiveBackPlayerState(*NewPlayerController, *Kept);
		PlayerState = Kept;
	}
	PlayerState->SetAccountId(AccountId);
	PlayerState->SetPlayerName(Roster->FindByAccount(AccountId)->DisplayName);
	if (AccountId == Roster->GetAssignment().HostAccountId)
	{
		GetVeyraGameState().SetHost(PlayerState);
	}
	Roster->MarkConnected(AccountId);
	NoteConnectedParticipants();
	return ErrorMessage;
}

void AVeyraGameMode::Logout(AController* Exiting)
{
	const AVeyraPlayerState* PlayerState = Exiting ? Exiting->GetPlayerState<AVeyraPlayerState>() : nullptr;
	if (Roster && PlayerState && !PlayerState->GetAccountId().IsEmpty())
	{
		Roster->MarkDisconnected(PlayerState->GetAccountId());
		UE_LOG(LogVeyraMatch, Log, TEXT("%s left; %d rostered participant(s) connected."), *PlayerState->GetPlayerName(), Roster->NumConnected());
		NoteConnectedParticipants();
	}
	// Its PlayerState stays for its return (ADR-019 §1); the record also keeps its line as it left, in
	// case the PlayerState goes (ADR-017 §5).
	if (UVeyraMatchStatisticsSubsystem* Statistics = PlayerState ? GetWorld()->GetSubsystem<UVeyraMatchStatisticsSubsystem>() : nullptr)
	{
		Statistics->NoteLeaving(*PlayerState);
	}
	// Its Echo ends with its player gone, waking its Vanguard from Stasis, so the autopilot takes it at once (ADR-050 §6).
	UVeyraEchoSubsystem* Echoes = GetWorld()->GetSubsystem<UVeyraEchoSubsystem>();
	if (const UAbilitySystemComponent* Holder = PlayerState ? PlayerState->GetAbilitySystemComponent() : nullptr; Holder && Echoes)
	{
		Echoes->End(*Holder, EVeyraEchoEnd::HolderGone);
	}
	// Its Vanguard is walked to safety until it returns (Match Flow Bible §4).
	if (UVeyraAbsenceSubsystem* Absence = PlayerState ? GetWorld()->GetSubsystem<UVeyraAbsenceSubsystem>() : nullptr)
	{
		Absence->NoteDisconnected(*PlayerState);
	}
	Super::Logout(Exiting);
}

void AVeyraGameMode::StartPlay()
{
	Super::StartPlay();

	if (Roster)
	{
		const FVeyraMatchAssignment& Assigned = Roster->GetAssignment();
		GetVeyraGameState().SetMatchRules(Assigned.Rules, VeyraMatchRules::HasVictory(Assigned.Rules, Assigned.Custom));
	}
	else
	{
		GetVeyraGameState().SetMatchRules(EVeyraMatchRules::Standard);
	}
	GetVeyraGameState().SetPhase(EVeyraMatchPhase::Loading);
	GetWorldTimerManager().SetTimer(LoadingTimeout, this, &AVeyraGameMode::OnLoadingTimedOut,
		static_cast<float>(UVeyraMatchTuningSubsystem::Get().Phases.LoadingTimeoutSeconds));
	SetActorTickEnabled(true);
	if (UVeyraCombatEventSubsystem* Events = GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>())
	{
		DeathHandle = Events->OnDeath.AddUObject(this, &AVeyraGameMode::OnDeath);
	}
	// The match's record begins with it (ADR-017 §3).
	if (UVeyraMatchStatisticsSubsystem* Statistics = GetWorld()->GetSubsystem<UVeyraMatchStatisticsSubsystem>())
	{
		Statistics->Start();
	}
	Battleground = MakeShared<FVeyraBattlegroundLink>();
	Battleground->Start(*GetWorld(), FVeyraBattlegroundLink::FOnPrimeWellDestroyed::CreateUObject(this, &AVeyraGameMode::OnPrimeWellDestroyed));
	EchoLink = MakeShared<FVeyraEchoLink>();
	EchoLink->Start(*GetWorld());
	CombatText = MakeShared<FVeyraCombatTextLink>();
	CombatText->Start(*GetWorld());
	if (Roster)
	{
		NoteConnectedParticipants();
		AbandonmentTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &AVeyraGameMode::TickAbandonment));
	}
	// Game/Scripts/Smoke.ps1 waits for this line.
	UE_LOG(LogVeyraMatch, Display, TEXT("Match server ready: loading, waiting for %d player(s)."), ExpectedPlayers);
	// The map is loaded and the server is listening.
	UVeyraMatchHostSubsystem* Host = UVeyraMatchHostSubsystem::Get();
	if (Host && GetNetMode() == NM_DedicatedServer)
	{
		Host->OnAcceptingPlayers.Broadcast();
	}
}

void AVeyraGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FTSTicker::GetCoreTicker().RemoveTicker(AbandonmentTicker);
	if (UVeyraCombatEventSubsystem* Events = GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>())
	{
		Events->OnDeath.Remove(DeathHandle);
	}
	Battleground.Reset();
	EchoLink.Reset();
	CombatText.Reset();
	Super::EndPlay(EndPlayReason);
}

void AVeyraGameMode::EndMatch(EVeyraMatchEndReason Reason, EVeyraTeam Winner)
{
	AVeyraGameState& State = GetVeyraGameState();
	if (State.GetPhase() == EVeyraMatchPhase::Ended)
	{
		return;
	}
	if (!VeyraMatchResults::IsWinnerConsistent(Reason, Winner))
	{
		UE_LOG(LogVeyraMatch, Error, TEXT("Refused to end the match (%s) with winner %s: only a destroyed Prime Well or a surrender has a winner."), LexToString(Reason),
			*StaticEnum<EVeyraTeam>()->GetNameStringByValue(static_cast<int64>(Winner)));
		return;
	}
	SetActorTickEnabled(false);
	GetWorldTimerManager().ClearTimer(LoadingTimeout);
	GetWorldTimerManager().ClearTimer(PreparationTimer);
	GetWorldTimerManager().ClearTimer(FountainTimer);
	FTSTicker::GetCoreTicker().RemoveTicker(AbandonmentTicker);
	// Nothing on the battleground happens after the end: no rebuilds, no regeneration (Economy Bible §8.2).
	if (Battleground)
	{
		Battleground->Stop();
	}
	// Every player commands its Vanguard again at the end.
	if (EchoLink)
	{
		EchoLink->Stop();
	}
	if (UVeyraAbsenceSubsystem* Absence = GetWorld()->GetSubsystem<UVeyraAbsenceSubsystem>())
	{
		Absence->OnBecameAfk.Remove(BecameAfkHandle);
		Absence->OnCameBack.Remove(CameBackHandle);
		Absence->Stop();
	}
	if (UVeyraVoteSubsystem* Votes = GetWorld()->GetSubsystem<UVeyraVoteSubsystem>())
	{
		Votes->OnVotePassed.Remove(VotePassedHandle);
		Votes->OnIntermissionOver.Remove(IntermissionOverHandle);
		Votes->Stop();
	}
	if (UVeyraMatchStatisticsSubsystem* Statistics = GetWorld()->GetSubsystem<UVeyraMatchStatisticsSubsystem>())
	{
		Statistics->Stop();
	}
	State.SetPhase(EVeyraMatchPhase::Ended);
	// The clock is frozen now; an ended match need not stay paused.
	if (State.IsMatchPaused())
	{
		ResumeMatch();
	}

	FVeyraMatchResult Result;
	Result.EndReason = Reason;
	Result.Winner = Winner;
	Result.DurationSeconds = State.GetMatchClockSeconds();
	if (Roster)
	{
		Result.MatchId = Roster->GetAssignment().MatchId;
		Result.Participants = Roster->BuildParticipantResults();
		// Each one's own outcome besides its team's (Match Flow Bible §6; ADR-019 §3).
		const UVeyraAbsenceSubsystem* Absence = GetWorld()->GetSubsystem<UVeyraAbsenceSubsystem>();
		for (FVeyraParticipantResult& Participant : Result.Participants)
		{
			const AVeyraPlayerState* Player = FindParticipant(Participant.AccountId);
			if (Absence && Player)
			{
				const UVeyraAbsenceSubsystem::FPersonalResult Personal = Absence->Adjudicate(*Player, Winner != EVeyraTeam::None && Winner == Player->GetVeyraTeam(),
					Result.DurationSeconds);
				Participant.bPersonalLoss = Personal.bPersonalLoss;
				Participant.AbsentSeconds = Personal.AbsentSeconds;
			}
		}
	}
	Result.Players = BuildScoreboard();
	if (const UVeyraMatchStatisticsSubsystem* Statistics = GetWorld()->GetSubsystem<UVeyraMatchStatisticsSubsystem>())
	{
		Result.Wells = Statistics->GetWellCaptures();
	}
	// Game/Scripts/Smoke.ps1 checks this line.
	UE_LOG(LogVeyraMatch, Display, TEXT("The match ended (%s) after %.1f s of match clock%s."), LexToString(Reason), Result.DurationSeconds,
		Winner == EVeyraTeam::None ? TEXT("") : *FString::Printf(TEXT("; team %s won"), *StaticEnum<EVeyraTeam>()->GetNameStringByValue(static_cast<int64>(Winner))));
	UVeyraMatchHostSubsystem* Host = UVeyraMatchHostSubsystem::Get();
	if (Host && GetNetMode() == NM_DedicatedServer)
	{
		Host->OnMatchEnded.Broadcast(Result);
	}
}

TArray<FVeyraPlayerResult> AVeyraGameMode::BuildScoreboard() const
{
	// From the statistics service's records, not the PlayerStates still here: one who left has none.
	const UVeyraMatchStatisticsSubsystem* Statistics = GetWorld()->GetSubsystem<UVeyraMatchStatisticsSubsystem>();
	return Statistics ? Statistics->BuildScoreboard() : TArray<FVeyraPlayerResult>();
}

EVeyraEndCustomMatchRefusal AVeyraGameMode::HandleEndCustomMatch(const APlayerController& Requester)
{
	const AVeyraGameState& State = GetVeyraGameState();
	const EVeyraEndCustomMatchRefusal Refusal = VeyraMatchRules::CheckEndCustomMatch(State.GetMatchRules(), State.GetPhase(),
		State.GetHost() != nullptr && State.GetHost() == Requester.PlayerState);
	if (Refusal != EVeyraEndCustomMatchRefusal::None)
	{
		UE_LOG(LogVeyraMatch, Log, TEXT("Refused to end the custom match for %s: %s."), *GetNameSafe(Requester.PlayerState), LexToString(Refusal));
		return Refusal;
	}
	UE_LOG(LogVeyraMatch, Log, TEXT("%s, the host, ended the custom match."), *GetNameSafe(Requester.PlayerState));
	EndMatch(EVeyraMatchEndReason::HostEnded);
	return Refusal;
}

void AVeyraGameMode::OnPrimeWellDestroyed(EVeyraTeam Winner)
{
	// Game/Scripts/Smoke.ps1 checks this line.
	UE_LOG(LogVeyraMatch, Display, TEXT("Team %s destroyed the other side's Prime Well."), *StaticEnum<EVeyraTeam>()->GetNameStringByValue(static_cast<int64>(Winner)));
	const AVeyraGameState& State = GetVeyraGameState();
	if (VeyraMatchRules::DoesPrimeWellWin(State.HasVictory(), State.GetPhase()))
	{
		EndMatch(EVeyraMatchEndReason::PrimeWellDestroyed, Winner);
	}
	else
	{
		// Practice, and a custom match with victory off, have none: the Well stays destroyed and the
		// match goes on (ADR-011 §14; ADR-021 §3).
		UE_LOG(LogVeyraMatch, Log, TEXT("The %s match goes on: it has no victory condition now."),
			*StaticEnum<EVeyraMatchRules>()->GetNameStringByValue(static_cast<int64>(State.GetMatchRules())));
	}
}

bool AVeyraGameMode::HandleDeveloperSiege(const APlayerController& Requester)
{
	const AVeyraPlayerState* PlayerState = Requester.GetPlayerState<AVeyraPlayerState>();
	UAbilitySystemComponent* Source = PlayerState ? PlayerState->GetAbilitySystemComponent() : nullptr;
	if (!Source || !Battleground || CheckOrdersAllowed() != EVeyraOrderRejection::None)
	{
		return false;
	}
	return Battleground->DeveloperSiege(*Source, PlayerState->GetVeyraTeam());
}

void AVeyraGameMode::NoteConnectedParticipants()
{
	if (Roster->NumConnected() > 0)
	{
		NobodyConnectedSince.Reset();
	}
	else if (!NobodyConnectedSince.IsSet())
	{
		NobodyConnectedSince = FPlatformTime::Seconds();
	}
}

bool AVeyraGameMode::TickAbandonment(float /*DeltaSeconds*/)
{
	if (NobodyConnectedSince.IsSet()
		&& FPlatformTime::Seconds() - NobodyConnectedSince.GetValue() >= UVeyraMatchTuningSubsystem::Get().Lifecycle.AbandonAfterSeconds)
	{
		EndMatch(EVeyraMatchEndReason::Abandoned);
		return false;
	}
	return true;
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
	// Preparation takes only move orders, inside the fountain (HandleMoveOrder; ADR-054 §1).
	return GetVeyraGameState().GetPhase() == EVeyraMatchPhase::Live ? EVeyraOrderRejection::None : EVeyraOrderRejection::WrongPhase;
}

namespace
{
	bool IsUsableOrderPoint(const FVector& Point)
	{
		return !Point.ContainsNaN() && FMath::IsFinite(Point.X) && FMath::IsFinite(Point.Y) && FMath::IsFinite(Point.Z);
	}

	AVeyraVanguardController* VanguardControllerOf(const AVeyraPlayerState* Participant)
	{
		return Participant ? Participant->GetVanguardController() : nullptr;
	}

	/**
	 * An order the Vanguard takes ends its Recall channel (ADR-012 §8); a refused one changes nothing.
	 * Returns Rejection, so each order path can pass its result through.
	 */
	template <typename RejectionType>
	RejectionType EndRecallIfTaken(const AVeyraPlayerState* Participant, RejectionType Rejection)
	{
		UVeyraRecallComponent* Recall = Participant ? Participant->FindComponentByClass<UVeyraRecallComponent>() : nullptr;
		if (Rejection == RejectionType::None && Recall)
		{
			Recall->Interrupt();
		}
		return Rejection;
	}

	/**
	 * An order the Vanguard takes is meaningful activity, a move only if it goes somewhere new (Match
	 * Flow Bible §5.1; ADR-019 §3). Returns Rejection.
	 */
	template <typename RejectionType>
	RejectionType NoteActivityIfTaken(const AVeyraPlayerState* Participant, RejectionType Rejection, const TOptional<FVector>& MoveDestination = {})
	{
		UVeyraAbsenceSubsystem* Absence = Participant && Participant->GetWorld() ? Participant->GetWorld()->GetSubsystem<UVeyraAbsenceSubsystem>() : nullptr;
		if (Rejection == RejectionType::None && Absence)
		{
			Absence->NoteActivity(*Participant, MoveDestination);
		}
		return Rejection;
	}
}

AVeyraVanguardController* AVeyraGameMode::OrderedControllerOf(const AVeyraPlayerState* Participant) const
{
	return EchoLink ? EchoLink->CommandedControllerOf(Participant) : VanguardControllerOf(Participant);
}

EVeyraOrderRejection AVeyraGameMode::HandleMoveOrder(AVeyraPlayerState* Participant, const FVector& Destination)
{
	// In fountain preparation a Vanguard may move, but not leave its fountain (Match Flow Bible §1; ADR-054 §1).
	const bool bPreparing = !GetWorld()->IsPaused() && GetVeyraGameState().GetPhase() == EVeyraMatchPhase::Preparation;
	const EVeyraOrderRejection Allowed = bPreparing ? EVeyraOrderRejection::None : CheckOrdersAllowed();
	if (Allowed != EVeyraOrderRejection::None)
	{
		return Allowed;
	}
	if (!IsUsableOrderPoint(Destination))
	{
		return EVeyraOrderRejection::InvalidOrder;
	}
	FVector Goal = Destination;
	if (bPreparing)
	{
		// The circle holds every path between its points, so a destination inside it keeps the walk inside.
		const AActor* Start = Participant ? FindTeamStart(Participant->GetVeyraTeam()) : nullptr;
		if (!Start)
		{
			return EVeyraOrderRejection::WrongPhase;
		}
		Goal = VeyraMatchRules::ClampToFountain(Destination, Start->GetActorLocation(), UVeyraMatchTuningSubsystem::Get().Fountain.Radius);
	}
	AVeyraVanguardController* Controller = OrderedControllerOf(Participant);
	return NoteActivityIfTaken(Participant, EndRecallIfTaken(Participant, Controller ? Controller->MoveToDestination(Goal) : EVeyraOrderRejection::NoVanguard),
		TOptional<FVector>(Goal));
}

EVeyraOrderRejection AVeyraGameMode::HandleAttackOrder(AVeyraPlayerState* Participant, AActor* Target)
{
	const EVeyraOrderRejection Allowed = CheckOrdersAllowed();
	if (Allowed != EVeyraOrderRejection::None)
	{
		return Allowed;
	}
	if (!Target)
	{
		return EVeyraOrderRejection::InvalidOrder;
	}
	AVeyraVanguardController* Controller = OrderedControllerOf(Participant);
	return NoteActivityIfTaken(Participant, EndRecallIfTaken(Participant, Controller ? Controller->AttackUnit(*Target) : EVeyraOrderRejection::NoVanguard));
}

EVeyraOrderRejection AVeyraGameMode::HandleAttackMoveOrder(AVeyraPlayerState* Participant, const FVector& Destination, EVeyraAttackMoveTarget Preference)
{
	const EVeyraOrderRejection Allowed = CheckOrdersAllowed();
	if (Allowed != EVeyraOrderRejection::None)
	{
		return Allowed;
	}
	if (!IsUsableOrderPoint(Destination))
	{
		return EVeyraOrderRejection::InvalidOrder;
	}
	AVeyraVanguardController* Controller = OrderedControllerOf(Participant);
	return NoteActivityIfTaken(Participant, EndRecallIfTaken(Participant, Controller ? Controller->AttackMoveTo(Destination, Preference) : EVeyraOrderRejection::NoVanguard),
		TOptional<FVector>(Destination));
}

EVeyraCastRejection AVeyraGameMode::HandleCastOrder(AVeyraPlayerState* Participant, EVeyraAbilitySlot Slot, const FVeyraCastTarget& Target)
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
	UAbilitySystemComponent* AbilitySystem = Participant ? Participant->GetAbilitySystemComponent() : nullptr;
	// Commanding its Echo, the participant casts through it: one eligible ability, never an item or a spell (ADR-050 §5).
	UVeyraEchoSubsystem* Echoes = GetWorld()->GetSubsystem<UVeyraEchoSubsystem>();
	if (AbilitySystem && Echoes && EchoLink && EchoLink->IsCommanding(Participant))
	{
		return NoteActivityIfTaken(Participant, Echoes->CastFrom(*AbilitySystem, Slot, Target));
	}
	const EVeyraCastRejection Rejection =
		NoteActivityIfTaken(Participant, EndRecallIfTaken(Participant, AbilitySystem ? VeyraAbilities::TryCast(*AbilitySystem, Slot, Target) : EVeyraCastRejection::UnknownAbility));
	// A cast item Active has given benefit, so its purchase can no longer be undone (Item Bible §12), whoever cast it:
	// a player or a bot (ADR-056 §5).
	if (Rejection == EVeyraCastRejection::None && Participant && VeyraAbilitySlots::IsItemSlot(Slot))
	{
		UVeyraShopSubsystem::NoteActiveUsed(*Participant, VeyraAbilitySlots::ItemIndexOf(Slot));
	}
	return Rejection;
}

EVeyraOrderRejection AVeyraGameMode::HandleRecallOrder(AVeyraPlayerState* PlayerState)
{
	const EVeyraOrderRejection Allowed = CheckOrdersAllowed();
	if (Allowed != EVeyraOrderRejection::None)
	{
		return Allowed;
	}
	AVeyraVanguardController* Controller = PlayerState ? PlayerState->GetVanguardController() : nullptr;
	UVeyraRecallComponent* Recall = PlayerState ? PlayerState->FindComponentByClass<UVeyraRecallComponent>() : nullptr;
	if (!Controller || !Controller->GetPawn() || !Recall || !VeyraTargeting::IsAlive(PlayerState))
	{
		return EVeyraOrderRejection::NoVanguard;
	}
	// A rider leaves the ride before it may recall (Combat Bible §56).
	if (const UAbilitySystemComponent* Rider = PlayerState->GetAbilitySystemComponent(); Rider && VeyraCombat::IsRiding(*Rider))
	{
		return EVeyraOrderRejection::Mounted;
	}
	// Recall is cast like an ability, so what stops casting stops it beginning.
	const UVeyraStatusComponent* Statuses = PlayerState->FindComponentByClass<UVeyraStatusComponent>();
	if (Statuses && EnumHasAnyFlags(Statuses->GetActionBlocks(), EVeyraActionBlocks::Cast))
	{
		return EVeyraOrderRejection::CrowdControlled;
	}
	// Nor while another cast holds the Vanguard, in its windup, channel or recovery: that cast would
	// go on beside the channel.
	const UVeyraCastStateComponent* CastState = PlayerState->FindComponentByClass<UVeyraCastStateComponent>();
	if (CastState && CastState->IsBusy())
	{
		return EVeyraOrderRejection::Casting;
	}
	if (Recall->IsRecalling())
	{
		return NoteActivityIfTaken(PlayerState, EVeyraOrderRejection::None);
	}
	Controller->StopOrders();
	Recall->Start(UVeyraMatchTuningSubsystem::Get().Recall.ChannelSeconds,
		FSimpleDelegate::CreateUObject(this, &AVeyraGameMode::CompleteRecall, TWeakObjectPtr<AVeyraPlayerState>(PlayerState)));
	return NoteActivityIfTaken(PlayerState, EVeyraOrderRejection::None);
}

EVeyraOrderRejection AVeyraGameMode::HandleVisionToolOrder(AVeyraPlayerState* PlayerState, const FVector& Point)
{
	const EVeyraOrderRejection Allowed = CheckOrdersAllowed();
	if (Allowed != EVeyraOrderRejection::None)
	{
		return Allowed;
	}
	UVeyraVisionToolComponent* Tool = PlayerState ? PlayerState->FindComponentByClass<UVeyraVisionToolComponent>() : nullptr;
	if (!Tool)
	{
		return EVeyraOrderRejection::NoVanguard;
	}
	// Its Vanguard waits in Stasis while it commands its Echo, which carries no tool (ADR-050 §5).
	if (EchoLink && EchoLink->IsCommanding(PlayerState))
	{
		return EVeyraOrderRejection::CrowdControlled;
	}
	switch (Tool->Use(Point))
	{
	case EVeyraVisionToolRejection::None:
		return NoteActivityIfTaken(PlayerState, EVeyraOrderRejection::None);
	case EVeyraVisionToolRejection::NoVanguard:
		return EVeyraOrderRejection::NoVanguard;
	case EVeyraVisionToolRejection::CrowdControlled:
		return EVeyraOrderRejection::CrowdControlled;
	case EVeyraVisionToolRejection::NoCharge:
	case EVeyraVisionToolRejection::CoolingDown:
		return EVeyraOrderRejection::NotReady;
	case EVeyraVisionToolRejection::InvalidPoint:
		return EVeyraOrderRejection::InvalidOrder;
	}
	return EVeyraOrderRejection::InvalidOrder;
}

void AVeyraGameMode::CompleteRecall(TWeakObjectPtr<AVeyraPlayerState> PlayerState)
{
	APawn* Body = PlayerState.IsValid() ? PlayerState->GetPawn() : nullptr;
	const AActor* Start = Body ? FindTeamStart(PlayerState->GetVeyraTeam()) : nullptr;
	// A channel that outlasts the match, or its Vanguard, brings no one home.
	if (!Start || !VeyraTargeting::IsAlive(PlayerState.Get()) || GetVeyraGameState().GetPhase() != EVeyraMatchPhase::Live)
	{
		return;
	}
	if (AVeyraVanguardController* Controller = PlayerState->GetVanguardController())
	{
		Controller->StopOrders();
	}
	if (!Body->TeleportTo(Start->GetActorLocation(), Start->GetActorRotation()))
	{
		return;
	}
	// Arriving opens its shop at once, not at the next fountain check: until then it would seem
	// still away, and a bot would recall again.
	if (UVeyraShopSubsystem* Shop = GetWorld()->GetSubsystem<UVeyraShopSubsystem>())
	{
		Shop->SetAtFountain(*PlayerState, true);
	}
	UE_LOG(LogVeyraMatch, Log, TEXT("%s recalls to its fountain."), *PlayerState->GetPlayerName());
}

bool AVeyraGameMode::PauseMatch(APlayerController& Requester)
{
	// The engine's world pause stops every gameplay clock on the server; the GameState tells clients.
	if (GetVeyraGameState().GetPhase() == EVeyraMatchPhase::Ended || GetVeyraGameState().IsMatchPaused() || !SetPause(&Requester))
	{
		return false;
	}
	GetVeyraGameState().SetMatchPaused(true);
	UE_LOG(LogVeyraMatch, Log, TEXT("Match paused by %s."), *GetNameSafe(Requester.PlayerState));
	return true;
}

void AVeyraGameMode::OnVotePassed(EVeyraVoteKind Kind, EVeyraTeam Team)
{
	UVeyraVoteSubsystem* Votes = GetWorld()->GetSubsystem<UVeyraVoteSubsystem>();
	switch (Kind)
	{
	case EVeyraVoteKind::Remake:
		EndMatch(EVeyraMatchEndReason::Remake);
		break;
	case EVeyraVoteKind::Surrender:
		EndMatch(EVeyraMatchEndReason::Surrender, VeyraTeams::Opposing(Team));
		break;
	case EVeyraVoteKind::Pause:
	{
		// The engine's pause names a player; any connected one stands for the vote.
		APlayerController* Requester = GetWorld()->GetFirstPlayerController();
		if (Requester && PauseMatch(*Requester) && Votes)
		{
			Votes->BeginIntermission();
		}
		break;
	}
	case EVeyraVoteKind::Resume:
		OnIntermissionOver();
		break;
	}
}

void AVeyraGameMode::OnBecameAfk(AVeyraPlayerState& Participant, const FVeyraAbsenceRecord& /*Record*/)
{
	if (AVeyraPlayerController* Player = Cast<AVeyraPlayerController>(Participant.GetOwner()))
	{
		Player->WarnAfk(true);
	}
}

void AVeyraGameMode::OnCameBack(const AVeyraPlayerState& Participant)
{
	if (AVeyraPlayerController* Player = Cast<AVeyraPlayerController>(Participant.GetOwner()))
	{
		Player->WarnAfk(false);
	}
}

void AVeyraGameMode::OnIntermissionOver()
{
	ResumeMatch();
	if (UVeyraVoteSubsystem* Votes = GetWorld()->GetSubsystem<UVeyraVoteSubsystem>())
	{
		Votes->EndIntermission();
	}
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
	// A participant with a side already is one returning, to the Vanguard it left (ADR-019 §1).
	const bool bReturning = PlayerState->GetVeyraTeam() != EVeyraTeam::None;
	UVeyraAbsenceSubsystem* Absence = GetWorld()->GetSubsystem<UVeyraAbsenceSubsystem>();
	if (!bReturning)
	{
		AssignTeam(*PlayerState);
		AssignVanguard(*PlayerState);
		if (Absence)
		{
			Absence->Track(*PlayerState);
		}
	}
	else if (Absence)
	{
		Absence->NoteReturned(*PlayerState);
	}
	const AVeyraVanguardController* Controller = PlayerState->GetVanguardController();
	if (GetVeyraGameState().GetPhase() != EVeyraMatchPhase::Loading && !(Controller && Controller->GetPawn()))
	{
		SpawnVanguard(*PlayerState);
	}
}

AVeyraPlayerState* AVeyraGameMode::FindParticipant(FStringView AccountId) const
{
	for (APlayerState* Member : GameState->PlayerArray)
	{
		AVeyraPlayerState* Candidate = Cast<AVeyraPlayerState>(Member);
		if (Candidate && Candidate->GetAccountId() == AccountId)
		{
			return Candidate;
		}
	}
	return nullptr;
}

AVeyraPlayerState* AVeyraGameMode::FindKeptPlayerState(FStringView AccountId) const
{
	for (APlayerState* Member : GameState->PlayerArray)
	{
		AVeyraPlayerState* Candidate = Cast<AVeyraPlayerState>(Member);
		if (Candidate && Candidate->IsInactive() && Candidate->GetAccountId() == AccountId)
		{
			return Candidate;
		}
	}
	return nullptr;
}

void AVeyraGameMode::GiveBackPlayerState(APlayerController& Controller, AVeyraPlayerState& Kept)
{
	APlayerState* Fresh = Controller.PlayerState;
	if (Fresh)
	{
		Kept.SetUniqueId(Fresh->GetUniqueId());
	}
	Controller.SetPlayerState(&Kept);
	Kept.SetOwner(&Controller);
	Kept.SetAway(false);
	// Its abilities act for the new controller, whose connection now owns what only its owner sees.
	if (UAbilitySystemComponent* AbilitySystem = Kept.GetAbilitySystemComponent())
	{
		AbilitySystem->RefreshAbilityActorInfo();
	}
	if (Fresh)
	{
		Fresh->Destroy();
	}
	UE_LOG(LogVeyraMatch, Log, TEXT("%s returned to the match, to the Vanguard it left."), *Kept.GetPlayerName());
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

AVeyraPlayerState* AVeyraGameMode::AddBotParticipant(const FString& Name, EVeyraTeam Side, const FVeyraContentId& Vanguard)
{
	const bool bSeated = Side == EVeyraTeam::A || Side == EVeyraTeam::B;
	if (bSeated ? CountTeamMembers(Side) >= UVeyraMatchTuningSubsystem::Get().Teams.MaxTeamSize : IsFull())
	{
		UE_LOG(LogVeyraMatch, Warning, TEXT("Cannot add %s: %s."), *Name, bSeated ? TEXT("its side is full") : TEXT("the match is full"));
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
	if (bSeated)
	{
		PlayerState->SetVeyraTeam(Side);
		UE_LOG(LogVeyraMatch, Log, TEXT("%s joins Team %s."), *Name, Side == EVeyraTeam::A ? TEXT("A") : TEXT("B"));
	}
	else
	{
		AssignTeam(*PlayerState);
	}
	if (Vanguard.IsValid())
	{
		PlayerState->SetVanguardId(Vanguard);
		UE_LOG(LogVeyraMatch, Log, TEXT("%s plays %s."), *Name, *Vanguard.ToString());
	}
	else
	{
		AssignVanguard(*PlayerState);
	}
	if (GetVeyraGameState().GetPhase() != EVeyraMatchPhase::Loading)
	{
		SpawnVanguard(*PlayerState);
	}
	return PlayerState;
}

AVeyraPlayerState* AVeyraGameMode::AddPlayingBot(const FString& Name, EVeyraTeam Side, const FVeyraContentId& Vanguard, EVeyraBotDifficulty Difficulty)
{
	// Its seat is its place among the bots its side already has.
	FVeyraBotSeat Seat;
	for (const APlayerState* Member : GameState->PlayerArray)
	{
		const AVeyraPlayerState* Other = Cast<AVeyraPlayerState>(Member);
		Seat.Seat += Other && Other->IsABot() && Other->GetVeyraTeam() == Side ? 1 : 0;
	}
	AVeyraPlayerState* Participant = AddBotParticipant(Name, Side, Vanguard);
	if (!Participant)
	{
		return nullptr;
	}
	Seat.Side = Participant->GetVeyraTeam();
	Seat.Vanguard = Participant->GetVanguardId();
	Seat.Difficulty = Difficulty;
	UE_LOG(LogVeyraMatch, Log, TEXT("%s plays as a %s bot in seat %d."), *Name, LexToString(Difficulty), Seat.Seat);
	if (UVeyraMatchEvents* Events = GetWorld()->GetSubsystem<UVeyraMatchEvents>())
	{
		Events->OnBotAdded.Broadcast(*Participant, Seat);
	}
	return Participant;
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
	// A rostered participant plays on its assigned side. Anyone else (a bot, or a player on a
	// developer server) joins the smaller side, or Team A on a tie; PreLogin has already refused
	// anyone beyond capacity.
	const FVeyraAssignedParticipant* Participant = Roster && !PlayerState.GetAccountId().IsEmpty() ? Roster->FindByAccount(PlayerState.GetAccountId()) : nullptr;
	const EVeyraTeam Team = Participant ? Participant->Side
		: CountTeamMembers(EVeyraTeam::B) < CountTeamMembers(EVeyraTeam::A) ? EVeyraTeam::B : EVeyraTeam::A;
	PlayerState.SetVeyraTeam(Team);
	UE_LOG(LogVeyraMatch, Log, TEXT("%s joins Team %s."), *PlayerState.GetPlayerName(), Team == EVeyraTeam::A ? TEXT("A") : TEXT("B"));
}

void AVeyraGameMode::AssignVanguard(AVeyraPlayerState& PlayerState)
{
	// A rostered participant plays what champion select locked; the assignment was checked against
	// Vanguards.json when the server took it.
	if (const FVeyraAssignedParticipant* Participant = Roster && !PlayerState.GetAccountId().IsEmpty() ? Roster->FindByAccount(PlayerState.GetAccountId()) : nullptr)
	{
		UE_CLOG(PlayerState.GetRequestedVanguardId().IsValid(), LogVeyraMatch, Warning,
			TEXT("%s asked for Vanguard %s, but an assigned match plays the Vanguard its roster names."), *PlayerState.GetPlayerName(),
			*PlayerState.GetRequestedVanguardId().ToString());
		PlayerState.SetVanguardId(Participant->VanguardId);
		PlayerState.SetStartingFluxSpells(Participant->FluxSpells);
		// What the mastery emote shows (ADR-045 §9).
		PlayerState.SetMastery(Participant->MasteryLevel, Participant->EmoteTier);
		UE_LOG(LogVeyraMatch, Log, TEXT("%s plays %s."), *PlayerState.GetPlayerName(), *Participant->VanguardId.ToString());
		return;
	}

	// On a developer server, developer data chooses by join order; the last entry covers later joiners.
	const TArray<FVeyraContentId>& Order = UVeyraMatchTuningSubsystem::Get().DeveloperMatch.Vanguards;
	FVeyraContentId Vanguard = Order.IsEmpty() ? FVeyraContentId() : Order[FMath::Min(VanguardsAssigned, Order.Num() - 1)];
	++VanguardsAssigned;
	const FVeyraContentId& Requested = PlayerState.GetRequestedVanguardId();
	if (Requested.IsValid())
	{
		if (UVeyraVanguardsTuningSubsystem::FindVanguard(Requested))
		{
			Vanguard = Requested;
		}
		else
		{
			UE_LOG(LogVeyraMatch, Warning, TEXT("%s asked for Vanguard %s, which Vanguards.json does not define."), *PlayerState.GetPlayerName(), *Requested.ToString());
		}
	}
	PlayerState.SetVanguardId(Vanguard);
	UE_LOG(LogVeyraMatch, Log, TEXT("%s plays %s."), *PlayerState.GetPlayerName(), *Vanguard.ToString());
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

void AVeyraGameMode::SeatNoShows()
{
	if (!Roster)
	{
		return;
	}
	// A rostered player who never connected still has its Vanguard: it starts the match disconnected,
	// follows the ordinary retreat and reconnect rules, and is there for its player to take late
	// (Match Flow Bible §3; ADR-019 §1).
	UVeyraAbsenceSubsystem* Absence = GetWorld()->GetSubsystem<UVeyraAbsenceSubsystem>();
	for (const FVeyraAssignedParticipant& Participant : Roster->GetAssignment().Participants)
	{
		if (FindParticipant(Participant.AccountId))
		{
			continue;
		}
		FActorSpawnParameters Parameters;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Parameters.ObjectFlags |= RF_Transient;
		AVeyraPlayerState* Seat = GetWorld()->SpawnActor<AVeyraPlayerState>(PlayerStateClass.Get(), Parameters);
		if (!Seat)
		{
			UE_LOG(LogVeyraMatch, Error, TEXT("Could not keep a seat for %s, who never connected."), *Participant.DisplayName);
			continue;
		}
		Seat->SetPlayerId(GameSession->GetNextPlayerID());
		Seat->SetAccountId(Participant.AccountId);
		Seat->SetPlayerName(Participant.DisplayName);
		// Kept as a disconnected participant's PlayerState is (AVeyraPlayerState::OnDeactivated).
		Seat->SetAway(true);
		AssignTeam(*Seat);
		AssignVanguard(*Seat);
		if (Absence)
		{
			Absence->Track(*Seat);
		}
		UE_LOG(LogVeyraMatch, Log, TEXT("%s never connected; its Vanguard starts the match disconnected."), *Participant.DisplayName);
	}
}

void AVeyraGameMode::AddAssignedBots()
{
	if (!Roster)
	{
		return;
	}
	const FVeyraMatchAssignment& Assignment = Roster->GetAssignment();
	int32 Added = 0;
	for (int32 Index = 0; Index < Assignment.Bots.Num(); ++Index)
	{
		const FVeyraAssignedBot& Bot = Assignment.Bots[Index];
		// Announced as it is seated, so its brain attaches (ADR-013 §2).
		Added += AddPlayingBot(FString::Printf(TEXT("Bot %d"), Index + 1), Bot.Side, Bot.VanguardId, Bot.Difficulty) ? 1 : 0;
	}
	// Game/Scripts/Smoke.ps1 checks this line in practice matches.
	UE_CLOG(!Assignment.Bots.IsEmpty(), LogVeyraMatch, Display, TEXT("Added %d of the assignment's %d bot(s)."), Added, Assignment.Bots.Num());
}

void AVeyraGameMode::BeginPreparation()
{
	SetActorTickEnabled(false);
	GetWorldTimerManager().ClearTimer(LoadingTimeout);
	// Before the phase changes, so each no-show and each bot gets its Vanguard below with everyone else.
	SeatNoShows();
	AddAssignedBots();
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
	// A world-time timer, so a pause holds it.
	GetWorldTimerManager().SetTimer(FountainTimer, this, &AVeyraGameMode::RecoverAtFountains,
		static_cast<float>(UVeyraMatchTuningSubsystem::Get().Fountain.IntervalSeconds), /*bLoop*/ true);
}

void AVeyraGameMode::BeginLive()
{
	GetVeyraGameState().SetPhase(EVeyraMatchPhase::Live);
	UE_LOG(LogVeyraMatch, Log, TEXT("The match is live."));
	// Absence counts from 0:00 in a standard match (Match Flow Bible §3–§5); a practice match's host
	// ends it instead (Custom Matches Bible §4).
	UVeyraAbsenceSubsystem* Absence = GetWorld()->GetSubsystem<UVeyraAbsenceSubsystem>();
	// Votes are taken while it runs (Match Flow Bible §7–§10); their rules refuse a practice match's.
	if (UVeyraVoteSubsystem* Votes = GetWorld()->GetSubsystem<UVeyraVoteSubsystem>())
	{
		VotePassedHandle = Votes->OnVotePassed.AddUObject(this, &AVeyraGameMode::OnVotePassed);
		IntermissionOverHandle = Votes->OnIntermissionOver.AddUObject(this, &AVeyraGameMode::OnIntermissionOver);
		Votes->Start();
	}
	if (Absence && GetVeyraGameState().GetMatchRules() == EVeyraMatchRules::Standard)
	{
		BecameAfkHandle = Absence->OnBecameAfk.AddUObject(this, &AVeyraGameMode::OnBecameAfk);
		CameBackHandle = Absence->OnCameBack.AddUObject(this, &AVeyraGameMode::OnCameBack);
		Absence->Start();
	}
	// Passive Gold runs with the live match (author ruling, 2026-09-28); the battleground link stops
	// it with every other reward when the match ends.
	if (UVeyraRewardSubsystem* Rewards = GetWorld()->GetSubsystem<UVeyraRewardSubsystem>())
	{
		Rewards->StartPassiveGold();
	}
	// The Fluxborn waves and the jungle's camps begin with the match clock (Battleground Bible §17).
	if (Battleground)
	{
		Battleground->StartLive();
	}
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
	const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(AbilitySystem, PlayerState.GetVanguardId());
	if (!Prepared.bPrepared)
	{
		UE_LOG(LogVeyraMatch, Error, TEXT("Could not prepare %s for the match; see the errors above."), *PlayerState.GetPlayerName());
		return false;
	}
	PlayerState.SetPassive(Prepared.Passive);

	EquipFluxSpells(PlayerState, AbilitySystem);

	// A developer match may spend the level-1 skill point for the player, so the Vanguard can cast at once.
	UVeyraProgressionComponent* Progression = PlayerState.FindComponentByClass<UVeyraProgressionComponent>();
	switch (UVeyraMatchTuningSubsystem::Get().DeveloperMatch.StartingRank)
	{
	case EVeyraDeveloperStartingRank::Q:
		Progression->AllocateRank(EVeyraAbilitySlot::Q);
		break;
	case EVeyraDeveloperStartingRank::W:
		Progression->AllocateRank(EVeyraAbilitySlot::W);
		break;
	case EVeyraDeveloperStartingRank::E:
		Progression->AllocateRank(EVeyraAbilitySlot::E);
		break;
	case EVeyraDeveloperStartingRank::None:
		break;
	}
	// Its record begins before its starting Gold, which counts as earned (ADR-017 §9.1).
	if (UVeyraMatchStatisticsSubsystem* Statistics = GetWorld()->GetSubsystem<UVeyraMatchStatisticsSubsystem>())
	{
		Statistics->AddParticipant(PlayerState);
	}
	// Its player sees the Gold it earns as numbers (ADR-065 §4).
	if (CombatText)
	{
		CombatText->WatchGold(PlayerState);
	}
	// The one guaranteed Gold, once per match (Economy & Progression Bible §1), and empty slots to spend it on (§10).
	// A custom match's host may set its own starting Gold (ADR-021 §3).
	const TOptional<FVeyraCustomSettings> Custom = Roster ? Roster->GetAssignment().Custom : TOptional<FVeyraCustomSettings>();
	UVeyraRewardSubsystem::GrantStartingGold(PlayerState, Custom.IsSet() ? Custom->StartingGold : TOptional<double>());
	UVeyraShopSubsystem::InitializeInventory(PlayerState);
	// Its spell slots start as open as its team's permanent Flux has made them (ADR-015 §4).
	if (Battleground)
	{
		Battleground->UnlockSpellSlots(PlayerState);
	}
	PlayerState.MarkStatsInitialized();
	return true;
}

void AVeyraGameMode::EquipFluxSpells(AVeyraPlayerState& PlayerState, UAbilitySystemComponent& AbilitySystem) const
{
	// The Flux Spells it chose, equipped in its spell slots, locked until its team's Flux opens them (ADR-015 §5).
	UVeyraAbilityLoadoutComponent* Loadout = PlayerState.FindComponentByClass<UVeyraAbilityLoadoutComponent>();
	const TArray<FVeyraContentId>& Spells = PlayerState.GetStartingFluxSpells();
	for (int32 Index = 0; Loadout && Index < Spells.Num() && Index < static_cast<int32>(UE_ARRAY_COUNT(VeyraAbilitySlots::Spells)); ++Index)
	{
		if (Spells[Index].IsValid() && !Loadout->Grant(AbilitySystem, VeyraAbilitySlots::Spells[Index], Spells[Index]))
		{
			UE_LOG(LogVeyraMatch, Error, TEXT("Could not equip %s's Flux Spell %s."), *PlayerState.GetPlayerName(), *Spells[Index].ToString());
		}
	}
	if (Spells.ContainsByPredicate([](const FVeyraContentId& Spell) { return Spell.IsValid(); }))
	{
		TArray<FString> Names;
		for (const FVeyraContentId& Spell : Spells)
		{
			Names.Add(Spell.IsValid() ? Spell.ToString() : TEXT("(empty)"));
		}
		UE_LOG(LogVeyraMatch, Log, TEXT("%s takes Flux Spells %s into the match."), *PlayerState.GetPlayerName(), *FString::Join(Names, TEXT(", ")));
	}
}

bool AVeyraGameMode::EquipStartingFluxSpells(AVeyraPlayerState& Participant, TArray<FVeyraContentId> Spells)
{
	const FString Problem = VeyraMatchRules::CheckAssignedFluxSpells(Spells, UVeyraAbilitiesTuningSubsystem::Get().FluxSpells.Roster);
	if (!Problem.IsEmpty())
	{
		UE_LOG(LogVeyraMatch, Error, TEXT("Refused %s's starting Flux Spells: %s."), *Participant.GetPlayerName(), *Problem);
		return false;
	}
	Participant.SetStartingFluxSpells(MoveTemp(Spells));
	// Already spawned, as a bot seated in preparation is: equip them now.
	if (UAbilitySystemComponent* AbilitySystem = Participant.HasInitializedStats() ? Participant.GetAbilitySystemComponent() : nullptr)
	{
		EquipFluxSpells(Participant, *AbilitySystem);
	}
	return true;
}

EVeyraOrderRejection AVeyraGameMode::CheckRankUpAllowed() const
{
	// Skill points may be spent in preparation too (Match Flow Bible §1), but not while paused or after the end.
	if (GetWorld()->IsPaused())
	{
		return EVeyraOrderRejection::Paused;
	}
	const EVeyraMatchPhase Phase = GetVeyraGameState().GetPhase();
	return Phase == EVeyraMatchPhase::Preparation || Phase == EVeyraMatchPhase::Live ? EVeyraOrderRejection::None : EVeyraOrderRejection::WrongPhase;
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
	// The timer grows with the Vanguard's level and the match clock (Economy & Progression Bible §14).
	const UVeyraProgressionComponent* Progression = PlayerState->FindComponentByClass<UVeyraProgressionComponent>();
	const int32 Level = Progression && Progression->IsInitialized() ? Progression->GetLevel() : 1;
	const double Delay = VeyraMatchRules::RespawnDelaySeconds(Level, GetVeyraGameState().GetMatchClockSeconds(), UVeyraMatchTuningSubsystem::Get().Respawn);
	PlayerState->SetRespawnAt(GetWorld()->GetTimeSeconds() + Delay);
	// Purchases waiting for the fountain are delivered there now, for use after the respawn (§11.2).
	if (UVeyraShopSubsystem* Shop = GetWorld()->GetSubsystem<UVeyraShopSubsystem>())
	{
		Shop->DeliverOnDeath(*PlayerState);
	}
	UE_LOG(LogVeyraMatch, Log, TEXT("%s died at level %d; respawning in %g s."), *PlayerState->GetPlayerName(), Level, Delay);
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
		GetWorldTimerManager().SetTimer(RespawnTimers.FindOrAdd(PlayerState), FTimerDelegate::CreateUObject(this, &AVeyraGameMode::Respawn, Participant),
			static_cast<float>(Delay), /*bLoop*/ false);
	}
}

EVeyraBuybackRefusal AVeyraGameMode::HandleBuybackOrder(AVeyraPlayerState* PlayerState)
{
	const AVeyraGameState& State = GetVeyraGameState();
	UVeyraBuybackComponent* Buyback = PlayerState ? PlayerState->FindComponentByClass<UVeyraBuybackComponent>() : nullptr;
	UVeyraGoldComponent* Gold = PlayerState ? PlayerState->FindComponentByClass<UVeyraGoldComponent>() : nullptr;
	// Practice has none (ADR-020, open items), and like the shop it waits out a pause (Match Flow Bible §10.2).
	if (!Buyback || !Gold || !VeyraMatchRules::AllowsBuyback(State.GetMatchRules()) || State.GetPhase() != EVeyraMatchPhase::Live || GetWorld()->IsPaused())
	{
		return EVeyraBuybackRefusal::Unavailable;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const EVeyraBuybackRefusal Refusal = Buyback->Buy(State.GetMatchClockSeconds(), Now, !VeyraTargeting::IsAlive(PlayerState), *Gold);
	if (Refusal != EVeyraBuybackRefusal::None)
	{
		return Refusal;
	}
	UE_LOG(LogVeyraMatch, Log, TEXT("%s bought back."), *PlayerState->GetPlayerName());
	NoteActivityIfTaken(PlayerState, EVeyraOrderRejection::None);
	RespawnNow(*PlayerState);
	return EVeyraBuybackRefusal::None;
}

bool AVeyraGameMode::HandleDeveloperRespawn(const APlayerController& Requester)
{
	AVeyraPlayerState* PlayerState = Requester.GetPlayerState<AVeyraPlayerState>();
	if (!PlayerState || VeyraTargeting::IsAlive(PlayerState) || CheckOrdersAllowed() != EVeyraOrderRejection::None)
	{
		return false;
	}
	UE_LOG(LogVeyraMatch, Log, TEXT("%s asked for a developer respawn."), *PlayerState->GetPlayerName());
	RespawnNow(*PlayerState);
	return VeyraTargeting::IsAlive(PlayerState);
}

void AVeyraGameMode::RespawnNow(AVeyraPlayerState& PlayerState)
{
	// The waiting respawn is spent: it must never bring back a later death early.
	if (FTimerHandle* Timer = RespawnTimers.Find(&PlayerState))
	{
		GetWorldTimerManager().ClearTimer(*Timer);
	}
	PlayerState.SetRespawnAt(GetWorld()->GetTimeSeconds());
	Respawn(&PlayerState);
}

void AVeyraGameMode::RecoverAtFountains()
{
	const FVeyraFountainTuning& Fountain = UVeyraMatchTuningSubsystem::Get().Fountain;
	UVeyraShopSubsystem* Shop = GetWorld()->GetSubsystem<UVeyraShopSubsystem>();
	for (APlayerState* Member : GameState->PlayerArray)
	{
		AVeyraPlayerState* PlayerState = Cast<AVeyraPlayerState>(Member);
		const APawn* Body = PlayerState ? PlayerState->GetPawn() : nullptr;
		const AActor* Start = Body ? FindTeamStart(PlayerState->GetVeyraTeam()) : nullptr;
		UAbilitySystemComponent* AbilitySystem = PlayerState ? PlayerState->GetAbilitySystemComponent() : nullptr;
		// In preparation no Vanguard leaves its fountain, whatever route navigation found near the rim: one found outside
		// is brought back to the edge and stops (Match Flow Bible §1; ADR-054 §1).
		if (Start && GetVeyraGameState().GetPhase() == EVeyraMatchPhase::Preparation
			&& FVector::Dist2D(Body->GetActorLocation(), Start->GetActorLocation()) > Fountain.Radius)
		{
			if (APawn* Straying = PlayerState->GetPawn())
			{
				Straying->SetActorLocation(VeyraMatchRules::ClampToFountain(Straying->GetActorLocation(), Start->GetActorLocation(), Fountain.Radius),
					/*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
			}
			if (AVeyraVanguardController* Controller = PlayerState->GetVanguardController())
			{
				Controller->StopOrders();
			}
		}
		// A rider uses the fountain only once it leaves the ride (Combat Bible §56).
		const bool bRiding = AbilitySystem && VeyraCombat::IsRiding(*AbilitySystem);
		const bool bAtFountain = Start && !bRiding && FVector::Dist2D(Body->GetActorLocation(), Start->GetActorLocation()) <= Fountain.Radius;
		// The shop receives, sells and undoes only here (Economy & Progression Bible §10, §12; ADR-012 §7).
		// The dead shop as if here (ADR-012 §9), and respawn here, so death never ends undo.
		if (Shop && PlayerState)
		{
			Shop->SetAtFountain(*PlayerState, bAtFountain || !VeyraTargeting::IsAlive(PlayerState));
		}
		if (!bAtFountain || !AbilitySystem)
		{
			continue;
		}
		// The fountain fills the ward charges at once (Vision Bible §4).
		if (UVeyraVisionToolComponent* Tool = PlayerState->FindComponentByClass<UVeyraVisionToolComponent>())
		{
			Tool->RefillWardCharges();
		}
		// The restore verbs refuse the dead and never overfill.
		VeyraCombat::RestoreHealth(*AbilitySystem,
			AbilitySystem->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) * Fountain.HealthFractionPerSecond * Fountain.IntervalSeconds);
		// A kept resource, as Charge, only effects fill (ADR-033 §1).
		if (AbilitySystem->HasAttributeSetForAttribute(UVeyraResourceSet::GetMaxResourceAttribute()) && !VeyraCombat::IsResourceKept(*AbilitySystem))
		{
			VeyraCombat::RestoreResource(*AbilitySystem,
				AbilitySystem->GetNumericAttribute(UVeyraResourceSet::GetMaxResourceAttribute()) * Fountain.ResourceFractionPerSecond * Fountain.IntervalSeconds);
		}
	}
}

void AVeyraGameMode::Respawn(TWeakObjectPtr<AVeyraPlayerState> PlayerState)
{
	UAbilitySystemComponent* AbilitySystem = PlayerState.IsValid() ? PlayerState->GetAbilitySystemComponent() : nullptr;
	if (!AbilitySystem || !VeyraCombat::Revive(*AbilitySystem))
	{
		return;
	}
	// What it bought while dead takes effect now (ADR-012 §9), and it comes back with its ward charges (Vision Bible §4).
	UVeyraShopSubsystem::ApplyItems(*PlayerState);
	if (UVeyraVisionToolComponent* Tool = PlayerState->FindComponentByClass<UVeyraVisionToolComponent>())
	{
		Tool->RefillWardCharges();
	}
	UE_LOG(LogVeyraMatch, Log, TEXT("%s respawns."), *PlayerState->GetPlayerName());
	SpawnVanguard(*PlayerState);
}
