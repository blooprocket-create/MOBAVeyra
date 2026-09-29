// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Pings/VeyraPingTypes.h"
#include "Subsystems/WorldSubsystem.h"

#include "VeyraPingSubsystem.generated.h"

class AVeyraPlayerState;

/**
 * Server: team pings (ADR-020 §2). A player pings a point through its controller; this owner checks it
 * may, counts it against the spam limit and hands it to each of its side's players' controllers, so
 * the other side never receives it. What a ping looks like, and for how long, is the UI's.
 */
UCLASS()
class VEYRAMATCH_API UVeyraPingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Server: Sender pings Point for its side. Returns why not, or None. */
	EVeyraPingRefusal Ping(const AVeyraPlayerState& Sender, const FVector& Point, EVeyraPingKind Kind);

private:
	/** When each player, by PlayerId, sent its recent pings, in real seconds. */
	TMap<int32, TArray<double>> SentAt;
};
