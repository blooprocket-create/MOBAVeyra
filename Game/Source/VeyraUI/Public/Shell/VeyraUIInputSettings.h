// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"

#include "VeyraUIInputSettings.generated.h"

/**
 * The default keys of Veyra's menus (Settings Bible §1.1: every action is rebindable; these are the
 * default profile). Stored in Config/DefaultInput.ini beside the Vanguard's controls.
 */
UCLASS(Config = Input, DefaultConfig, meta = (DisplayName = "Veyra Menu Input"))
class VEYRAUI_API UVeyraUIInputSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Every problem with these settings, as "Field: message"; empty when they are usable. */
	TArray<FString> Validate() const;

	/** Opens and closes the in-match menu (ADR-010 §4). */
	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey MatchMenuKey;

	/** Opens and closes the shop (ADR-012 §11), as P does in League. */
	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey ShopKey;
};
