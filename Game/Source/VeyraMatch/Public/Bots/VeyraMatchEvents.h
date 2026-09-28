// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Subsystems/WorldSubsystem.h"
#include "Teams/VeyraTeam.h"
#include "VeyraMatchTypes.h"

#include "VeyraMatchEvents.generated.h"

class AVeyraPlayerState;

/** Where a bot sits and how it plays, as the match seated it (ADR-013 §2, §6). */
struct FVeyraBotSeat
{
	EVeyraTeam Side = EVeyraTeam::None;
	FVeyraContentId Vanguard;
	EVeyraBotDifficulty Difficulty = EVeyraBotDifficulty::Beginner;

	/** Its place among its side's bots, from 0, in the order the match seated them; its lane follows from it. */
	int32 Seat = 0;
};

/**
 * What the match announces to the layers above it (ADR-013 §2): events go up, so the match never
 * names what listens. Server only.
 */
UCLASS()
class VEYRAMATCH_API UVeyraMatchEvents : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBotAdded, AVeyraPlayerState&, const FVeyraBotSeat&);

	/** A bot joined the match with its Vanguard controller, before or as its Vanguard spawns. */
	FOnBotAdded OnBotAdded;
};
