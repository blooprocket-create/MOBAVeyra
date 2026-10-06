// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Content/VeyraContentId.h"
#include "GameplayAbilitySpecHandle.h"
#include "VeyraAbilityTypes.h"

#include "VeyraAbilityLoadoutComponent.generated.h"

class UAbilitySystemComponent;

/** How long a slot override lasts (ADR-018 §1). */
UENUM()
enum class EVeyraOverrideUse : uint8
{
	/** Used up by its own cast, with every override of its group: a follow-up, or a Redlined form. */
	Once,
	/** Lasts until its time or its state ends, however often it is cast: a variant, or a ride's set. */
	WhileActive,
};

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

/** What a slot holds instead of its own ability for a while (ADR-018 §1). */
USTRUCT()
struct FVeyraSlotOverride
{
	GENERATED_BODY()

	UPROPERTY()
	FVeyraLoadoutEntry Entry;

	/** When it ends, in the server's world time; 0 for when its state ends. */
	UPROPERTY()
	double EndsAt = 0.0;

	UPROPERTY()
	EVeyraOverrideUse Use = EVeyraOverrideUse::Once;

	/** Overrides that end together; None for one alone. */
	UPROPERTY(NotReplicated)
	FName Group;

	/**
	 * The slot's own ability it belongs to, as a follow-up belongs to the ability whose cast opened it:
	 * while the slot holds another own ability, as in another stance, it waits unseen (ADR-031 §3). None
	 * for one that holds the slot whatever its own ability, as a variant or a ride's set does.
	 */
	UPROPERTY()
	FVeyraContentId Over;

	/** Server only: whether it casts itself as its time runs out. */
	UPROPERTY(NotReplicated)
	bool bCastOnExpiry = false;

	/** Whether it cools down as its slot's own ability: one cooldown for both. */
	UPROPERTY()
	bool bSharesCooldown = false;
};

/** What a slot holds instead of its own ability, and for how long (ADR-018 §1). */
struct FVeyraOverrideSpec
{
	FVeyraContentId Ability;

	/** Seconds it lasts; 0 until it is ended. */
	double DurationSeconds = 0.0;

	EVeyraOverrideUse Use = EVeyraOverrideUse::Once;

	/** Overrides that end together; None for one alone. */
	FName Group;

	/** Whether it casts itself as its time runs out, where the caster faces. */
	bool bCastOnExpiry = false;

	/** Whether it cools down as its slot's own ability, one cooldown for both; else it keeps its own. */
	bool bSharesCooldown = false;

	/** The slot's own ability it belongs to, or None for any (FVeyraSlotOverride::Over). */
	FVeyraContentId Over;
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

	/**
	 * Server only: Slot's own ability becomes Ability, the present one stowed with its grant and its
	 * cooldown (ADR-031 §3). An ability stowed there before comes back as it was; a cast of the stowed one
	 * still running finishes. Returns false if Slot holds nothing or no archetype defines Ability.
	 */
	bool SwapOwn(UAbilitySystemComponent& AbilitySystem, EVeyraAbilitySlot Slot, const FVeyraContentId& Ability);

	/** Server only: what Slot stowed, if anything (ADR-031 §3). */
	const FVeyraLoadoutEntry* FindStowed(EVeyraAbilitySlot Slot) const;

	/**
	 * The stance ability whose set its slots hold now, or None in its own set (ADR-031 §3). Unlike its slots, every
	 * machine receives it, so its body can show the stance it is in (ADR-064 §1). The stance ability sets it.
	 */
	const FVeyraContentId& GetStance() const { return Stance; }

	/** Server only: records the stance its slots hold, None for its own set. */
	void SetStance(const FVeyraContentId& InStance);

	/** Server: visits every ability it holds, its own, its stowed ones and its overrides', once each. */
	void ForEachAbility(TFunctionRef<void(const FVeyraLoadoutEntry&)> Visit) const;

