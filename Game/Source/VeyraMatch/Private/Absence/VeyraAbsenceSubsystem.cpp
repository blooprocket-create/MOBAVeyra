// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Absence/VeyraAbsenceSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Structures/VeyraStructure.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "VeyraBattlegroundSubsystem.h"
#include "VeyraGameState.h"
#include "VeyraMatchLog.h"
#include "VeyraPlayerState.h"
#include "VeyraTeamStart.h"
#include "VeyraVanguardController.h"

void UVeyraAbsenceSubsystem::Start()
{
	bRunning = true;
	const double Clock = Now();
	for (TPair<TWeakObjectPtr<AVeyraPlayerState>, FTracked>& Pair : Tracked)
	{
		FVeyraAbsenceRecord& Record = Pair.Value.Record;
		Record.LastActiveAt = Clock;
		// One who left before the match went live is absent from its start (Match Flow Bible §3).
		if (!Record.bConnected)
		{
			VeyraAbsence::NoteDisconnected(Record, Clock);
		}
	}
}

void UVeyraAbsenceSubsystem::Stop()
{
	bRunning = false;
}

void UVeyraAbsenceSubsystem::Track(AVeyraPlayerState& Participant)
{
	if (Participant.IsABot() || Tracked.Contains(&Participant))
	{
		return;
	}
	FTracked& Added = Tracked.Add(&Participant);
	Added.Record.bConnected = !Participant.IsInactive();
	Added.Record.LastActiveAt = Now();
}

void UVeyraAbsenceSubsystem::NoteDisconnected(const AVeyraPlayerState& Participant)
{
	FTracked* Entry = Tracked.Find(&Participant);
	if (!Entry)
	{
		return;
	}
	if (bRunning)
	{
		VeyraAbsence::NoteDisconnected(Entry->Record, Now());
	}
	else
	{
		Entry->Record.bConnected = false;
	}
}

void UVeyraAbsenceSubsystem::NoteReturned(const AVeyraPlayerState& Participant)
{
	FTracked* Entry = Tracked.Find(&Participant);
	if (!Entry)
	{
		return;
	}
	if (bRunning)
	{
		VeyraAbsence::NoteConnected(Entry->Record, Now());
	}
	else
	{
		Entry->Record.bConnected = true;
	}
	// Control comes back at once: the autopilot's walk ends where the Vanguard stands (§4).
	if (Entry->Stage != EVeyraAutopilotStage::None)
	{
		if (AVeyraVanguardController* Controller = Participant.GetVanguardController())
		{
			Controller->StopOrders();
		}
		Entry->Stage = EVeyraAutopilotStage::None;
		Entry->Heading.Reset();
	}
}

void UVeyraAbsenceSubsystem::NoteActivity(const AVeyraPlayerState& Participant, const TOptional<FVector>& MoveDestination)
{
	FTracked* Entry = Tracked.Find(&Participant);
	if (!Entry || !bRunning)
	{
		return;
	}
	if (VeyraAbsence::NoteActivity(Entry->Record, Now(), MoveDestination, UVeyraMatchTuningSubsystem::Get().Activity))
	{
		// The order itself already replaced the autopilot's walk.
		Entry->Stage = EVeyraAutopilotStage::None;
		Entry->Heading.Reset();
	}
}

const FVeyraAbsenceRecord* UVeyraAbsenceSubsystem::Find(const AVeyraPlayerState& Participant) const
{
	const FTracked* Entry = Tracked.Find(&Participant);
	return Entry ? &Entry->Record : nullptr;
}

void UVeyraAbsenceSubsystem::Tick(float /*DeltaSeconds*/)
{
	const FVeyraAbsenceTuning& Tuning = UVeyraMatchTuningSubsystem::Get().Absence;
	const double Clock = Now();
	for (TPair<TWeakObjectPtr<AVeyraPlayerState>, FTracked>& Pair : Tracked)
	{
		AVeyraPlayerState* Participant = Pair.Key.Get();
		if (!Participant)
		{
			continue;
		}
		FVeyraAbsenceRecord& Record = Pair.Value.Record;
		const VeyraAbsence::FChange Change = VeyraAbsence::Update(Record, Clock, Tuning);
		if (Change.bBecameAfk)
		{
			UE_LOG(LogVeyraMatch, Log, TEXT("%s is AFK; the autopilot walks its Vanguard to safety."), *Participant->GetPlayerName());
			OnBecameAfk.Broadcast(*Participant, Record);
		}
		if (Change.bPenalized)
		{
			UE_LOG(LogVeyraMatch, Log, TEXT("%s has been absent too long: a personal loss (Match Flow Bible §6)."), *Participant->GetPlayerName());
			OnPersonalLoss.Broadcast(*Participant, Record);
		}
		Drive(*Participant, Pair.Value.Record);
	}
}

void UVeyraAbsenceSubsystem::Drive(AVeyraPlayerState& Participant, FVeyraAbsenceRecord& Record)
{
	FTracked& Entry = Tracked.FindChecked(&Participant);
	const EVeyraAutopilotStage Stage = VeyraAbsence::StageOf(Record, Now(), UVeyraMatchTuningSubsystem::Get().Autopilot);
	AVeyraVanguardController* Controller = Participant.GetVanguardController();
	APawn* Vanguard = Controller ? Controller->GetPawn() : nullptr;
	if (Stage == EVeyraAutopilotStage::None || !Vanguard || !VeyraTargeting::IsAlive(Vanguard))
	{
		return;
	}

	TArray<FVector> Towers;
	if (const UVeyraBattlegroundSubsystem* Battleground = GetWorld()->GetSubsystem<UVeyraBattlegroundSubsystem>())
	{
		for (const AVeyraStructure* Structure : Battleground->GetStructures())
		{
			const bool bTower = Structure && (Structure->GetStructureKind() == EVeyraStructureKind::LaneSpire || Structure->GetStructureKind() == EVeyraStructureKind::BaseTower);
			if (bTower && !Structure->IsDestroyed() && Structure->GetVeyraTeam() == Participant.GetVeyraTeam())
			{
				Towers.Add(Structure->GetActorLocation());
			}
		}
	}
	TOptional<FVector> Home;
	for (TActorIterator<AVeyraTeamStart> It(GetWorld()); It; ++It)
	{
		if (It->GetVeyraTeam() == Participant.GetVeyraTeam())
		{
			Home = It->GetActorLocation();
			break;
		}
	}
	if (!Home.IsSet())
	{
		return;
	}
	const FVector Destination = VeyraAbsence::Destination(Stage, Vanguard->GetActorLocation(), Towers, Home.GetValue(),
		UVeyraMatchTuningSubsystem::Get().Autopilot.BehindTowerDistance);
	// It walks on only when where it is going changes: a new stage, or a tower fell.
	if (Stage == Entry.Stage && Entry.Heading.IsSet() && Entry.Heading->Equals(Destination))
	{
		return;
	}
	Entry.Stage = Stage;
	Entry.Heading = Destination;
	Controller->MoveToDestination(Destination);
	UE_LOG(LogVeyraMatch, Verbose, TEXT("%s's autopilot walks its Vanguard %s."), *Participant.GetPlayerName(),
		Stage == EVeyraAutopilotStage::Fountain ? TEXT("home") : TEXT("behind a tower"));
}

double UVeyraAbsenceSubsystem::Now() const
{
	const AVeyraGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AVeyraGameState>() : nullptr;
	return GameState ? GameState->GetMatchClockSeconds() : 0.0;
}

TStatId UVeyraAbsenceSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVeyraAbsenceSubsystem, STATGROUP_Tickables);
}
