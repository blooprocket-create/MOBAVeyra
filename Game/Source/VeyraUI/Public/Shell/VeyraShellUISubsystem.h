// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "VeyraShellUISubsystem.generated.h"

class APlayerController;
class UVeyraClientFlowSubsystem;
class UVeyraShellScreen;

/**
 * Puts the shell's screen on the viewport whenever the client-state coordinator has one to show
 * (ADR-010 §4), and takes it off for the match. A map load clears the viewport, so the screen is
 * made again in each front-end world. The shell takes UI input only; the match gets game input back.
 * It exists only in a game signed in by a launcher, whose GameInstance has the coordinator.
 */
UCLASS()
class VEYRAUI_API UVeyraShellUISubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** The screen on the viewport now, or null. */
	UVeyraShellScreen* GetScreen() const { return Screen; }

private:
	/** Shows or hides the screen for the coordinator's state. */
	void Update();
	void DropScreen();
	/** Lets go of the coordinator's client and takes the screen down, while the client is still valid. */
	void ReleaseClient();
	void OnPostLoadMap(UWorld* World);
	/** Gives the player's controller the input mode the screen needs, once per controller and mode. */
	bool Tick(float DeltaSeconds);

	TWeakObjectPtr<UVeyraClientFlowSubsystem> Flow;

	UPROPERTY(Transient)
	TObjectPtr<UVeyraShellScreen> Screen;

	TWeakObjectPtr<APlayerController> InputModeFor;
	bool bInputModeIsShell = false;
	FDelegateHandle ChangedHandle;
	FDelegateHandle EndingHandle;
	FDelegateHandle PostLoadMapHandle;
	FTSTicker::FDelegateHandle TickHandle;
};
