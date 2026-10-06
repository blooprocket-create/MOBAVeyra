// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Feedback/VeyraKillFeedLink.h"

#include "Engine/World.h"
#include "Feedback/VeyraKillFeedTypes.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "VeyraPlayerController.h"

FVeyraKillFeedLink::~FVeyraKillFeedLink()
{
	Stop();
}

void FVeyraKillFeedLink::Start(UWorld& World)
{
	if (UVeyraCombatEventSubsystem* Subsystem = World.GetSubsystem<UVeyraCombatEventSubsystem>())
	{
		Events = Subsystem;
		DeathHandle = Subsystem->OnDeath.AddRaw(this, &FVeyraKillFeedLink::OnDeath);
	}
}

void FVeyraKillFeedLink::Stop()
{
	if (UVeyraCombatEventSubsystem* Subsystem = Events.Get())
	{
		Subsystem->OnDeath.Remove(DeathHandle);
	}
	Events.Reset();
}

void FVeyraKillFeedLink::OnDeath(const FVeyraDeathEvent& Death)
{
	TOptional<FVeyraKillFeedLine> Line = VeyraKillFeedRules::LineFor(Death);
	const UVeyraCombatEventSubsystem* Subsystem = Events.Get();
	UWorld* World = Subsystem ? Subsystem->GetWorld() : nullptr;
	if (!Line.IsSet() || !World)
	{
		return;
	}
	if (Line->Kind == EVeyraKillFeedKind::Takedown && !bFirstBloodTaken)
	{
		bFirstBloodTaken = true;
		Line->bFirstBlood = true;
	}
	// Every player learns of it, whatever its side sees: a fall is no secret (ADR-065 §10).
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (AVeyraPlayerController* Controller = Cast<AVeyraPlayerController>(It->Get()))
		{
			Controller->ClientKillFeed(Line.GetValue());
		}
	}
}
