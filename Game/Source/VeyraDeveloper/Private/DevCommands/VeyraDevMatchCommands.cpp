// Copyright © 2026 Wayfinder Studios. All rights reserved.

// The developer commands for the match as a whole: its pause and end, its waves, its structures,
// Team Flux and fog of war. Each asks the owning system, which applies its own rules.

#include "DevCommands/VeyraDevCommands.h"

#include "Engine/World.h"
#include "Join/VeyraMatchAssignment.h"
#include "NavigationSystem.h"
#include "VeyraBattlegroundSubsystem.h"
#include "VeyraGameMode.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"
#include "VeyraTeamFluxSubsystem.h"
#include "VeyraVisionSubsystem.h"

namespace VeyraDevMatchCommands
{
	const TCHAR* const Category = TEXT("Match");

	AVeyraGameMode* GameModeOf(const AVeyraPlayerController& Requester)
	{
		return Requester.GetWorld()->GetAuthGameMode<AVeyraGameMode>();
	}

	FString RunPause(AVeyraPlayerController& Requester, TConstArrayView<FString> /*Args*/)
	{
		AVeyraGameMode* GameMode = GameModeOf(Requester);
		return GameMode && GameMode->PauseMatch(Requester) ? TEXT("The match is paused; Veyra.Dev.Resume resumes it.")
														   : TEXT("Not paused: the match must be live, and not paused already.");
	}

	FString RunResume(AVeyraPlayerController& Requester, TConstArrayView<FString> /*Args*/)
	{
		AVeyraGameMode* GameMode = GameModeOf(Requester);
		return GameMode && GameMode->ResumeMatch() ? TEXT("The match resumes.") : TEXT("The match is not paused.");
	}

	FString RunEndMatch(AVeyraPlayerController& Requester, TConstArrayView<FString> /*Args*/)
	{
		AVeyraGameMode* GameMode = GameModeOf(Requester);
		if (!GameMode)
		{
			return TEXT("There is no match to end.");
		}
		GameMode->EndMatch(EVeyraMatchEndReason::DeveloperRequest);
		return TEXT("The match is over, with no winner.");
	}

	FString RunSiege(AVeyraPlayerController& Requester, TConstArrayView<FString> /*Args*/)
	{
		AVeyraGameMode* GameMode = GameModeOf(Requester);
		return GameMode && GameMode->HandleDeveloperSiege(Requester)
			? TEXT("An enemy structure fell.")
			: TEXT("Nothing fell: the match must be live and not paused, with an enemy structure standing.");
	}

	FString RunSpawnWave(AVeyraPlayerController& Requester, TConstArrayView<FString> /*Args*/)
	{
		const AVeyraGameMode* GameMode = GameModeOf(Requester);
		UVeyraBattlegroundSubsystem* Battleground = Requester.GetWorld()->GetSubsystem<UVeyraBattlegroundSubsystem>();
		if (!GameMode || !Battleground || !Battleground->GetLayout() || GameMode->CheckOrdersAllowed() != EVeyraOrderRejection::None)
		{
			return TEXT("No wave: the match must be live and not paused, on a map with lanes.");
		}
		// Another of the latest wave, which leaves the schedule as it was. Before the first, the first
		// comes early, and the schedule moves on by one.
		const int32 Index = FMath::Max(0, Battleground->GetWavesSpawned() - 1);
		Battleground->SpawnWave(Index);
		return FString::Printf(TEXT("Wave %d set out in every lane, for both teams."), Index + 1);
	}

