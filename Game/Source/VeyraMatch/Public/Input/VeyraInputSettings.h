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
 * rebindable; these are the default profile). Stored in Config/DefaultInput.ini. Move or attack,
 * attack-move, and the four ability slots with Quick Cast (§1.2); casting modes and profiles follow.
 */
UCLASS(Config = Input, DefaultConfig, meta = (DisplayName = "Veyra Input"))
class VEYRAMATCH_API UVeyraInputSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Orders the Vanguard to the ground under the cursor, or to attack the enemy under it. */
	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey MoveOrderKey;

	/** Attack-moves the Vanguard to the ground under the cursor, attacking enemies it meets on the way. */
	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey AttackMoveKey;

	/** Each slot casts at once at the cursor: the unit under it, and the ground under it (Quick Cast). */
	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey AbilityQKey;

	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey AbilityWKey;

	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey AbilityEKey;

	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey AbilityRKey;

	/**
	 * Each uses the item in its inventory slot, 1 to 6 (author ruling 2026-09-28; ADR-012 §1): a
	 * consumable is used up, and an Active is cast at the cursor, as Quick Cast casts.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey Item1Key;

	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey Item2Key;

	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey Item3Key;

	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey Item4Key;

	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey Item5Key;

	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey Item6Key;

	/** Channels the Vanguard home to its fountain (Economy & Progression Bible §10; ADR-012 §8). */
	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey RecallKey;

	/** Held with an ability slot's key, spends a skill point on that slot instead of casting. */
	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey RankUpModifierKey;

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
	TObjectPtr<UInputAction> AttackMove;

	UPROPERTY()
	TObjectPtr<UInputAction> AbilityQ;

	UPROPERTY()
	TObjectPtr<UInputAction> AbilityW;

	UPROPERTY()
	TObjectPtr<UInputAction> AbilityE;

	UPROPERTY()
	TObjectPtr<UInputAction> AbilityR;

	/** One per item slot, in inventory order. */
	UPROPERTY()
	TArray<TObjectPtr<UInputAction>> ItemSlots;

	UPROPERTY()
	TObjectPtr<UInputAction> Recall;

	/** The action that casts Slot. */
	UInputAction* GetAbilityAction(EVeyraAbilitySlot Slot) const;
};

namespace VeyraInput
{
	/** Builds the actions and their mapping context from Settings. Outer keeps them alive. */
	VEYRAMATCH_API FVeyraInputObjects Build(const UVeyraInputSettings& Settings, UObject& Outer);
}
