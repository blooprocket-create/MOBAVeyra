// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Math/IntPoint.h"
#include "Misc/Optional.h"
#include "Shell/VeyraDisplaySettings.h"

class FVeyraSettingsStore;

/** What the engine shows, as the player's Graphics & Display settings ask (ADR-024 §6; Settings Bible §8). */
struct FVeyraDisplayState
{
	/** The most frames per second the engine draws; 0 for no cap. */
	float FrameCap = 0.0f;
	bool bVSync = false;
	/** The scene's share of the window's resolution, in percent; the interface stays sharp. */
	float RenderScale = 100.0f;
	/** Scalability levels, from 0 (Low) up. */
	int32 TextureQuality = 0;
	int32 ShadowQuality = 0;
	int32 EffectsQuality = 0;
	/** The client's windowed size. */
	FIntPoint WindowSize = FIntPoint::ZeroValue;
	/** How a match takes the screen. */
	EVeyraDisplayMode MatchMode = EVeyraDisplayMode::BorderlessFullscreen;
};

/** The Graphics & Display settings' rules, apart from the engine, so tests can read them. */
namespace VeyraDisplayRules
{
	/** The settings the rules read, as the registry names them. */
	VEYRAUI_API const FVeyraContentId& FrameCap();
	VEYRAUI_API const FVeyraContentId& BackgroundFrameCap();
	VEYRAUI_API const FVeyraContentId& VSync();
	VEYRAUI_API const FVeyraContentId& RenderScale();
	VEYRAUI_API const FVeyraContentId& Quality();
	VEYRAUI_API const FVeyraContentId& TextureQuality();
	VEYRAUI_API const FVeyraContentId& ShadowQuality();
	VEYRAUI_API const FVeyraContentId& EffectsQuality();
	VEYRAUI_API const FVeyraContentId& WindowSize();
	VEYRAUI_API const FVeyraContentId& MatchMode();

	/**
	 * The engine's state for Store's values: the foreground or the background frame cap, as the
	 * game is in front of other windows or not (SET-109).
	 */
	VEYRAUI_API FVeyraDisplayState Resolve(const FVeyraSettingsStore& Store, bool bForeground);

	/** A size such as "1280x720"; unset for anything else. */
	VEYRAUI_API TOptional<FIntPoint> ParseSize(const FString& Text);

	/**
	 * Keeps the quality preset and its groups agreeing after Changed changed (§8): a preset sets every
	 * group to it, and a group changed on its own makes the preset the level all groups share, or
	 * Custom. The groups follow without taking the Undo step, which stays with the player's change.
	 */
	VEYRAUI_API void FollowQualityPreset(FVeyraSettingsStore& Store, const FVeyraContentId& Changed);
}

/**
 * A disruptive display change waiting for the player to keep it (SET-92): the window's size, or a
 * match's display mode, changed in Settings. Without Keep by the deadline, it reverts.
 */
class VEYRAUI_API FVeyraDisplayConfirmation
{
public:
	/** Id changed from Previous; it reverts at Deadline unless kept. A later change replaces an earlier one's wait. */
	void Await(const FVeyraContentId& Id, const FString& Previous, double Deadline);

	bool IsPending() const { return Pending.IsSet(); }
	double SecondsLeft(double Now) const;

	/** Reverts in Store once Now reaches the deadline; true if it did. */
	bool Tick(double Now, FVeyraSettingsStore& Store, bool bInLiveMatch);

	void Keep();
	void Revert(FVeyraSettingsStore& Store, bool bInLiveMatch);

private:
	struct FPending
	{
		FVeyraContentId Id;
		FString Previous;
		double Deadline = 0.0;
	};
	TOptional<FPending> Pending;
};
