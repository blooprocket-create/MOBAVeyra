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
 * rebindable; these are the default profile). Stored in Config/DefaultInput.ini. The player's own keys
 * (ADR-024 §6) are a copy of these with their bindings put in place (VeyraSettings::ApplyBindings). Move or attack,
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
	 * consumable is used up, and an Active is cast at the cursor, as Quick Cast casts. By default
	 * they sit on 1 2 3 5 6 7, around the vision tool's (ADR-016 §6).
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

	/** Each casts the Flux Spell in its slot at the cursor, as Quick Cast casts (ADR-015 §1). */
	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey Spell1Key;

	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey Spell2Key;

	/** Uses the vision tool toward the cursor: a Persistent Ward is placed there (ADR-016 §6). */
	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey VisionToolKey;

	/** Channels the Vanguard home to its fountain (Economy & Progression Bible §10; ADR-012 §8). */
	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey RecallKey;

	/** The camera (Settings Bible §2; ADR-020 §1): cycle its mode, hold it on the Vanguard, pan and drag it. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera")
	FKey CameraModeKey;

	UPROPERTY(Config, EditAnywhere, Category = "Camera")
	FKey HoldToCenterKey;

	UPROPERTY(Config, EditAnywhere, Category = "Camera")
	FKey CameraUpKey;

	UPROPERTY(Config, EditAnywhere, Category = "Camera")
	FKey CameraDownKey;

	UPROPERTY(Config, EditAnywhere, Category = "Camera")
	FKey CameraLeftKey;

	UPROPERTY(Config, EditAnywhere, Category = "Camera")
	FKey CameraRightKey;

	UPROPERTY(Config, EditAnywhere, Category = "Camera")
	FKey CameraDragKey;

	/** Each press moves the camera nearer or farther, within the zoom's range (ADR-052 §3). */
	UPROPERTY(Config, EditAnywhere, Category = "Camera")
	FKey CameraZoomInKey;

	UPROPERTY(Config, EditAnywhere, Category = "Camera")
	FKey CameraZoomOutKey;

	/** Held on the minimap, moves the camera there (ADR-020 §2). */
	UPROPERTY(Config, EditAnywhere, Category = "Camera")
	FKey MinimapCameraKey;

	/** Held with a click, pings "look here" or "danger" for the player's side (ADR-020 §2). */
	UPROPERTY(Config, EditAnywhere, Category = "Pings")
	FKey PingKey;

	UPROPERTY(Config, EditAnywhere, Category = "Pings")
	FKey DangerPingKey;

	/** The click that pings while a ping key is held. */
	UPROPERTY(Config, EditAnywhere, Category = "Pings")
	FKey PingClickKey;

	/** Answer the open vote YES or NO (Match Flow Bible §7–§10; ADR-019 §7). */
	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey VoteYesKey;

	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey VoteNoKey;

	/** Shows the mastery emote above the player's Vanguard, with its Mastery Level (ADR-045 §9). */
	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey MasteryEmoteKey;

	/** Held with an ability slot's key, spends a skill point on that slot instead of casting. */
	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey RankUpModifierKey;

	/** One press attack-moves toward the cursor: Attack Move Click (Settings Bible §1.3; ADR-041 §4). Unbound by default. */
	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey AttackMoveClickKey;

	/** Held with an ability's key, shows its indicator without casting it (Settings Bible §1.7; ADR-041 §1). */
	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey ShowCastRangeKey;

	/** Held, shows how far the player's basic attacks reach now (ADR-052 §4). Unbound by default. */
	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey ShowAttackRangeKey;

	/** The click that casts an ability waiting for one, a Normal Cast's (Settings Bible §1.2, §1.8). */
	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey SelectKey;

	/** Held with an ability's key, names the player's own Vanguard when the ability may name an ally (Settings Bible §1.5). */
	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey SelfCastKey;

	/** Held, or pressed to switch, so attacks and casts name only Vanguards under the cursor (Settings Bible §1.4). */
	UPROPERTY(Config, EditAnywhere, Category = "Bindings")
	FKey TargetVanguardsOnlyKey;

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

	/** One per Flux Spell slot, in slot order. */
	UPROPERTY()
	TArray<TObjectPtr<UInputAction>> SpellSlots;

	UPROPERTY()
	TObjectPtr<UInputAction> VisionTool;

	UPROPERTY()
	TObjectPtr<UInputAction> Recall;

	UPROPERTY()
	TObjectPtr<UInputAction> VoteYes;

	UPROPERTY()
	TObjectPtr<UInputAction> VoteNo;

	UPROPERTY()
	TObjectPtr<UInputAction> MasteryEmote;

	/** The action that casts Slot. */
	UInputAction* GetAbilityAction(EVeyraAbilitySlot Slot) const;
};

namespace VeyraInput
{
	/** Builds the actions and their mapping context from Settings. Outer keeps them alive. */
	VEYRAMATCH_API FVeyraInputObjects Build(const UVeyraInputSettings& Settings, UObject& Outer);

	/**
	 * A new mapping context putting Actions on Settings' keys, as after the player rebinds one: the
	 * actions, and so what they are bound to, stay. An action without a key is left unmapped.
	 */
	VEYRAMATCH_API UInputMappingContext* MapKeys(const UVeyraInputSettings& Settings, const FVeyraInputObjects& Actions, UObject& Outer);
}