	FString RunTeamFlux(AVeyraPlayerController& Requester, TConstArrayView<FString> Args)
	{
		const UEnum* Sources = StaticEnum<EVeyraFluxSource>();
		const int64 Source = Args.Num() == 1 ? Sources->GetValueByNameString(Args[0]) : INDEX_NONE;
		if (Source == INDEX_NONE)
		{
			TArray<FString> Names;
			for (int32 Index = 0; Index < Sources->NumEnums() - 1; ++Index)
			{
				Names.Add(Sources->GetNameStringByIndex(Index));
			}
			return VeyraDevCommands::UsageReply(TEXT("TeamFlux")) + TEXT(", where the source is one of ") + FString::Join(Names, TEXT(", ")) + TEXT(".");
		}
		const AVeyraPlayerState* Participant = VeyraDevCommands::ParticipantOf(Requester);
		UVeyraTeamFluxSubsystem* Flux = Requester.GetWorld()->GetSubsystem<UVeyraTeamFluxSubsystem>();
		if (!Participant || !Flux)
		{
			return TEXT("There is no Team Flux in this match.");
		}
		const EVeyraTeam Team = Participant->GetVeyraTeam();
		Flux->Grant(Team, static_cast<EVeyraFluxSource>(Source));
		return FString::Printf(TEXT("Your team took the Team Flux of a %s: %.0f permanent, %.0f active."), *Sources->GetNameStringByValue(Source),
			Flux->GetPermanent(Team), Flux->GetActive(Team));
	}

	FString RunRevealMap(AVeyraPlayerController& Requester, TConstArrayView<FString> Args)
	{
		const TOptional<double> Seconds = VeyraDevCommands::ParseNumber(Args, 0);
		if (!Seconds.IsSet() || Seconds.GetValue() <= 0.0 || Args.Num() != 1)
		{
			return VeyraDevCommands::UsageReply(TEXT("RevealMap"));
		}
		UWorld* World = Requester.GetWorld();
		UVeyraVisionSubsystem* Vision = World->GetSubsystem<UVeyraVisionSubsystem>();
		const AVeyraPlayerState* Participant = VeyraDevCommands::ParticipantOf(Requester);
		if (!Vision || !Vision->IsStarted() || !Participant)
		{
			return TEXT("Nothing is hidden in this match.");
		}
		// Vision's own reveal, never a stop: the fog gate hides every unit it has not opened, so vision
		// must keep running for anyone to receive anything. It covers everywhere a unit can walk.
		const UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		const FBox Map = Navigation ? Navigation->GetNavigableWorldBounds() : FBox(ForceInit);
		if (!Map.IsValid)
		{
			return TEXT("This map has no navigable ground to reveal.");
		}
		Vision->RevealArea(Participant->GetVeyraTeam(), Map.GetCenter(), Map.GetExtent().Size2D(), Seconds.GetValue());
		return FString::Printf(TEXT("Your team sees the whole map for %s seconds; Dense Fog and Camouflage still hide what they hide."),
			*FString::SanitizeFloat(Seconds.GetValue()));
	}
}

void VeyraDevCommands::AddMatchCommands(TArray<FVeyraDevCommand>& Out)
{
	using namespace VeyraDevMatchCommands;
	Out.Add(FVeyraDevCommand::Server(TEXT("Pause"), Category, TEXT(""), TEXT("Pauses the match at once, without a vote."), &RunPause));
	Out.Add(FVeyraDevCommand::Server(TEXT("Resume"), Category, TEXT(""), TEXT("Resumes the paused match."), &RunResume));
	Out.Add(FVeyraDevCommand::Server(TEXT("EndMatch"), Category, TEXT(""), TEXT("Ends the match now, with no winner."), &RunEndMatch));
	Out.Add(FVeyraDevCommand::Server(TEXT("Siege"), Category, TEXT(""),
		TEXT("Destroys the next enemy structure in siege order, the Prime Well last."), &RunSiege));
	Out.Add(FVeyraDevCommand::Server(TEXT("SpawnWave"), Category, TEXT(""),
		TEXT("Sends another Fluxborn wave down every lane, for both teams, now."), &RunSpawnWave));
	Out.Add(FVeyraDevCommand::Server(TEXT("TeamFlux"), Category, TEXT("<source>"),
		TEXT("Gives your team the Team Flux of a destroyed structure or secured Well, as tuned."), &RunTeamFlux));
	Out.Add(FVeyraDevCommand::Server(TEXT("RevealMap"), Category, TEXT("<seconds>"),
		TEXT("Your team sees the whole map for this long, as a reveal ability would."), &RunRevealMap));
}
