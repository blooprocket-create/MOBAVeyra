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

	/** One bot of a side, as it was seated, and the seat whose place it holds. */
	struct FSeated
	{
		TWeakObjectPtr<AVeyraPlayerState> Bot;
		TWeakObjectPtr<UVeyraBotBrainComponent> Brain;
		FVeyraContentId Vanguard;
		EVeyraBotDifficulty Difficulty = EVeyraBotDifficulty::Beginner;
		int32 Seat = 0;
		int32 PlaceSeat = INDEX_NONE;
	};

	/** Gives Each the place of seat PlaceSeat: its role, its Flux Spells, whether it wards. */
	void Place(FSeated& Each, int32 PlaceSeat);

	/** Deals Side's places afresh among its bots not yet in play, moving each whose place changes. */
	void Deal(EVeyraTeam Side);
	TMap<EVeyraTeam, TArray<FSeated>> Seated;

	FDelegateHandle BotAddedHandle;
};
