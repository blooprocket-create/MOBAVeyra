// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Loading/VeyraLoadingScreenSubsystem.h"

#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "HAL/PlatformTime.h"
#include "Engine/GameInstance.h"
#include "Loading/VeyraLoadingScreen.h"
#include "Match/VeyraMatchMenuSubsystem.h"
#include "Shell/VeyraShellLook.h"
#include "Settings/VeyraInterfacePreferences.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "Text/VeyraContentText.h"
#include "VeyraGameState.h"
#include "VeyraPlayerController.h"
#include "VeyraSettingsSubsystem.h"

bool UVeyraLoadingScreenSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return Super::ShouldCreateSubsystem(Outer) && World && World->IsGameWorld() && !IsRunningDedicatedServer();
}

void UVeyraLoadingScreenSubsystem::Deinitialize()
{
	Close();
	Super::Deinitialize();
}

TStatId UVeyraLoadingScreenSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVeyraLoadingScreenSubsystem, STATGROUP_Tickables);
}

void UVeyraLoadingScreenSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (bDone || !World)
	{
		return;
	}
	// Only a match's world: a client of a match server, or a match played in place. The front end has neither.
	const AVeyraGameState* GameState = World->GetGameState<AVeyraGameState>();
	if (World->GetNetMode() != NM_Client && !GameState)
	{
		return;
	}
	AVeyraPlayerController* Local = Cast<AVeyraPlayerController>(World->GetFirstPlayerController());
	Stage = VeyraLoadingModel::StageOf(GameState ? TOptional<EVeyraMatchPhase>(GameState->GetPhase()) : TOptional<EVeyraMatchPhase>(), Local && Local->GetVanguard());
	if (!Stage.IsSet())
	{
		// Loading is done: the screen closes at once, never held for a tip (SET-120).
		bDone = true;
		Close();
		return;
	}
	const double Now = FPlatformTime::Seconds();
	if (!Screen && Local && Local->IsLocalController() && World->GetGameViewport())
	{
		// In the player's look: text size, transparency and motion (SET-112; ADR-055 §2–§3).
		VeyraShellLook::FollowPlayer(Local);
		Screen = CreateWidget<UVeyraLoadingScreen>(Local);
		if (Screen)
		{
			const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
			FVeyraLoadingTiming Timing;
			Timing.MinimumSeconds = Style.LoadingEntryMinimumSeconds;
			Timing.BaseCharacters = Style.LoadingEntryBaseCharacters;
			Timing.CharactersPerSecond = Style.LoadingEntryCharactersPerSecond;
			// A different order each match, so the screen never opens on the same entry (SET-119).
			const int32 Seed = static_cast<int32>(FPlatformTime::Cycles());
			// The player's categories, followed while the screen is up: Settings stay reachable from the menu (SET-118).
			UVeyraSettingsSubsystem* Settings = UVeyraSettingsSubsystem::Get(World);
			Screen->ShowFor(Settings ? &Settings->GetStore() : nullptr, Seed, Timing, Now);
			// Under the in-match menu, so Leave Match and Settings stay reachable while the match loads.
			Screen->AddToViewport(/*ZOrder*/ 0);
			// It takes the keyboard for Previous and Next whenever no other screen has it (SET-117).
			if (UVeyraMatchMenuSubsystem* Menus = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UVeyraMatchMenuSubsystem>() : nullptr)
			{
				Menus->SetLoadingScreen(Screen);
			}
		}
	}
	if (Screen)
	{
		Screen->SetStage(Stage.GetValue());
		Screen->Update(Now);
	}
}

void UVeyraLoadingScreenSubsystem::Close()
{
	if (Screen)
	{
		const UWorld* World = GetWorld();
		if (UVeyraMatchMenuSubsystem* Menus = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UVeyraMatchMenuSubsystem>() : nullptr)
		{
			Menus->SetLoadingScreen(nullptr);
		}
		Screen->UnbindSettings();
		Screen->RemoveFromParent();
		Screen = nullptr;
	}
}
