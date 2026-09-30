// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Settings/VeyraDisplayApplier.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Scalability.h"
#include "Shell/VeyraDisplaySettings.h"
#include "Shell/VeyraMatchDisplaySubsystem.h"
#include "VeyraSettingsStore.h"
#include "VeyraSettingsSubsystem.h"
#include "VeyraUILog.h"
#include "Widgets/SWindow.h"

namespace VeyraDisplayApplierEngine
{
	void SetConsoleValue(const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Variable->Set(Value, ECVF_SetByGameSetting);
		}
	}
}

bool UVeyraDisplayApplier::ShouldCreateSubsystem(UObject* Outer) const
{
	// The editor's window and frame rate are the editor's own.
	return !IsRunningDedicatedServer() && !GIsEditor && Super::ShouldCreateSubsystem(Outer);
}

void UVeyraDisplayApplier::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Settings = Collection.InitializeDependency<UVeyraSettingsSubsystem>();
	MatchDisplay = Collection.InitializeDependency<UVeyraMatchDisplaySubsystem>();
	if (!Settings.IsValid() || !Settings->IsReady())
	{
		UE_LOG(LogVeyraUI, Warning, TEXT("VeyraDisplay: no player settings, so the engine keeps its own display settings."));
		return;
	}
	ChangedHandle = Settings->GetStore().OnChanged.AddUObject(this, &UVeyraDisplayApplier::OnSettingChanged);
	if (FSlateApplication::IsInitialized())
	{
		bForeground = FSlateApplication::Get().IsActive();
		ActivationHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(this, &UVeyraDisplayApplier::OnActivationChanged);
	}
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVeyraDisplayApplier::Tick));
	Apply(/*bWindow*/ false);
}

void UVeyraDisplayApplier::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ActivationHandle);
	}
	if (UVeyraSettingsSubsystem* Found = Settings.Get(); Found && Found->IsReady())
	{
		Found->GetStore().OnChanged.Remove(ChangedHandle);
	}
	Super::Deinitialize();
}

UVeyraDisplayApplier* UVeyraDisplayApplier::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UVeyraDisplayApplier>() : nullptr;
}

bool UVeyraDisplayApplier::InLiveMatch() const
{
	const UVeyraMatchDisplaySubsystem* Display = MatchDisplay.Get();
	return Display && Display->HasTheScreen();
}

bool UVeyraDisplayApplier::NeedsConfirmation(const FVeyraContentId& Id) const
{
	// The client's window changes at once outside a match; a match's display mode, during one.
	if (Id == VeyraDisplayRules::WindowSize())
	{
		return !InLiveMatch();
	}
	return Id == VeyraDisplayRules::MatchMode() && InLiveMatch();
}

void UVeyraDisplayApplier::AwaitConfirmation(const FVeyraContentId& Id, const FString& Previous)
{
	Confirmation.Await(Id, Previous, FPlatformTime::Seconds() + GetDefault<UVeyraDisplaySettings>()->KeepChangesSeconds);
	OnConfirmationChanged.Broadcast();
}

double UVeyraDisplayApplier::GetSecondsToRevert() const
{
	return Confirmation.SecondsLeft(FPlatformTime::Seconds());
}

void UVeyraDisplayApplier::KeepChange()
{
	Confirmation.Keep();
	OnConfirmationChanged.Broadcast();
}

void UVeyraDisplayApplier::RevertChange()
{
	if (UVeyraSettingsSubsystem* Found = Settings.Get(); Found && Found->IsReady())
	{
		Confirmation.Revert(Found->GetStore(), InLiveMatch());
	}
	OnConfirmationChanged.Broadcast();
}

FIntPoint UVeyraDisplayApplier::GetClientWindowSize() const
{
	int32 Width = 0;
	int32 Height = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("ResX="), Width) && FParse::Value(FCommandLine::Get(), TEXT("ResY="), Height) && Width > 0 && Height > 0)
	{
		return FIntPoint(Width, Height);
	}
	const UVeyraSettingsSubsystem* Found = Settings.Get();
	const FIntPoint Chosen = Found && Found->IsReady() ? VeyraDisplayRules::Resolve(Found->GetStore(), bForeground).WindowSize : FIntPoint::ZeroValue;
	if (Chosen.X > 0 && Chosen.Y > 0)
	{
		return Chosen;
	}
	const UGameUserSettings* Engine = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	return Engine ? Engine->GetScreenResolution() : FIntPoint::ZeroValue;
}

bool UVeyraDisplayApplier::Tick(float /*DeltaSeconds*/)
{
	if (!bWindowSized && GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWindow())
	{
		// The window exists once the engine has drawn: now it takes the client's size.
		bWindowSized = true;
		Apply(/*bWindow*/ true);
	}
	UVeyraSettingsSubsystem* Found = Settings.Get();
	if (Found && Found->IsReady() && Confirmation.Tick(FPlatformTime::Seconds(), Found->GetStore(), InLiveMatch()))
	{
		UE_LOG(LogVeyraUI, Log, TEXT("VeyraDisplay: the display change was not kept, so it reverted."));
		OnConfirmationChanged.Broadcast();
	}
	return true;
}

void UVeyraDisplayApplier::OnActivationChanged(bool bActive)
{
	bForeground = bActive;
	Apply(/*bWindow*/ false);
}

void UVeyraDisplayApplier::OnSettingChanged(const FVeyraContentId& Id)
{
	UVeyraSettingsSubsystem* Found = Settings.Get();
	if (!Found || !Found->IsReady())
	{
		return;
	}
	if (!bFollowing)
	{
		TGuardValue<bool> Following(bFollowing, true);
		VeyraDisplayRules::FollowQualityPreset(Found->GetStore(), Id);
	}
	Apply(/*bWindow*/ Id == VeyraDisplayRules::WindowSize());
	if (Id == VeyraDisplayRules::MatchMode())
	{
		if (UVeyraMatchDisplaySubsystem* Display = MatchDisplay.Get())
		{
			Display->RetakeTheScreen();
		}
	}
}

void UVeyraDisplayApplier::Apply(bool bWindow)
{
	const UVeyraSettingsSubsystem* Found = Settings.Get();
	if (!Found || !Found->IsReady() || !GEngine)
	{
		return;
	}
	const FVeyraDisplayState State = VeyraDisplayRules::Resolve(Found->GetStore(), bForeground);
	GEngine->SetMaxFPS(State.FrameCap);
	VeyraDisplayApplierEngine::SetConsoleValue(TEXT("r.VSync"), State.bVSync ? 1.0f : 0.0f);
	VeyraDisplayApplierEngine::SetConsoleValue(TEXT("r.ScreenPercentage"), State.RenderScale);
	Scalability::FQualityLevels Levels = Scalability::GetQualityLevels();
	Levels.TextureQuality = State.TextureQuality;
	Levels.ShadowQuality = State.ShadowQuality;
	Levels.EffectsQuality = State.EffectsQuality;
	Scalability::SetQualityLevels(Levels);
	// A match holding the screen gives the window back at the client's size when it ends.
	UGameUserSettings* Engine = GEngine->GetGameUserSettings();
	if (bWindow && Engine && bWindowSized && !InLiveMatch())
	{
		const FIntPoint Size = GetClientWindowSize();
		Engine->SetFullscreenMode(EWindowMode::Windowed);
		Engine->SetScreenResolution(Size);
		Engine->ApplyResolutionSettings(/*bCheckForCommandLineOverrides*/ false);
		UE_LOG(LogVeyraUI, Log, TEXT("VeyraDisplay: the client's window is %dx%d."), Size.X, Size.Y);
	}
}
