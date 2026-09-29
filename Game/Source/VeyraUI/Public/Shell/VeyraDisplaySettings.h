// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "GenericPlatform/GenericWindow.h"

#include "VeyraDisplaySettings.generated.h"

enum class EVeyraClientState : uint8;

/** The Display Mode setting's choices (Settings & Accessibility Bible 166). */
UENUM()
enum class EVeyraDisplayMode : uint8
{
	Windowed,
	BorderlessFullscreen,
	Fullscreen,
};

/**
 * How the game uses the screen. The pre-game client keeps its window. A match takes the screen in
 * MatchDisplayMode from the loading after champion select until the match ends, and the client's
 * window comes back for the results, as League's client and game do. Presentation, stored in
 * Config/DefaultGame.ini; the player's own choice joins the Settings menu (Settings Bible 166).
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Veyra Display"))
class VEYRAUI_API UVeyraDisplaySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** How a match takes the screen. */
	UPROPERTY(Config, EditAnywhere, Category = "Display")
	EVeyraDisplayMode MatchDisplayMode = EVeyraDisplayMode::BorderlessFullscreen;

	/**
	 * The mode this run's matches use: MatchDisplayMode, unless the command line says
	 * -VeyraMatchDisplay=Windowed|BorderlessFullscreen|Fullscreen, as scripts that run several
	 * clients on one screen do.
	 */
	EVeyraDisplayMode GetMatchDisplayMode() const;
};

/** When a match takes the screen, as plain rules. */
namespace VeyraMatchDisplay
{
	/** Whether the client is in a match, from the loading after champion select to the match's end. */
	VEYRAUI_API bool TakesTheScreen(EVeyraClientState State);

	/** The mode Text names, such as "BorderlessFullscreen"; unset for anything else. */
	VEYRAUI_API TOptional<EVeyraDisplayMode> ParseDisplayMode(const FString& Text);

	/** The engine's window mode for Mode. */
	VEYRAUI_API EWindowMode::Type ToWindowMode(EVeyraDisplayMode Mode);
}
