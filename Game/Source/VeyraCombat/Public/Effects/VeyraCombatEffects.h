// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameplayEffect.h"

#include "VeyraCombatEffects.generated.h"

// The Gameplay Effects Combat's verbs apply (VeyraCombatVerbs.h). Each is a C++ definition with no
// numbers of its own: every amount and duration arrives on the spec from the caller's data
// (ADR-006 §6), so nothing tunable lives in a binary asset.

/**
 * One damage event: an instant effect whose execution runs the canonical pipeline (Combat Bible §25).
 * Its components are Damage.Type SetByCaller magnitudes; the event's own penetration, when it has
 * any, uses the SetByCaller names below.
 */
UCLASS()
class VEYRACOMBAT_API UVeyraDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UVeyraDamageEffect();

	static const FName PhysicalPenetrationFlatName;
	static const FName PhysicalPenetrationRetainedName;
	static const FName MagicPenetrationFlatName;
	static const FName MagicPenetrationRetainedName;
};

/**
 * One shield (Combat Bible §7). Its category and amount are the spec's single Shield.Type
 * SetByCaller magnitude; its duration is set on the spec.
 */
UCLASS()
class VEYRACOMBAT_API UVeyraShieldEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UVeyraShieldEffect();
};

/** One grant of Temporary Health (Combat Bible §7): a TemporaryHealth SetByCaller amount and a duration. */
UCLASS()
class VEYRACOMBAT_API UVeyraTemporaryHealthEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UVeyraTemporaryHealthEffect();
};

/**
 * The stat part of one status (ADR-009 §1): a duration effect with one percentage modifier for each
 * stat a status can change. Each modifier's multiplier is a SetByCaller value named below; the
 * status ledger sets every one, 1 for the stats the status leaves alone.
 */
UCLASS()
class VEYRACOMBAT_API UVeyraStatusEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UVeyraStatusEffect();

	static const FName MoveSpeedMultiplierName;
	static const FName AttackSpeedMultiplierName;
	static const FName TenacityMultiplierName;
	static const FName IncomingDamageMultiplierName;
	static const FName DisplacementMultiplierName;
	static const FName HealthRegenMultiplierName;
	static const FName OutgoingDamageMultiplierName;
};

/**
 * A status with no stat change, such as a Stun or a Slow (ADR-009 §1): a duration effect with no
 * modifiers, so the status freezes with the world and ends at death like every other.
 */
UCLASS()
class VEYRACOMBAT_API UVeyraStatusMarkerEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UVeyraStatusMarkerEffect();
};

/**
 * A unit's equipment (ADR-012 §6): an infinite effect with one flat modifier for each stat an item
 * can add and a percentage factor for Magic Power, each a SetByCaller value named below. One is
 * active at a time; VeyraCombat::SetEquipmentStats replaces it whenever the equipment changes.
 */
UCLASS()
class VEYRACOMBAT_API UVeyraEquipmentEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UVeyraEquipmentEffect();

	static const FName MaxHealthName;
	static const FName HealthRegenName;
	static const FName PhysicalPowerName;
	static const FName MagicPowerName;
	static const FName AttackSpeedName;
	static const FName AbilityHasteName;
	static const FName MoveSpeedName;
	static const FName MagicPenetrationFlatName;
	static const FName MagicPowerMultiplierName;
	static const FName CritChanceName;
	static const FName CritDamageBonusName;
};

/** One resource cost (Combat Bible §27): an instant effect whose execution spends the cost from the spec. */
UCLASS()
class VEYRACOMBAT_API UVeyraResourceSpendEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UVeyraResourceSpendEffect();
};
