// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "VeyraCastSubsystem.generated.h"

/** Issues each cast in a world its unique server ID (Combat Bible §45), so later hits can be traced to it. */
UCLASS()
class VEYRAABILITIES_API UVeyraCastSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** A Cast ID no other cast in this world has had; never 0. */
	int32 IssueCastId() { return ++LastCastId; }

private:
	int32 LastCastId = 0;
};
