// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraTeamFluxSubsystem.h"

#include "Engine/World.h"
#include "State/VeyraTeamFluxState.h"
#include "TimerManager.h"
#include "Tuning/VeyraFluxTuningSubsystem.h"
#include "VeyraFluxLog.h"

namespace
{
	const FVeyraFluxGrantTuning& GrantFor(EVeyraFluxSource Source, const FVeyraFluxGrantsTuning& Grants)
	{
		switch (Source)
		{
		case EVeyraFluxSource::LaneSpire:
			return Grants.LaneSpire;
		case EVeyraFluxSource::BaseTower:
			return Grants.BaseTower;
		case EVeyraFluxSource::Inhibitor:
			return Grants.Inhibitor;
		case EVeyraFluxSource::FluxWell:
			return Grants.FluxWell;
		}
		return Grants.LaneSpire;
	}

	/** Team Flux is decided where the match is: on a server or in standalone play, never on a client. */
	bool IsServerWorld(const UWorld* World)
	{
		return World && World->GetNetMode() != NM_Client;
	}
}

bool UVeyraTeamFluxSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Every world has one; only a world that begins play on its server spawns the published state.
	return Super::ShouldCreateSubsystem(Outer);
}

void UVeyraTeamFluxSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!IsServerWorld(&InWorld))
	{
		return;
	}
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	State = InWorld.SpawnActor<AVeyraTeamFluxState>(AVeyraTeamFluxState::StaticClass(), FTransform::Identity, Spawn);
	Publish();
}

void UVeyraTeamFluxSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExpiryTimer);
	}
	Super::Deinitialize();
}

void UVeyraTeamFluxSubsystem::Grant(EVeyraTeam Team, EVeyraFluxSource Source)
{
	FVeyraFluxLedger* Ledger = LedgerOf(Team);
	if (!Ledger || !IsServerWorld(GetWorld()))
	{
		return;
	}
	const FVeyraFluxGrantTuning& Grant = GrantFor(Source, UVeyraFluxTuningSubsystem::Get().Grants);
	Ledger->Add(Grant, Now());
	UE_LOG(LogVeyraFlux, Log, TEXT("Team %s gained %g %s Team Flux from a %s; %g active."), *UEnum::GetValueAsString(Team), Grant.Amount,
		*UEnum::GetValueAsString(Grant.Duration), *UEnum::GetValueAsString(Source), Ledger->Active(Now()));
	Publish();
	ScheduleExpiry();
	OnTeamFluxChanged.Broadcast(Team);
}

double UVeyraTeamFluxSubsystem::GetActive(EVeyraTeam Team) const
{
	const FVeyraFluxLedger* Ledger = LedgerOf(Team);
	return Ledger ? Ledger->Active(Now()) : 0.0;
}

double UVeyraTeamFluxSubsystem::GetPermanent(EVeyraTeam Team) const
{
	const FVeyraFluxLedger* Ledger = LedgerOf(Team);
	return Ledger ? Ledger->Permanent : 0.0;
}

FVeyraFluxbornStrength UVeyraTeamFluxSubsystem::GetFluxbornStrength(EVeyraTeam Team) const
{
	return VeyraFlux::StrengthFor(GetActive(Team), UVeyraFluxTuningSubsystem::Get().FluxbornScaling);
}

void UVeyraTeamFluxSubsystem::ExpireGrants()
{
	const double At = Now();
	TArray<EVeyraTeam, TInlineAllocator<2>> Changed;
	for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
	{
		if (LedgerOf(Team)->Expire(At))
		{
			Changed.Add(Team);
		}
	}
	if (Changed.IsEmpty())
	{
		ScheduleExpiry();
		return;
	}
	Publish();
	ScheduleExpiry();
	for (const EVeyraTeam Team : Changed)
	{
		UE_LOG(LogVeyraFlux, Log, TEXT("Temporary Team Flux expired for team %s; %g active."), *UEnum::GetValueAsString(Team), GetActive(Team));
		OnTeamFluxChanged.Broadcast(Team);
	}
}

FVeyraFluxLedger* UVeyraTeamFluxSubsystem::LedgerOf(EVeyraTeam Team)
{
	return Team == EVeyraTeam::A ? &LedgerA : Team == EVeyraTeam::B ? &LedgerB : nullptr;
}

const FVeyraFluxLedger* UVeyraTeamFluxSubsystem::LedgerOf(EVeyraTeam Team) const
{
	return Team == EVeyraTeam::A ? &LedgerA : Team == EVeyraTeam::B ? &LedgerB : nullptr;
}

double UVeyraTeamFluxSubsystem::Now() const
{
	// World time, which a pause holds (ADR-006 §8).
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

void UVeyraTeamFluxSubsystem::Publish()
{
	AVeyraTeamFluxState* Published = State.Get();
	if (!Published)
	{
		return;
	}
	TArray<FVeyraTeamFluxView> Teams;
	for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
	{
		const FVeyraFluxLedger& Ledger = *LedgerOf(Team);
		FVeyraTeamFluxView& View = Teams.AddDefaulted_GetRef();
		View.Team = Team;
		View.Permanent = Ledger.Permanent;
		for (const FVeyraTemporaryFlux& Grant : Ledger.Temporary)
		{
			View.Temporary.Add({ Grant.Amount, Grant.ExpiresAt });
		}
	}
	Published->SetTeams(MoveTemp(Teams));
}

void UVeyraTeamFluxSubsystem::ScheduleExpiry()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	TOptional<double> Next;
	for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
	{
		const TOptional<double> TeamNext = LedgerOf(Team)->NextExpiry();
		if (TeamNext.IsSet())
		{
			Next = Next.IsSet() ? FMath::Min(Next.GetValue(), TeamNext.GetValue()) : TeamNext.GetValue();
		}
	}
	FTimerManager& Timers = World->GetTimerManager();
	Timers.ClearTimer(ExpiryTimer);
	if (Next.IsSet())
	{
		// A timer needs a positive delay; a grant already due expires on the next frame.
		const float Delay = FMath::Max(static_cast<float>(Next.GetValue() - Now()), UE_KINDA_SMALL_NUMBER);
		Timers.SetTimer(ExpiryTimer, FTimerDelegate::CreateUObject(this, &UVeyraTeamFluxSubsystem::ExpireGrants), Delay, /*bLoop*/ false);
	}
}
