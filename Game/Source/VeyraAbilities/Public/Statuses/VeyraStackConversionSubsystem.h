// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "VeyraStackConversionSubsystem.generated.h"

struct FVeyraStatusApplied;

/**
 * Stacks that turn into another status (ADR-026 §2): when an application brings a status whose data
 * names `atMaxStacks` to its most stacks, the status goes and the named one lands from the same source,
 * as Korruk's Splinters become Fractured. It listens to Combat's status-applied event, so every site
 * that applies a status is covered. Server only.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraStackConversionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void OnStatusApplied(const FVeyraStatusApplied& Applied);

private:
	FDelegateHandle StatusAppliedHandle;
};
