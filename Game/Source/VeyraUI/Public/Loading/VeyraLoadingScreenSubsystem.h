// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Loading/VeyraLoadingModel.h"
#include "Subsystems/WorldSubsystem.h"

#include "VeyraLoadingScreenSubsystem.generated.h"

class UVeyraLoadingScreen;

/**
 * Shows a match's loading screen (ADR-053 §3): from the client's arrival in the match until the server leaves its Loading
 * phase, with the stage it is at and the player's tips and lore. Once loading is done it never shows again in this world.
 * Presentation only, in a match's world on a machine with a screen.
 */
UCLASS()
class VEYRAUI_API UVeyraLoadingScreenSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Loading screens go on through a pause. */
	virtual bool IsTickableWhenPaused() const override { return true; }

	/** The stage the match is at for this machine's player, or none once loading is done. For tests. */
	TOptional<EVeyraLoadingStage> GetStage() const { return Stage; }

	/** The screen, while it shows. For tests. */
	UVeyraLoadingScreen* GetScreen() const { return Screen; }

private:
	void Close();

	TOptional<EVeyraLoadingStage> Stage;

	/** Loading finished in this world. */
	bool bDone = false;

	UPROPERTY(Transient)
	TObjectPtr<UVeyraLoadingScreen> Screen;
};
