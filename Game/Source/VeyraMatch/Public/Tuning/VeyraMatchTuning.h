// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "UObject/ObjectMacros.h"

#include "VeyraMatchTuning.generated.h"

// The Match domain's tuning, bound from Game/Tuning/Match.json (ADR-006 §6). The schema holds every
// range; a 0 here only means "not loaded".

USTRUCT()
struct FVeyraTeamsTuning
{
	GENERATED_BODY()

	UPROPERTY()
	int32 MaxTeamSize = 0;
};

/** The in-match stages before play (Match Flow Bible §1). */
USTRUCT()
struct FVeyraPhasesTuning
{
	GENERATED_BODY()

	UPROPERTY()
	double LoadingTimeoutSeconds = 0.0;

	UPROPERTY()
	double PreparationSeconds = 0.0;
};

/** How the server accepts a player's move orders (ADR-006 §7). */
USTRUCT()
struct FVeyraOrdersTuning
{
	GENERATED_BODY()

	UPROPERTY()
	double MaxPerSecond = 0.0;

	UPROPERTY()
	double DestinationProjectionExtent = 0.0;

	UPROPERTY()
	double ArrivalTolerance = 0.0;
};

/** Base stats for every Vanguard in developer matches, until Vanguard definitions arrive (M4). */
USTRUCT()
struct FVeyraDeveloperLoadoutTuning
{
	GENERATED_BODY()

	UPROPERTY()
	double MaxHealth = 0.0;

	UPROPERTY()
	double MoveSpeed = 0.0;

	UPROPERTY()
	double TurnRateDegreesPerSecond = 0.0;

	UPROPERTY()
	double CapsuleRadius = 0.0;

	UPROPERTY()
	double CapsuleHalfHeight = 0.0;
};

USTRUCT()
struct FVeyraMatchTuning
{
	GENERATED_BODY()

	/** The Match.json format this build reads (a schema version marker, not tuning). */
	static constexpr int32 SchemaVersion = 1;

	UPROPERTY()
	FVeyraTeamsTuning Teams;

	UPROPERTY()
	FVeyraPhasesTuning Phases;

	UPROPERTY()
	FVeyraOrdersTuning Orders;

	UPROPERTY()
	FVeyraDeveloperLoadoutTuning DeveloperLoadout;
};
