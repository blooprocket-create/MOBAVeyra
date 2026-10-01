// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "VeyraTakedownRefundSubsystem.generated.h"

struct FVeyraDeathEvent;

/**
 * Cooldowns a takedown refunds (ADR-031 §9), as Black Step's reset: when an enemy Vanguard dies, each
 * takedown participant, its credited killer and its assisters (Combat Bible §18), has the remaining
 * cooldown of every ability in its loadout whose cast names a takedown refund cut by that fraction,
 * stowed ones and follow-ups included. Server only.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraTakedownRefundSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void OnDeath(const FVeyraDeathEvent& Death);

	FDelegateHandle DeathHandle;
};
