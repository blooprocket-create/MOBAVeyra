// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraMatchDisplaySubsystem.h"

#include "Client/VeyraClientFlowSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameUserSettings.h"
#include "Settings/VeyraDisplayApplier.h"
#include "Shell/VeyraDisplaySettings.h"
#include "VeyraSettingsSubsystem.h"
#include "VeyraUILog.h"
#include "Widgets/SWindow.h"

namespace
{
	/** The game's window, or null without one (a commandlet, a test, a server). */
	TSharedPtr<SWindow> GameWindow()
	{
		return GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : nullptr;
	}

	/**
	 * Puts the window in Mode at Size. The launch's own -windowed is left out of it: that was for the
	 * client's window, and the match's mode is the player's choice (UVeyraDisplaySettings).
	 */
	void Apply(UGameUserSettings& Settings, EWindowMode::Type Mode, const FIntPoint& Size)
	{
		Settings.SetFullscreenMode(Mode);
		Settings.SetScreenResolution(Size);
		Settings.ApplyResolutionSettings(/*bCheckForCommandLineOverrides*/ false);
	}
}

bool UVeyraMatchDisplaySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UVeyraMatchDisplaySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Flow = Collection.InitializeDependency<UVeyraClientFlowSubsystem>();
	if (!Flow.IsValid())
	{
		// A game not started by a launcher has no coordinator: its window is its own business.
		return;
	}
	ChangedHandle = Flow->GetClient().OnChanged().AddUObject(this, &UVeyraMatchDisplaySubsystem::Update);
	EndingHandle = Flow->OnClientEnding().AddUObject(this, &UVeyraMatchDisplaySubsystem::ReleaseClient);
	Update();
}

void UVeyraMatchDisplaySubsystem::Deinitialize()
{
	ReleaseClient();
	Super::Deinitialize();
}

void UVeyraMatchDisplaySubsystem::ReleaseClient()
{
	if (UVeyraClientFlowSubsystem* Coordinator = Flow.Get())
	{
		Coordinator->OnClientEnding().Remove(EndingHandle);
		Coordinator->GetClient().OnChanged().Remove(ChangedHandle);
	}
	Flow.Reset();
}

void UVeyraMatchDisplaySubsystem::Update()
{
	const UVeyraClientFlowSubsystem* Coordinator = Flow.Get();
	if (!Coordinator)
	{
		return;
	}
	const bool bMatch = VeyraMatchDisplay::TakesTheScreen(Coordinator->GetClient().GetSnapshot().State);
	if (bMatch && !Saved.IsSet())
	{
		TakeTheScreen();
	}
	else if (!bMatch && Saved.IsSet())
	{
		GiveTheScreenBack();
	}
}

void UVeyraMatchDisplaySubsystem::TakeTheScreen()
{
	const TSharedPtr<SWindow> Window = GameWindow();
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Window || !Settings)
	{
		return;
	}
	const FVector2D Size = Window->GetClientSizeInScreen();
	Saved = FSavedWindow{ Window->GetWindowMode(), FIntPoint(FMath::RoundToInt(Size.X), FMath::RoundToInt(Size.Y)), Window->GetPositionInScreen() };
	const UVeyraSettingsSubsystem* Player = GetGameInstance()->GetSubsystem<UVeyraSettingsSubsystem>();
	const EVeyraDisplayMode Mode = GetDefault<UVeyraDisplaySettings>()->GetMatchDisplayMode(Player && Player->IsReady() ? &Player->GetStore() : nullptr);
	if (Mode == EVeyraDisplayMode::Windowed)
	{
		UE_LOG(LogVeyraUI, Log, TEXT("The match keeps the client's window (%dx%d)."), Saved->Size.X, Saved->Size.Y);
		return;
	}
	// A fullscreen match fills the monitor its window is on, which need not be the primary one.
	FDisplayMetrics Metrics;
	FDisplayMetrics::RebuildDisplayMetrics(Metrics);
	const FVector2D Centre = Saved->Position + FVector2D(Saved->Size) / 2.0;
	const FIntPoint Desktop = VeyraMatchDisplay::MonitorSizeAt(Metrics.MonitorInfo, Centre, Settings->GetDesktopResolution());
	Saved->bChanged = true;
	Apply(*Settings, VeyraMatchDisplay::ToWindowMode(Mode), Desktop);
	UE_LOG(LogVeyraUI, Log, TEXT("The match takes the screen: %s at %dx%d."), *UEnum::GetValueAsString(Mode), Desktop.X, Desktop.Y);
}

void UVeyraMatchDisplaySubsystem::RetakeTheScreen()
{
	if (!Saved.IsSet())
	{
		// Not in a match: the next one takes the screen in the new mode.
		return;
	}
	GiveTheScreenBack();
	TakeTheScreen();
}

void UVeyraMatchDisplaySubsystem::GiveTheScreenBack()
{
	const FSavedWindow Window = Saved.GetValue();
	Saved.Reset();
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Settings || !Window.bChanged)
	{
		return;
	}
	// The client's size, which the player may have changed during the match.
	const UVeyraDisplayApplier* Applier = GetGameInstance()->GetSubsystem<UVeyraDisplayApplier>();
	const FIntPoint Size = Applier && Window.Mode == EWindowMode::Windowed ? Applier->GetClientWindowSize() : Window.Size;
	Apply(*Settings, Window.Mode, Size.X > 0 && Size.Y > 0 ? Size : Window.Size);
	if (const TSharedPtr<SWindow> GameWindowNow = GameWindow(); GameWindowNow && Window.Mode == EWindowMode::Windowed)
	{
		GameWindowNow->MoveWindowTo(Window.Position);
	}
	UE_LOG(LogVeyraUI, Log, TEXT("The client's window is back (%dx%d)."), Window.Size.X, Window.Size.Y);
}
