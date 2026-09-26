// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "VeyraCombatEventSubsystem.generated.h"

class UAbilitySystemComponent;

/** A finalized death (Combat Bible §18). */
struct FVeyraDeathEvent
{
	TWeakObjectPtr<UAbilitySystemComponent> Victim;

	/** Whoever dealt the lethal damage. Null for damage with no source. Kill credit (§18) reads it later. */
	TWeakObjectPtr<UAbilitySystemComponent> Killer;
};

/**
 * Combat's outcomes, announced to the systems above it as events rather than through hard
 * references (ARCHITECTURE.md §1.7). Match listens for deaths to schedule respawns; Economy will
 * listen for kill rewards. Server only: Combat decides outcomes only on the server.
 */
UCLASS()
class VEYRACOMBAT_API UVeyraCombatEventSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnDeath, const FVeyraDeathEvent&);

	FOnDeath OnDeath;
};
