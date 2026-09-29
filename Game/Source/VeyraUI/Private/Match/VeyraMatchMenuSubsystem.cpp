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
#include "Shop/VeyraShopScreen.h"
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
	if (Shop)
	{
		Shop->RemoveFromParent();
		Shop = nullptr;
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
	// A new match: the old controller, its binding and any open screen went with the old world. The
	// actions and their mapping are built at runtime, so no binary input asset exists.
	Menu = nullptr;
	Shop = nullptr;
	const UVeyraUIInputSettings& Keys = *GetDefault<UVeyraUIInputSettings>();
	MenuAction = NewObject<UInputAction>(this, NAME_None, RF_Transient);
	MenuAction->ValueType = EInputActionValueType::Boolean;
	ShopAction = NewObject<UInputAction>(this, NAME_None, RF_Transient);
	ShopAction->ValueType = EInputActionValueType::Boolean;
	MenuMapping = NewObject<UInputMappingContext>(this, NAME_None, RF_Transient);
	MenuMapping->MapKey(MenuAction, Keys.MatchMenuKey);
	MenuMapping->MapKey(ShopAction, Keys.ShopKey);
	Input->AddMappingContext(MenuMapping, /*Priority*/ 1);
	UEnhancedInputComponent* Component = NewObject<UEnhancedInputComponent>(Controller, NAME_None, RF_Transient);
	Component->BindAction(MenuAction, ETriggerEvent::Started, this, &UVeyraMatchMenuSubsystem::ToggleMenu);
	Component->BindAction(ShopAction, ETriggerEvent::Started, this, &UVeyraMatchMenuSubsystem::ToggleShop);
	Controller->PushInputComponent(Component);
	MenuInput = Component;
	BoundController = Controller;
	return true;
}

void UVeyraMatchMenuSubsystem::ToggleMenu()
{
	if (Shop && !Menu)
	{
		// The menu's key closes the shop first.
		CloseShop();
	}
	else if (Menu)
	{
		CloseMenu();
	}
	else
	{
		OpenMenu();
	}
}

void UVeyraMatchMenuSubsystem::ToggleShop()
{
	if (Shop)
	{
		CloseShop();
	}
	else
	{
		OpenShop();
	}
}

void UVeyraMatchMenuSubsystem::OpenShop()
{
	AVeyraPlayerController* Controller = BoundController.Get();
	if (!Controller || !Controller->IsLocalController())
	{
		return;
	}
	Shop = CreateWidget<UVeyraShopScreen>(Controller);
	if (!Shop)
	{
		return;
	}
	Shop->Show(*Controller, [this] { CloseShop(); });
	// Centred over the match, which stays in view and in play around it. The viewport keeps these
	// only once the shop is in it, and a size resets the anchors, so they go in this order.
	const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
	const FVector2D Centre(0.5, 0.5);
	Shop->AddToViewport();
	Shop->SetDesiredSizeInViewport(FVector2D(Style.ShopWidth, Style.ShopHeight));
	Shop->SetAnchorsInViewport(FAnchors(Centre.X, Centre.Y));
	Shop->SetAlignmentInViewport(Centre);
	UpdateInputMode();
}

void UVeyraMatchMenuSubsystem::CloseShop()
{
	if (Shop)
	{
		Shop->RemoveFromParent();
		Shop = nullptr;
	}
	UpdateInputMode();
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
	// Above the shop, if it is open.
	Menu->AddToViewport(/*ZOrder*/ 1);
	UpdateInputMode();
}

void UVeyraMatchMenuSubsystem::CloseMenu()
{
	if (Menu)
	{
		Menu->RemoveFromParent();
		Menu = nullptr;
	}
	UpdateInputMode();
}

void UVeyraMatchMenuSubsystem::UpdateInputMode()
{
	AVeyraPlayerController* Controller = BoundController.Get();
	if (!Controller)
	{
		return;
	}
	if (Menu || Shop)
	{
		// The menu takes the keyboard; the shop leaves it to the game, so abilities and items still work.
		FInputModeGameAndUI Mode;
		if (Menu)
		{
			Mode.SetWidgetToFocus(Menu->TakeWidget());
		}
		Mode.SetHideCursorDuringCapture(false);
		Controller->SetInputMode(Mode);
	}
	else
	{
		FInputModeGameOnly Mode;
		Mode.SetConsumeCaptureMouseDown(false);
		Controller->SetInputMode(Mode);
	}
}