	/** What Slot holds now: its override while one lasts and belongs to its own ability, else its own ability. */
	const FVeyraLoadoutEntry* FindSlot(EVeyraAbilitySlot Slot) const;

	/** Slot's own ability, whatever override holds it now. */
	const FVeyraLoadoutEntry* FindOwnSlot(EVeyraAbilitySlot Slot) const;

	/** Slot's override now, if one holds it. */
	const FVeyraSlotOverride* FindOverride(EVeyraAbilitySlot Slot) const;

	/**
	 * Ability's entry, its own, a stowed one's on the server, or an override's: an override and a stowed
	 * ability share their slot's rank (ADR-018 §1; ADR-031 §3).
	 */
	const FVeyraLoadoutEntry* FindAbility(const FVeyraContentId& Ability) const;
	const FVeyraLoadoutEntry* FindHandle(FGameplayAbilitySpecHandle Handle) const;

	/**
	 * Server: Slot holds Spec's ability instead of its own, replacing any override already there
	 * (ADR-018 §1). Returns false if no archetype defines it.
	 */
	bool Override(UAbilitySystemComponent& AbilitySystem, EVeyraAbilitySlot Slot, const FVeyraOverrideSpec& Spec);

	/** Server: Slot holds its own ability again. */
	void EndOverride(UAbilitySystemComponent& AbilitySystem, EVeyraAbilitySlot Slot);

	/** Server: every override of Group ends. */
	void EndGroup(UAbilitySystemComponent& AbilitySystem, FName Group);

	/** Server: Ability committed; a used-once override of it ends, with its group. */
	void NoteCommitted(UAbilitySystemComponent& AbilitySystem, const FVeyraContentId& Ability);

	/** Whether Slot shows an override now. */
	bool IsOverridden(EVeyraAbilitySlot Slot) const;

	/**
	 * The ID Ability cools down under: for an override that shares it, the own ability it belongs to, or
	 * its slot's own ability; else its own.
	 */
	FVeyraContentId CooldownIdOf(const FVeyraContentId& Ability) const;

	const TArray<FVeyraSlotOverride>& GetOverrides() const { return Overrides; }

	/**
	 * Server only: how many Flux Spell slots, in slot order, its team's permanent Flux has unlocked
	 * (Battleground Bible §14; ADR-015 §4). Match sets it; a slot stays unlocked once it is.
	 */
	void SetUnlockedSpellSlots(int32 Count);

	int32 GetUnlockedSpellSlots() const { return UnlockedSpellSlots; }

	/** Whether Slot is a Flux Spell slot not unlocked yet, whatever it holds. */
	bool IsLocked(EVeyraAbilitySlot Slot) const;

private:
	/** Server: Slot's override ran out: it casts itself first if it should, then ends. */
	void OnOverrideExpired(EVeyraAbilitySlot Slot);

	/** Removes the override at Index, its ability leaving once it ends. */
	void RemoveOverrideAt(UAbilitySystemComponent& AbilitySystem, int32 Index);

	/** Whether Override shows in its slot now: it belongs to any own ability, or to the one there (ADR-031 §3). */
	bool IsShown(const FVeyraSlotOverride& Override) const;

	UPROPERTY(Replicated)
	TArray<FVeyraLoadoutEntry> Entries;

	UPROPERTY(Replicated)
	TArray<FVeyraSlotOverride> Overrides;

	/** Server: overrides that ended while their ability still ran, so its cast can still name it. */
	TArray<FVeyraLoadoutEntry> Retired;

	/** Server: own abilities a stance put away, each with its grant, to come back to its slot (ADR-031 §3). */
	TArray<FVeyraLoadoutEntry> Stowed;

	/** Server: each timed override's timer, by slot. */
	TMap<EVeyraAbilitySlot, FTimerHandle> OverrideTimers;

	UPROPERTY(Replicated)
	int32 UnlockedSpellSlots = 0;

	/** Every machine: the stance whose set its slots hold (GetStance). */
	UPROPERTY(Replicated)
	FVeyraContentId Stance;
};
