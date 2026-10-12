// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Showcase/VeyraShowcaseSubsystem.h"

#include "DevCommands/VeyraDevCommands.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "AbilitySystemComponent.h"
#include "Showcase/VeyraShowcaseRules.h"
#include "Showcase/VeyraShowcaseSettings.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraGameMode.h"
#include "VeyraGameState.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"
#include "VeyraTeamStart.h"

DEFINE_LOG_CATEGORY_STATIC(LogVeyraShowcaseStage, Log, All);

namespace
{
	const TCHAR* const ShowcaseOption = TEXT("VeyraShowcase");
	const TCHAR* const DummyOption = TEXT("VeyraShowcaseDummy=");
	const TCHAR* const DummyName = TEXT("Dummy");

	/** The start of Team's side on the stage; null if it has none. */
	const AVeyraTeamStart* StartOf(UWorld& World, EVeyraTeam Team)
	{
		for (TActorIterator<AVeyraTeamStart> Start(&World); Start; ++Start)
		{
			if (Start->GetVeyraTeam() == Team)
			{
				return *Start;
			}
		}
		return nullptr;
	}
}

bool UVeyraShowcaseSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !UE_BUILD_SHIPPING && IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UVeyraShowcaseSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.URL.HasOption(ShowcaseOption))
	{
		return;
	}
	const TArray<FString> Problems = GetDefault<UVeyraShowcaseSettings>()->Validate();
	for (const FString& Problem : Problems)
	{
		UE_LOG(LogVeyraShowcaseStage, Error, TEXT("VeyraShowcase: the showcase settings are off: %s"), *Problem);
	}
	const TCHAR* Named = InWorld.URL.GetOption(DummyOption, nullptr);
	const TOptional<FVeyraContentId> Vanguard = FVeyraContentId::FromText(Named ? FString(Named) : GetDefault<UVeyraShowcaseSettings>()->DefaultDummy);
	if (!Problems.IsEmpty() || !Vanguard.IsSet() || !UVeyraVanguardsTuningSubsystem::FindVanguard(Vanguard.GetValue()))
	{
		UE_LOG(LogVeyraShowcaseStage, Error, TEXT("VeyraShowcase: no stage: the dummy needs a Vanguard Vanguards.json knows, and the settings must be whole."));
		return;
	}
	bActive = true;
	DummyVanguard = Vanguard.GetValue();
	UE_LOG(LogVeyraShowcaseStage, Display, TEXT("VeyraShowcase: will seat a dummy playing %s when preparation begins."), *DummyVanguard.ToString());
}

void UVeyraShowcaseSubsystem::Tick(float /*DeltaTime*/)
{
	UWorld* World = GetWorld();
	const AVeyraGameState* GameState = World ? World->GetGameState<AVeyraGameState>() : nullptr;
	if (GameState && GameState->GetPhase() >= EVeyraMatchPhase::Preparation)
	{
		bSeated = true;
		Seat(*World);
	}
}

TStatId UVeyraShowcaseSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVeyraShowcaseSubsystem, STATGROUP_Tickables);
}

void UVeyraShowcaseSubsystem::Seat(UWorld& World)
{
	AVeyraGameMode* GameMode = World.GetAuthGameMode<AVeyraGameMode>();
	const AVeyraGameState* GameState = World.GetGameState<AVeyraGameState>();
	if (!GameMode || !GameState)
	{
		return;
	}
	// The player fills a side first; the dummy takes the other, so it is the player's enemy.
	EVeyraTeam Side = EVeyraTeam::B;
	for (const APlayerState* Member : GameState->PlayerArray)
	{
		const AVeyraPlayerState* Participant = Cast<AVeyraPlayerState>(Member);
		if (Participant && !Participant->IsABot() && Participant->GetVeyraTeam() == EVeyraTeam::B)
		{
			Side = EVeyraTeam::A;
		}
	}
	Dummy = GameMode->AddBotParticipant(DummyName, Side, DummyVanguard);
	UE_LOG(LogVeyraShowcaseStage, Display, TEXT("VeyraShowcase: %s the dummy."), Dummy.IsValid() ? TEXT("seated") : TEXT("could not seat"));
}

FString UVeyraShowcaseSubsystem::Reset(AVeyraPlayerController& Requester, int32 Token)
{
	UWorld* World = GetWorld();
	AVeyraGameMode* GameMode = World ? World->GetAuthGameMode<AVeyraGameMode>() : nullptr;
	AVeyraPlayerState* Caster = VeyraDevCommands::ParticipantOf(Requester);
	AVeyraPlayerState* Seated = Dummy.Get();
	if (!bActive || !GameMode || !Caster || !Seated)
	{
		return VeyraShowcase::ResetReply(Token, false, TEXT("This match is not the skill showcase's stage, or its dummy is not seated yet (ADR-074 §2)."));
	}
	const AVeyraTeamStart* StartA = StartOf(*World, EVeyraTeam::A);
	const AVeyraTeamStart* StartB = StartOf(*World, EVeyraTeam::B);
	if (!StartA || !StartB)
	{
		return VeyraShowcase::ResetReply(Token, false, TEXT("The stage has no start for each side."));
	}
	// Each body stands on the line between the starts, its settings' distance from the centre on its own side, facing
	// the other.
	const FVector Centre = (StartA->GetActorLocation() + StartB->GetActorLocation()) / 2.0;
	const double Stand = GetDefault<UVeyraShowcaseSettings>()->StandDistanceFromCenter;
	TArray<FString> Done;
	bool bSet = true;
	for (AVeyraPlayerState* Participant : { Caster, Seated })
	{
		if (!VeyraTargeting::IsAlive(Participant))
		{
			GameMode->HandleDeveloperRespawn(*Participant);
		}
		// A ride still under way, as the last clip's, ends, and a body held on to is let go, so it can stand at its mark.
		if (UAbilitySystemComponent* AbilitySystem = Participant->GetAbilitySystemComponent())
		{
			VeyraShowcase::ReleaseHolds(*AbilitySystem);
		}
		Done.Add(VeyraDevCommands::HealParticipant(*Participant));
		const AVeyraTeamStart* Own = Participant->GetVeyraTeam() == EVeyraTeam::A ? StartA : StartB;
		const FVector Outward = (Own->GetActorLocation() - Centre).GetSafeNormal2D();
		const FRotator Facing = (-Outward).Rotation();
		FString Moved;
		// A body that cannot be stood at its mark yet, as one held on to another, leaves the stage unset: the recorder asks
		// again.
		bSet &= VeyraDevCommands::TeleportParticipant(*Participant, Centre + Outward * Stand, &Facing, Moved);
		Done.Add(Moved);
	}
	Done.Add(VeyraDevCommands::ResetCooldownsOf(*Caster));
	return VeyraShowcase::ResetReply(Token, bSet, FString::Join(Done, TEXT(" ")));
}
