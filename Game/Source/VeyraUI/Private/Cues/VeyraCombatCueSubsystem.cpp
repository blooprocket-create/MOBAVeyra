// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Cues/VeyraCombatCueSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Teams/VeyraTeam.h"

bool UVeyraCombatCueSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Cues are for worlds someone watches: game and play-in-editor worlds, never a dedicated server's.
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && World->GetNetMode() != NM_DedicatedServer && Super::ShouldCreateSubsystem(Outer);
}

void UVeyraCombatCueSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Refresh();
}

TStatId UVeyraCombatCueSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVeyraCombatCueSubsystem, STATGROUP_Tickables);
}

void UVeyraCombatCueSubsystem::Refresh()
{
	const APlayerController* Viewer = GetWorld()->GetFirstPlayerController();
	const EVeyraTeam ViewerTeam = Viewer ? VeyraTeams::TeamOf(Viewer->PlayerState) : EVeyraTeam::None;
	TSet<TWeakObjectPtr<const AActor>> Seen;
	TArray<FVeyraCombatCue> Cues;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		const APawn& Unit = **It;
		// A unit hidden from this client is not seen; it is sighted afresh when it shows again.
		const TOptional<FVeyraUnitSighting> Now = Unit.IsHidden() ? TOptional<FVeyraUnitSighting>() : VeyraCombatCues::Sight(Unit, ViewerTeam);
		if (!Now)
		{
			continue;
		}
		Seen.Add(&Unit);
		if (const FVeyraUnitSighting* Before = Sightings.Find(&Unit))
		{
			Cues.Append(VeyraCombatCues::Between(Unit, *Before, Now.GetValue()));
		}
		Sightings.Add(&Unit, Now.GetValue());
	}
	for (auto It = Sightings.CreateIterator(); It; ++It)
	{
		if (!Seen.Contains(It->Key))
		{
			It.RemoveCurrent();
		}
	}
	// Raised after every unit is sighted, so a listener sees the world as it is this frame.
	for (const FVeyraCombatCue& Cue : Cues)
	{
		OnCue.Broadcast(Cue);
	}
}
