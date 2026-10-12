// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Subsystems/WorldSubsystem.h"

#include "VeyraShowcaseSubsystem.generated.h"

class AVeyraPlayerController;
class AVeyraPlayerState;

/**
 * The skill showcase's stage, on its server (ADR-074 §2). On a dedicated server whose map URL carries VeyraShowcase,
 * it seats a dummy when preparation begins: a bot Vanguard with no brain on the side the player is not on, which stands
 * where it starts and does nothing. VeyraShowcaseDummy=<id> names the Vanguard it plays; the showcase settings' default
 * plays otherwise. Veyra.Dev.ResetShowcase asks it to put the stage back as it began. Developer builds only.
 */
UCLASS()
class UVeyraShowcaseSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return bActive && !bSeated; }
	virtual TStatId GetStatId() const override;

	/** Whether this server's map is the showcase's stage. */
	bool IsActive() const { return bActive; }

	/**
	 * Puts the stage back as it began, for Requester's recording: each of Requester's Vanguard and the dummy respawns
	 * if it died, has its Health and resource filled and stands at its mark between the sides' starts, facing the
	 * other; Requester's cooldowns are cleared. Statuses run their course. Returns what was done, or why the stage
	 * cannot be reset.
	 */
	FString Reset(AVeyraPlayerController& Requester);

private:
	void Seat(UWorld& World);

	bool bActive = false;
	bool bSeated = false;
	FVeyraContentId DummyVanguard;
	TWeakObjectPtr<AVeyraPlayerState> Dummy;
};
