// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Match/VeyraMatchMenuSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Match/VeyraMatchMenu.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "Shell/VeyraUIInputSettings.h"
#include "VeyraGameState.h"
#include "VeyraPlayerController.h"
#include "VeyraUILog.h"

bool UVeyraMatchMenuSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UVeyraMatchMenuSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TArray<FString> Problems = GetDefault<UVeyraUIInputSettings>()->Validate();
	Problems.Append(GetDefault<UVeyraShellStyleSettings>()->Validate());
	for (const FString& Problem : Problems)
	{
		UE_LOG(LogVeyraUI, Error, TEXT("The in-match menu is off: %s"), *Problem);
	}
	bReady = Problems.IsEmpty();
	if (bReady)
	{
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVeyraMatchMenuSubsystem::Tick));
	}
}

void UVeyraMatchMenuSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	if (Menu)
	{
		Menu->RemoveFromParent();
		Menu = nullptr;
	}
	Super::Deinitialize();
}

bool UVeyraMatchMenuSubsystem::Tick(float /*DeltaSeconds*/)
{
	AVeyraPlayerController* Controller = Cast<AVeyraPlayerController>(GetGameInstance()->GetFirstLocalPlayerController());
	if (!Controller || Controller == BoundController.Get() || !Controller->GetWorld() || !Controller->GetWorld()->GetGameState<AVeyraGameState>())
	{
		return true;
	}
	UEnhancedInputLocalPlayerSubsystem* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer());
	if (!Input)
	{
		return true;
	}
	// A new match: the old controller, its binding and any open menu went with the old world. The
	// action and its mapping are built at runtime, so no binary input asset exists.
	Menu = nullptr;
	MenuAction = NewObject<UInputAction>(this, NAME_None, RF_Transient);
	MenuAction->ValueType = EInputActionValueType::Boolean;
	MenuMapping = NewObject<UInputMappingContext>(this, NAME_None, RF_Transient);
	MenuMapping->MapKey(MenuAction, GetDefault<UVeyraUIInputSettings>()->MatchMenuKey);
	Input->AddMappingContext(MenuMapping, /*Priority*/ 1);
	UEnhancedInputComponent* Component = NewObject<UEnhancedInputComponent>(Controller, NAME_None, RF_Transient);
	Component->BindAction(MenuAction, ETriggerEvent::Started, this, &UVeyraMatchMenuSubsystem::ToggleMenu);
	Controller->PushInputComponent(Component);
	MenuInput = Component;
	BoundController = Controller;
	return true;
}

void UVeyraMatchMenuSubsystem::ToggleMenu()
{
	if (Menu)
	{
		CloseMenu();
	}
	else
	{
		OpenMenu();
	}
}

void UVeyraMatchMenuSubsystem::OpenMenu()
{
	AVeyraPlayerController* Controller = BoundController.Get();
	if (!Controller || !Controller->IsLocalController())
	{
		return;
	}
	Menu = CreateWidget<UVeyraMatchMenu>(Controller);
	if (!Menu)
	{
		return;
	}
	Menu->Show(*Controller, [this] { CloseMenu(); });
	Menu->AddToViewport();
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(Menu->TakeWidget());
	Mode.SetHideCursorDuringCapture(false);
	Controller->SetInputMode(Mode);
}

void UVeyraMatchMenuSubsystem::CloseMenu()
{
	if (Menu)
	{
		Menu->RemoveFromParent();
		Menu = nullptr;
	}
	if (AVeyraPlayerController* Controller = BoundController.Get())
	{
		FInputModeGameOnly Mode;
		Mode.SetConsumeCaptureMouseDown(false);
		Controller->SetInputMode(Mode);
	}
}
