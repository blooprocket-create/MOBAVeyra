// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Chat/VeyraChatTypes.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "VeyraMatchMenuSubsystem.generated.h"

class AVeyraPlayerController;
class UInputAction;
class UInputComponent;
class UInputMappingContext;
class UVeyraLoadingScreen;
class UVeyraMatchMenu;
class UVeyraSettingsScreen;
class UVeyraUIInputSettings;
struct FVeyraChatCommand;
struct FVeyraContentId;
struct FVeyraInterfacePreferences;
class UVeyraChatComposer;
class UVeyraScoreboard;
class UVeyraShopScreen;

/**
 * Opens and closes the in-match screens with their keys (UVeyraUIInputSettings) in any match a Veyra
 * player controller plays: the menu (ADR-010 §4) and the shop (ADR-012 §11). While either is open,
 * the player's input reaches both it and the game; when both close, only the game. The menu's key
 * closes open Settings, a scoreboard the player toggled open, or an open shop first. Settings (ADR-024
 * §4) open from the menu, in its place, over the match, which goes on. The
 * scoreboard shows while its key is held, or switches with each press, as the player chose (SET-56),
 * and the cursor stays in the window unless the player lets it go (SET-83). The scoreboard (ADR-017 §4) shows while its
 * key is held and takes no input. The chat key opens the chat composer (ADR-029 §5), on All with Shift
 * held, and the composer keeps the keyboard until it sends or closes.
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

	/**
	 * The match loading screen while it is up, or null once it closes. It takes the keyboard whenever none of this
	 * subsystem's screens has it, so its Previous and Next are keyboard-accessible (SET-117; ADR-053 §3).
	 */
	void SetLoadingScreen(UVeyraLoadingScreen* Screen);

	/** Opens the menu if it is closed, and closes it if it is open, as its key does. */
	void ToggleMenu();

	/** The open menu, or null. */
	UVeyraMatchMenu* GetMenu() const { return Menu; }

	/** Opens Settings in the menu's place, as its Settings button does; the menu's key closes them. */
	void OpenSettings();
	void CloseSettings();

	/** Open Settings, or null. */
	UVeyraSettingsScreen* GetSettings() const { return Settings; }

	bool IsShopOpen() const { return Shop != nullptr; }

	/** Opens the shop if it is closed, and closes it if it is open, as its key does. */
	void ToggleShop();

	/** The open shop, or null. */
	UVeyraShopScreen* GetShop() const { return Shop; }

	/** Opens the shop if it is shut, and puts the keyboard in its search, as Focus Shop Search does (SET-58; ADR-058 §1). */
	void FocusShopSearch();

	/** Shows the scoreboard, and hides it. */
	void ShowScoreboard();
	void HideScoreboard();

	/** The scoreboard's key pressed and let go: held shows it while down; toggled switches it with each press. */
	void PressScoreboardKey();
	void ReleaseScoreboardKey();

	/** The scoreboard while shown, or null. */
	UVeyraScoreboard* GetScoreboard() const { return Scoreboard; }

	bool IsChatOpen() const { return Chat != nullptr; }

	/** Opens the chat composer on Channel, as the chat key does; not over the menu or Settings. */
	void OpenChat(EVeyraChatChannel Channel);
	void CloseChat();

	/** The open chat composer, or null. */
	UVeyraChatComposer* GetChat() const { return Chat; }

	/** Carries out a line the player sent from the composer: a message, or a mute (ADR-029 §3). */
	void SubmitChat(EVeyraChatChannel Channel, const FString& Typed);

	/** "/p", "/r" and "/msg": Party Chat and direct messages, which the client flow sends to the backend (ADR-046 §6). */
	void SubmitOutsideChat(AVeyraPlayerController& Controller, const FVeyraChatCommand& Command);

private:
	/** Binds the menu key for each new match controller. */
	bool Tick(float DeltaSeconds);
	void OpenMenu();
	void CloseMenu();
	void OpenShop();
	void CloseShop();
	/** The chat key: Team Chat, or All with Shift held. */
	void PressChatKey();

	/** Gives the open screens the player's input beside the game, or the game alone when none is open. */
	void UpdateInputMode();

	/** The player's interface settings over the developer's. */
	FVeyraInterfacePreferences Preferences() const;

public:
	/** The keys of the menu, the shop and the scoreboard: the player's bindings over the developer's. */
	const UVeyraUIInputSettings& GetKeys() const;

private:
	/** Makes PlayerKeys the developer's keys with the player's bindings, and maps the screens' actions to them anew. */
	void RefreshKeys();
	void OnPlayerSettingChanged(const FVeyraContentId& Id);

	UPROPERTY(Transient)
	TObjectPtr<UVeyraUIInputSettings> PlayerKeys;

	FDelegateHandle SettingsHandle;

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
	TObjectPtr<UVeyraSettingsScreen> Settings;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ShopAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ShopSearchAction;

	UPROPERTY(Transient)
	TObjectPtr<UVeyraShopScreen> Shop;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ScoreboardAction;

	UPROPERTY(Transient)
	TObjectPtr<UVeyraScoreboard> Scoreboard;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ChatAction;

	UPROPERTY(Transient)
	TObjectPtr<UVeyraChatComposer> Chat;

	/** The world's loading screen, which that world owns. */
	TWeakObjectPtr<UVeyraLoadingScreen> LoadingScreen;

	FTSTicker::FDelegateHandle TickHandle;
	/** Whether the input settings are usable; the menu is off otherwise. */
	bool bReady = false;
};
