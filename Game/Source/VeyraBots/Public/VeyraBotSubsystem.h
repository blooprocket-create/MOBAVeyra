// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Subsystems/WorldSubsystem.h"
#include "Teams/VeyraTeam.h"
#include "VeyraMatchTypes.h"

#include "VeyraBotSubsystem.generated.h"

class AVeyraPlayerState;
class UVeyraBotBrainComponent;
struct FVeyraBotSeat;

/**
 * Gives each bot the match announces a brain (ADR-013 §2): it listens to UVeyraMatchEvents and puts a
 * UVeyraBotBrainComponent on the bot's Vanguard controller, with its difficulty. A team's seats set the places it
 * fills (a role, its Flux Spells, whether it wards); as each bot is seated, the team deals those places afresh among
 * its bots not yet in play, by the roles their Vanguards play (ADR-038 §5). Server only.
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

	/** Deals Side's places among its bots not yet in play, and gives each its place. */
	void Deal(EVeyraTeam Side);

	/** One bot of a side, as it was seated. */
	struct FSeated
	{
		TWeakObjectPtr<AVeyraPlayerState> Bot;
		TWeakObjectPtr<UVeyraBotBrainComponent> Brain;
		FVeyraContentId Vanguard;
		EVeyraBotDifficulty Difficulty = EVeyraBotDifficulty::Beginner;
		int32 Seat = 0;
	};
	TMap<EVeyraTeam, TArray<FSeated>> Seated;

	FDelegateHandle BotAddedHandle;
};
