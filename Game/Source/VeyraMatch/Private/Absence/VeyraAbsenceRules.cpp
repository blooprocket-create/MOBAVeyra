// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Absence/VeyraAbsenceRules.h"

#include "Tuning/VeyraMatchTuning.h"

namespace VeyraAbsence
{
namespace
{
	void EndAbsence(FVeyraAbsenceRecord& Record, double Now)
	{
		if (Record.AwaySince.IsSet())
		{
			Record.ClosedAbsentSeconds += FMath::Max(0.0, Now - Record.AwaySince.GetValue());
		}
		Record.AwaySince.Reset();
		Record.TriggerSince.Reset();
		Record.Absence = EVeyraAbsence::Present;
		Record.LastActiveAt = Now;
		Record.ReturnedAt = Now;
	}
}

FChange Update(FVeyraAbsenceRecord& Record, double Now, const FVeyraAbsenceTuning& Tuning)
{
	FChange Change;
	if (Record.Absence == EVeyraAbsence::Present && Record.bConnected && Now - Record.LastActiveAt >= Tuning.AfkAfterSeconds)
	{
		// AFK from the moment the threshold passed, not from when this update noticed.
		const double Since = Record.LastActiveAt + Tuning.AfkAfterSeconds;
		Record.Absence = EVeyraAbsence::Afk;
		Record.AwaySince = Since;
		Record.TriggerSince = Since;
		Change.bBecameAfk = true;
	}
	if (Record.Absence != EVeyraAbsence::Present && !Record.bPersonalLoss && Record.TriggerSince.IsSet())
	{
		const double After = Record.Absence == EVeyraAbsence::Afk ? Tuning.AfkPenaltyAfterSeconds : Tuning.DisconnectPenaltyAfterSeconds;
		if (Now - Record.TriggerSince.GetValue() >= After)
		{
			Record.bPersonalLoss = true;
			Change.bPenalized = true;
		}
	}
	return Change;
}

void NoteDisconnected(FVeyraAbsenceRecord& Record, double Now)
{
	Record.bConnected = false;
	if (Record.Absence == EVeyraAbsence::Present)
	{
		Record.AwaySince = Now;
	}
	Record.Absence = EVeyraAbsence::Disconnected;
	Record.TriggerSince = Now;
}

void NoteConnected(FVeyraAbsenceRecord& Record, double Now)
{
	Record.bConnected = true;
	if (Record.Absence == EVeyraAbsence::Disconnected)
	{
		EndAbsence(Record, Now);
	}
}

bool NoteActivity(FVeyraAbsenceRecord& Record, double Now, const TOptional<FVector>& MoveDestination, const FVeyraActivityTuning& Tuning)
{
	if (MoveDestination.IsSet())
	{
		// A move to nearly where the last one went is input, not play (Match Flow Bible §5.1).
		if (Record.LastCountedMove.IsSet()
			&& FVector::Dist2D(Record.LastCountedMove.GetValue(), MoveDestination.GetValue()) <= Tuning.MinimumMoveDistance)
		{
			return false;
		}
		Record.LastCountedMove = MoveDestination;
	}
	if (Record.Absence == EVeyraAbsence::Afk)
	{
		EndAbsence(Record, Now);
	}
	Record.LastActiveAt = Now;
	return true;
}

double AbsentSeconds(const FVeyraAbsenceRecord& Record, double Now)
{
	return Record.ClosedAbsentSeconds + (Record.AwaySince.IsSet() ? FMath::Max(0.0, Now - Record.AwaySince.GetValue()) : 0.0);
}

EVeyraAutopilotStage StageOf(const FVeyraAbsenceRecord& Record, double Now, const FVeyraAutopilotTuning& Tuning)
{
	if (Record.Absence == EVeyraAbsence::Present || !Record.AwaySince.IsSet())
	{
		return EVeyraAutopilotStage::None;
	}
	return Now - Record.AwaySince.GetValue() < Tuning.FountainAfterSeconds ? EVeyraAutopilotStage::BehindTower : EVeyraAutopilotStage::Fountain;
}

FVector Destination(EVeyraAutopilotStage Stage, const FVector& From, TConstArrayView<FVector> Towers, const FVector& Home, double Distance)
{
	if (Stage != EVeyraAutopilotStage::BehindTower || Towers.IsEmpty())
	{
		return Home;
	}
	const FVector* Nearest = &Towers[0];
	for (const FVector& Tower : Towers)
	{
		Nearest = FVector::DistSquared2D(From, Tower) < FVector::DistSquared2D(From, *Nearest) ? &Tower : Nearest;
	}
	const FVector Toward = (Home - *Nearest).GetSafeNormal2D();
	return *Nearest + Toward * FMath::Min(Distance, FVector::Dist2D(Home, *Nearest));
}

bool IsForgiven(const FVeyraAbsenceRecord& Record, bool bTeamWon, double ActiveSeconds, bool bContributedSinceReturn, const FVeyraAbsenceTuning& Tuning)
{
	if (!Record.bPersonalLoss)
	{
		return true;
	}
	return bTeamWon && bContributedSinceReturn && Record.Absence == EVeyraAbsence::Present
		&& AbsentSeconds(Record, ActiveSeconds) <= Tuning.MaxForgivenAbsentFraction * ActiveSeconds;
}
}
