// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "VeyraBotSubsystem.generated.h"

class AVeyraPlayerState;
struct FVeyraBotSeat;

/**
 * Gives each bot the match announces a brain (ADR-013 §2): it listens to UVeyraMatchEvents and puts a
 * UVeyraBotBrainComponent on the bot's Vanguard controller, with the lane its seat plays and its
 * difficulty. Server only.
 */
UCLASS()
class VEYRABOTS_API UVeyraBotSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void OnBotAdded(AVeyraPlayerState& Bot, const FVeyraBotSeat& Seat);

	FDelegateHandle BotAddedHandle;
};
