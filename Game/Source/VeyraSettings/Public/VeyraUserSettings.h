// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameFramework/GameUserSettings.h"
#include "VeyraUserSettings.generated.h"

/**
 * The device-local store (ADR-024 §1): the engine's user settings, saved in this machine's
 * GameUserSettings.ini, plus the device-scope player settings the registry lists. DefaultEngine.ini
 * names it the engine's GameUserSettingsClassName. It never leaves the machine.
 */
UCLASS()
class VEYRASETTINGS_API UVeyraUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	/** The engine's user settings as Veyra's, or null if another class holds them (a tool, a test without the config). */
	static UVeyraUserSettings* Get();

	/** The device-scope settings the player changed, as text by setting ID (FVeyraSettingsStore::SaveScope). */
	UPROPERTY(config)
	TMap<FString, FString> DeviceSettings;
};
