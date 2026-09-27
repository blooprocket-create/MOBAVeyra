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

/**
 * Opens and closes the in-match menu with its key (UVeyraUIInputSettings, ADR-010 §4) in any match
 * a Veyra player controller plays. While the menu is open, the player's input reaches both the menu
 * and the game; when it closes, only the game.
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

private:
	/** Binds the menu key for each new match controller. */
	bool Tick(float DeltaSeconds);
	void OpenMenu();
	void CloseMenu();

	TWeakObjectPtr<AVeyraPlayerController> BoundController;

	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> MenuInput;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MenuAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MenuMapping;

	UPROPERTY(Transient)
	TObjectPtr<UVeyraMatchMenu> Menu;

	FTSTicker::FDelegateHandle TickHandle;
	/** Whether the input settings are usable; the menu is off otherwise. */
	bool bReady = false;
};
