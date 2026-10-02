// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Math/Color.h"

class UObject;
struct FVeyraInterfacePreferences;

/**
 * How the client's menus look to the player (Settings Bible Proposals 62, 65 and 75; ADR-055 §2–§3): their text size,
 * panel opacity and decorative motion. The shell style builds every menu with the current look.
 */
struct FVeyraShellLook
{
	/** Interface Text Size's factor on the menus' text; the HUD and combat numbers keep their own. */
	float TextScale = 1.0f;
	/** Panels drawn fully opaque (Reduce Interface Transparency). */
	bool bOpaquePanels = false;
	/** Decorative motion made still (Reduce Interface Animation). */
	bool bStillAnimation = false;

	bool operator==(const FVeyraShellLook& Other) const = default;
};

namespace VeyraShellLook
{
	/** The look Preferences give: Interface Text Size's factor from the shell style settings, and the reductions chosen. */
	VEYRAUI_API FVeyraShellLook For(const FVeyraInterfacePreferences& Preferences);

	/** The look the menus are built with now. */
	VEYRAUI_API const FVeyraShellLook& Current();

	/** Builds the menus with Look from now on. True if it changed. */
	VEYRAUI_API bool Use(const FVeyraShellLook& Look);

	/** Uses the look of WorldContext's player settings: every menu calls it before it builds. */
	VEYRAUI_API void FollowPlayer(const UObject* WorldContext);

	/** A menu font's Size in the current look: scaled by Interface Text Size, never under 1. */
	VEYRAUI_API int32 ScaledFontSize(int32 Size);

	/** A panel's fill as the look draws it: opaque when panels are. */
	VEYRAUI_API FLinearColor Panel(const FLinearColor& Fill);
}