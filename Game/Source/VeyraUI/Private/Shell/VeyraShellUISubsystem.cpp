// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraShellUISubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Client/VeyraClientFlowSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Shell/VeyraShellModels.h"
#include "Shell/VeyraShellScreen.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "UObject/UObjectGlobals.h"
#include "VeyraUILog.h"

bool UVeyraShellUISubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UVeyraShellUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Flow = Collection.InitializeDependency<UVeyraClientFlowSubsystem>();
	if (!Flow.IsValid())
	{
		// A game not started by a launcher has no coordinator, and so no shell.
		return;
	}
	if (const TArray<FString> Problems = GetDefault<UVeyraShellStyleSettings>()->Validate(); !Problems.IsEmpty())
	{
		for (const FString& Problem : Problems)
		{
			UE_LOG(LogVeyraUI, Error, TEXT("The shell shows no screen: %s"), *Problem);
		}
		Flow.Reset();
		return;
	}
	ChangedHandle = Flow->GetClient().OnChanged().AddUObject(this, &UVeyraShellUISubsystem::Update);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UVeyraShellUISubsystem::OnPostLoadMap);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVeyraShellUISubsystem::Tick));
	Update();
}

void UVeyraShellUISubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	if (UVeyraClientFlowSubsystem* Coordinator = Flow.Get())
	{
		Coordinator->GetClient().OnChanged().Remove(ChangedHandle);
	}
	DropScreen();
	Flow.Reset();
	Super::Deinitialize();
}

void UVeyraShellUISubsystem::Update()
{
	UVeyraClientFlowSubsystem* Coordinator = Flow.Get();
	if (!Coordinator)
	{
		return;
	}
	if (VeyraShellModels::ScreenFor(Coordinator->GetClient().GetSnapshot().State) == EVeyraShellScreen::None)
	{
		DropScreen();
		return;
	}
	UGameInstance* GameInstance = GetGameInstance();
	// A world with no viewport, such as a commandlet's, shows nothing.
	if (Screen || !GameInstance->GetWorld() || !GameInstance->GetGameViewportClient())
	{
		return;
	}
	Screen = CreateWidget<UVeyraShellScreen>(GameInstance);
	if (Screen)
	{
		Screen->Bind(Coordinator->GetClient());
		Screen->AddToViewport();
	}
}

void UVeyraShellUISubsystem::DropScreen()
{
	if (Screen)
	{
		Screen->Unbind();
		Screen->RemoveFromParent();
		Screen = nullptr;
	}
}

void UVeyraShellUISubsystem::OnPostLoadMap(UWorld* World)
{
	if (World && World->GetGameInstance() == GetGameInstance())
	{
		// Loading a map cleared the viewport: the screen is made again for the new world.
		DropScreen();
		Update();
	}
}

bool UVeyraShellUISubsystem::Tick(float /*DeltaSeconds*/)
{
	APlayerController* Controller = GetGameInstance()->GetFirstLocalPlayerController();
	const bool bShell = Screen != nullptr;
	if (!Controller || (Controller == InputModeFor.Get() && bShell == bInputModeIsShell))
	{
		return true;
	}
	if (bShell)
	{
		// Menus take the mouse and keys; the viewport does not capture the cursor.
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(Screen->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Controller->SetInputMode(Mode);
		Controller->SetShowMouseCursor(true);
	}
	else
	{
		// The match's own input, as the project's viewport settings set it up.
		FInputModeGameOnly Mode;
		Mode.SetConsumeCaptureMouseDown(false);
		Controller->SetInputMode(Mode);
	}
	InputModeFor = Controller;
	bInputModeIsShell = bShell;
	return true;
}
