// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Content/VeyraContentId.h"
#include "GameplayAbilitySpecHandle.h"
#include "VeyraAbilityTypes.h"

#include "VeyraAbilityLoadoutComponent.generated.h"

class UAbilitySystemComponent;

/** One slot of a loadout: the ability's content ID, and on the server its granted spec. */
USTRUCT()
struct FVeyraLoadoutEntry
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraAbilitySlot Slot = EVeyraAbilitySlot::Q;

	UPROPERTY()
	FVeyraContentId Ability;

	/** Server only. */
	UPROPERTY(NotReplicated)
	FGameplayAbilitySpecHandle Handle;
};

/**
 * Which ability sits in each of a combatant's slots. It lives beside the Ability System Component
 * (a Vanguard's PlayerState), grants each ability from its content ID and tells an ability which
 * content it is running. The owner receives the slots for its HUD.
 */
UCLASS(ClassGroup = Abilities)
class VEYRAABILITIES_API UVeyraAbilityLoadoutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVeyraAbilityLoadoutComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Server only: grants Ability in Slot, replacing whatever was there. Its archetype comes from the
	 * Abilities tuning. Returns false if no archetype defines Ability.
	 */
	bool Grant(UAbilitySystemComponent& AbilitySystem, EVeyraAbilitySlot Slot, const FVeyraContentId& Ability);

	/** Server only: empties Slot, taking back its ability, as when an item with an Active leaves its slot. */
	void Clear(UAbilitySystemComponent& AbilitySystem, EVeyraAbilitySlot Slot);

	const FVeyraLoadoutEntry* FindSlot(EVeyraAbilitySlot Slot) const;
	const FVeyraLoadoutEntry* FindAbility(const FVeyraContentId& Ability) const;
	const FVeyraLoadoutEntry* FindHandle(FGameplayAbilitySpecHandle Handle) const;

	/**
	 * Server only: how many Flux Spell slots, in slot order, its team's permanent Flux has unlocked
	 * (Battleground Bible §14; ADR-015 §4). Match sets it; a slot stays unlocked once it is.
	 */
	void SetUnlockedSpellSlots(int32 Count);

	int32 GetUnlockedSpellSlots() const { return UnlockedSpellSlots; }

	/** Whether Slot is a Flux Spell slot not unlocked yet, whatever it holds. */
	bool IsLocked(EVeyraAbilitySlot Slot) const;

private:
	UPROPERTY(Replicated)
	TArray<FVeyraLoadoutEntry> Entries;

	UPROPERTY(Replicated)
	int32 UnlockedSpellSlots = 0;
};
