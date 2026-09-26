// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameplayEffect.h"

#include "VeyraCombatEffects.generated.h"

// The Gameplay Effects Combat's verbs apply (VeyraCombatVerbs.h). Each is a C++ definition with no
// numbers of its own: every amount and duration arrives on the spec from the caller's data
// (ADR-006 §6), so nothing tunable lives in a binary asset.

/** One damage event: an instant effect whose execution runs the canonical pipeline (Combat Bible §25). */
UCLASS()
class VEYRACOMBAT_API UVeyraDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UVeyraDamageEffect();
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
