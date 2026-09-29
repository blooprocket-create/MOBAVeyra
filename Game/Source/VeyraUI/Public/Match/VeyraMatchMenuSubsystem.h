// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "VeyraMatchMenuSubsystem.generated.h"

class AVeyraPlayerController;
class UInputAction;
class UInputComponent;
class UInputMappingContext;
class UVeyraMatchMenu;
class UVeyraShopScreen;

/**
 * Opens and closes the in-match screens with their keys (UVeyraUIInputSettings) in any match a Veyra
 * player controller plays: the menu (ADR-010 §4) and the shop (ADR-012 §11). While either is open,
 * the player's input reaches both it and the game; when both close, only the game. The menu's key
 * closes an open shop first, as Escape does in League.
 */
UCLASS()
class VEYRAUI_API UVeyraMatchMenuSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	bool IsMenuOpen() const { return Menu != nullptr; }

	/** Opens the menu if it is closed, and closes it if it is open, as its key does. */
	void ToggleMenu();

	/** The open menu, or null. */
	UVeyraMatchMenu* GetMenu() const { return Menu; }

	bool IsShopOpen() const { return Shop != nullptr; }

	/** Opens the shop if it is closed, and closes it if it is open, as its key does. */
	void ToggleShop();

	/** The open shop, or null. */
	UVeyraShopScreen* GetShop() const { return Shop; }

private:
	/** Binds the menu key for each new match controller. */
	bool Tick(float DeltaSeconds);
	void OpenMenu();
	void CloseMenu();
	void OpenShop();
	void CloseShop();

	/** Gives the open screens the player's input beside the game, or the game alone when none is open. */
	void UpdateInputMode();

	TWeakObjectPtr<AVeyraPlayerController> BoundController;

	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> MenuInput;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MenuAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MenuMapping;

	UPROPERTY(Transient)
	TObjectPtr<UVeyraMatchMenu> Menu;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ShopAction;

	UPROPERTY(Transient)
	TObjectPtr<UVeyraShopScreen> Shop;

	FTSTicker::FDelegateHandle TickHandle;
	/** Whether the input settings are usable; the menu is off otherwise. */
	bool bReady = false;
};
