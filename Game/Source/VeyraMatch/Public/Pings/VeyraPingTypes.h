// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Math/Vector.h"

#include "VeyraPingTypes.generated.h"

/**
 * What a team ping says (ADR-020 §2). Provisional: League's two basic pings stand in until the ping
 * system is designed (Chat & Communication Bible, open items).
 */
UENUM()
enum class EVeyraPingKind : uint8
{
	/** "Look here." */
	Look,
	/** "Danger: fall back." */
	Danger,
};

/** Why a ping was not sent. */
UENUM()
enum class EVeyraPingRefusal : uint8
{
	None,
	/** Only a seated player pings, and only to its own side. */
	NotAPlayer,
	/** Pings need the match: from preparation until it ends. */
	NotNow,
	/** Too many in a short time (pings.maxPerWindow in any pings.windowSeconds). */
	TooMany,
};

/** A ping as its side's players receive it. */
USTRUCT()
struct FVeyraPing
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Point = FVector::ZeroVector;

	UPROPERTY()
	EVeyraPingKind Kind = EVeyraPingKind::Look;

	/** The PlayerId of the teammate who sent it. */
	UPROPERTY()
	int32 SenderId = INDEX_NONE;
};

/** A ping a client holds, and when it arrived, in real seconds. */
struct FVeyraReceivedPing
{
	FVeyraPing Ping;
	double ReceivedAt = 0.0;
};
