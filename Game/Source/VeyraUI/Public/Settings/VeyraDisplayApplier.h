// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Ticker.h"
#include "Settings/VeyraDisplayRules.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "VeyraDisplayApplier.generated.h"

class UVeyraMatchDisplaySubsystem;
class UVeyraSettingsSubsystem;

/**
 * Applies the player's Graphics & Display settings to the engine as they change (ADR-024 §6; Settings
 * Bible §8): the frame cap in front of other windows and behind them (SET-109), VSync, render scale,
 * the quality preset and its groups, and the client's window size, which a launch's -ResX and -ResY
 * override for scripts that run several clients. A change that could leave the player unable to see,
 * the window's size or a live match's display mode, waits for Keep and reverts after the countdown
 * otherwise (SET-92). A standalone client's only: never on a server or in the editor, whose window
 * and frame rate are the editor's.
 */
UCLASS()
class VEYRAUI_API UVeyraDisplayApplier : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** The game instance's applier, or null where there is none. */
	static UVeyraDisplayApplier* Get(const UObject* WorldContext);

	/** Whether a change to Id now takes effect on the screen at once, so it needs the player to keep it. */
	bool NeedsConfirmation(const FVeyraContentId& Id) const;

	/** Id just changed in Settings from Previous: it waits for Keep, and reverts when the countdown ends. */
	void AwaitConfirmation(const FVeyraContentId& Id, const FString& Previous);

	bool IsAwaitingConfirmation() const { return Confirmation.IsPending(); }
	double GetSecondsToRevert() const;
	void KeepChange();
	void RevertChange();

	/** A change began or stopped waiting for Keep. */
	FSimpleMulticastDelegate OnConfirmationChanged;

	/** The client's windowed size: the launch's -ResX and -ResY, or the player's setting. */
	FIntPoint GetClientWindowSize() const;

private:
	bool Tick(float DeltaSeconds);
	void OnSettingChanged(const FVeyraContentId& Id);
	void OnActivationChanged(bool bActive);
	/** Makes the engine show Store's values; the window only when bWindow. */
	void Apply(bool bWindow);
	bool InLiveMatch() const;

	TWeakObjectPtr<UVeyraSettingsSubsystem> Settings;
	TWeakObjectPtr<UVeyraMatchDisplaySubsystem> MatchDisplay;
	FDelegateHandle ChangedHandle;
	FDelegateHandle ActivationHandle;
	FTSTicker::FDelegateHandle TickHandle;
	FVeyraDisplayConfirmation Confirmation;
	bool bForeground = true;
	/** The window gets its size once it exists. */
	bool bWindowSized = false;
	/** Set while the quality preset's groups follow a change, so their own changes do not start another. */
	bool bFollowing = false;
};
