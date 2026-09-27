// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"
#include "Slots/VeyraAbilitySlot.h"

#include "VeyraInputSettings.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * The default bindings for a Vanguard's controls (Settings Bible §1.1: every gameplay action is
 * rebindable; these are the default profile). Stored in Config/DefaultInput.ini. Move, and the four
 * ability slots with Quick Cast (§1.2); casting modes and profiles follow.
 */
UCLASS(Config = Input, DefaultConfig, meta = (DisplayName = "Veyra Input"))
class VEYRAMATCH_API UVeyraInputSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Orders the Vanguard to the ground under the cursor. */
	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey MoveOrderKey;

	/** Each slot casts at once at the cursor: the unit under it, and the ground under it (Quick Cast). */
	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey AbilityQKey;

	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey AbilityWKey;

	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey AbilityEKey;

	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey AbilityRKey;

	/** The key bound to Slot. */
	const FKey& GetAbilityKey(EVeyraAbilitySlot Slot) const;

	/**
	 * How often a held move order repeats toward the cursor, in seconds. Keep it slower than the
	 * server's order limit (Match.json orders.maxPerSecond), or repeats are refused.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Pacing", meta = (ClampMin = "0.01"))
	float HeldMoveOrderIntervalSeconds = 0.0f;
};

/** Enhanced Input objects built at runtime from UVeyraInputSettings, so no binary input asset exists. */
USTRUCT()
struct VEYRAMATCH_API FVeyraInputObjects
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY()
	TObjectPtr<UInputAction> MoveOrder;

	UPROPERTY()
	TObjectPtr<UInputAction> AbilityQ;

	UPROPERTY()
	TObjectPtr<UInputAction> AbilityW;

	UPROPERTY()
	TObjectPtr<UInputAction> AbilityE;

	UPROPERTY()
	TObjectPtr<UInputAction> AbilityR;

	/** The action that casts Slot. */
	UInputAction* GetAbilityAction(EVeyraAbilitySlot Slot) const;
};

namespace VeyraInput
{
	/** Builds the actions and their mapping context from Settings. Outer keeps them alive. */
	VEYRAMATCH_API FVeyraInputObjects Build(const UVeyraInputSettings& Settings, UObject& Outer);
}
