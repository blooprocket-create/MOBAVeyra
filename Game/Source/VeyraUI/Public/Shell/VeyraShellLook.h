// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Math/Color.h"
#include "Templates/UnrealTemplate.h"

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
	/** Keyboard focus drawn thick and high-contrast (Enhanced Keyboard Focus Indicator). */
	bool bEnhancedFocus = false;

	bool operator==(const FVeyraShellLook& Other) const = default;
};

/** How a focused text field is edged (ADR-055 §3). */
struct FVeyraFocusEdge
{
	FLinearColor Color = FLinearColor::White;
	float Width = 1.0f;
};

/**
 * Builds with a look while it lives, then with the one before: for surfaces the menus' look never reaches, as the
 * match's scoreboard, which keeps the HUD's own text and panels (ADR-055 §2).
 */
class VEYRAUI_API FVeyraScopedShellLook
{
public:
	explicit FVeyraScopedShellLook(const FVeyraShellLook& Look);
	~FVeyraScopedShellLook();

	UE_NONCOPYABLE(FVeyraScopedShellLook);

private:
	FVeyraShellLook Previous;
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

	/**
	 * The edge of a focused text field in the current look: under Enhanced focus, the thick high-contrast outline the
	 * focused button wears (SET-74); otherwise the accent's hairline.
	 */
	VEYRAUI_API FVeyraFocusEdge FieldFocusEdge();
}