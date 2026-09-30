// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Tuning/VeyraTuningProvenance.h"
#include "UObject/ObjectMacros.h"

#include "VeyraItemsTuning.generated.h"

/** What kind of item something is, for the rules that name a kind (Economy & Progression Bible §10; ADR-012 §9). */
UENUM()
enum class EVeyraItemCategory : uint8
{
	/** Equipment that holds a slot for good. */
	Equipment,
	/** Boots: a Vanguard holds at most the shop's limit of them. */
	Boots,
	/** Used up; it may stack in one slot. */
	Consumable,
};

/** What an item adds to its holder while delivered (Item Bible §3). */
USTRUCT()
struct FVeyraItemStatsTuning
{
	GENERATED_BODY()

	UPROPERTY()
	double Health = 0.0;

	UPROPERTY()
	double HealthRegeneration = 0.0;

	UPROPERTY()
	double PhysicalPower = 0.0;

	UPROPERTY()
	double MagicPower = 0.0;

	/** Bonus Attack Speed, as a fraction of the Vanguard's base (ADR-012 §6). */
	UPROPERTY()
	double AttackSpeed = 0.0;

	UPROPERTY()
	double AbilityHaste = 0.0;

	UPROPERTY()
	double MoveSpeed = 0.0;

	UPROPERTY()
	double MagicPenetrationFlat = 0.0;

	/** Crit Chance, as a fraction; items' add (Combat Bible §5; ADR-022 §2). */
	UPROPERTY()
	double CritChance = 0.0;

	/** A percentage of total Magic Power, as a fraction, multiplying with every other (Combat Bible §41; ADR-022 §2). */
	UPROPERTY()
	double MagicPowerFraction = 0.0;
};

/** One item the shop sells (Item Bible §2, §4–§10). */
USTRUCT()
struct FVeyraItemDefinition
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** 1 components, 2 assemblies, 3 Masterworks. */
	UPROPERTY()
	int32 Tier = 0;

	UPROPERTY()
	EVeyraItemCategory Category = EVeyraItemCategory::Equipment;

	/** The items its recipe consumes; empty for a component. */
	UPROPERTY()
	TArray<FVeyraContentId> Components;

	/** A component's price, or the recipe's completion cost beyond its components. */
	UPROPERTY()
	double Cost = 0.0;

	/** How many share one slot. */
	UPROPERTY()
	int32 StackLimit = 0;

	UPROPERTY()
	FVeyraItemStatsTuning Stats;

	/** At most one: an ability Abilities.json defines. */
	UPROPERTY()
	TArray<FVeyraContentId> Active;

	/** Exactly one on a Masterwork, none below: an ID one of the Attunement maps defines. */
	UPROPERTY()
	TArray<FVeyraContentId> Attunement;
};

/** The shop's own rules (Economy & Progression Bible §10, §12; ADR-012 §9). */
USTRUCT()
struct FVeyraShopTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	int32 InventorySlots = 0;

	/** What selling returns, as a fraction of the Gold spent on the item's present form. */
	UPROPERTY()
	double ResaleFraction = 0.0;

	/** Items of this tier and above may be held once each. */
	UPROPERTY()
	int32 UniqueFromTier = 0;

	UPROPERTY()
	int32 MaxBoots = 0;
};

/** What a consumable does when used (Item Bible §10). */
USTRUCT()
struct FVeyraConsumableTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Health restored over the duration, in equal parts. */
	UPROPERTY()
	double HealthRestored = 0.0;

	UPROPERTY()
	double DurationSeconds = 0.0;

	/** What selling an unused one returns, as a fraction of its cost. */
	UPROPERTY()
	double ResaleFraction = 0.0;

	/**
	 * 0 for one that is used up. Above 0, it is refillable (Item Bible §10; ADR-022 §6): bought with
	 * this many charges, it spends one on each use and never goes, refills at its holder's fountain
	 * and when its holder's side secures a Flux Well, and is held once.
	 */
	UPROPERTY()
	int32 Charges = 0;
};

/** Weight of War: Physical Power from bonus Health (Item Bible §8). */
USTRUCT()
struct FVeyraWeightOfWarTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	double BonusHealthFraction = 0.0;
};

/** Overcharge: a percentage of total Magic Power (Item Bible §9). */
USTRUCT()
struct FVeyraOverchargeTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	double MagicPowerFraction = 0.0;
};

/** Perfect Cut: critical strikes deal more damage (Item Bible §8; ADR-022 §3). */
USTRUCT()
struct FVeyraPerfectCutTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Added to the holder's Crit Damage, as a fraction of the attack's damage. */
	UPROPERTY()
	double CritDamageBonus = 0.0;
};

/** A buff that stacks on each qualifying hit and refreshes, up to a cap: Spool Up and Overcycle (Item Bible §8, §9). */
USTRUCT()
struct FVeyraStackingAttunementTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	double PerStack = 0.0;

	UPROPERTY()
	int32 MaxStacks = 0;

	UPROPERTY()
	double DurationSeconds = 0.0;
};

/** The Items domain's tuning, bound from Game/Tuning/Items.json (ADR-006 §6, ADR-012 §3). */
USTRUCT()
struct FVeyraItemsTuning
{
	GENERATED_BODY()

	/** The Items.json format this build reads (a schema version marker, not tuning). */
	static constexpr int32 SchemaVersion = 2;

	UPROPERTY()
	FVeyraShopTuning Shop;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraItemDefinition> Items;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraConsumableTuning> Consumables;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraWeightOfWarTuning> WeightOfWar;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraOverchargeTuning> Overcharge;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraStackingAttunementTuning> SpoolUp;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraStackingAttunementTuning> Overcycle;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraPerfectCutTuning> PerfectCut;
};

/** The Items domain's checks that a schema cannot express (ADR-012 §3). */
namespace VeyraItems
{
	/**
	 * Problems with Tuning, each a JSON pointer and a message; empty when it is consistent. Tier 1 has
	 * no recipe and no Attunement; Tier 2 has a recipe and no Attunement; Tier 3 has a recipe and
	 * exactly one Attunement (Item Bible §2, §11); Boots stop at Tier 2 (§5); every component is a
	 * lower tier than its recipe, so recipes never loop; a consumable is a Tier 1 item with its own
	 * entry, and only a consumable stacks; every Attunement is defined in exactly one map.
	 */
	VEYRAITEMS_API TArray<FString> Validate(const FVeyraItemsTuning& Tuning);

	/** The Gold an item costs from nothing: its cost and every component's, all the way down (Economy §12's "present form"). */
	VEYRAITEMS_API double TotalCost(const FVeyraItemsTuning& Tuning, const FVeyraContentId& Item);
}
