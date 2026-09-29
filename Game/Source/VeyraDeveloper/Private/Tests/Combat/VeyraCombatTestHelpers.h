// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/ActorTestSpawner.h"
#include "GameplayEffect.h"
#include "Stats/VeyraStatBlock.h"
#include "Statuses/VeyraStatusTypes.h"
#include "UObject/Package.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"

namespace VeyraCombatTests
{
	/** A Camouflage status with DetectionRadius (Combat Bible §11). Fixture values. */
	inline FVeyraStatusSpec CamouflageSpec(double DetectionRadius)
	{
		// Longer than any of these tests runs.
		constexpr double LongSeconds = 60.0;
		FVeyraStatusSpec Spec;
		Spec.Id = FVeyraContentId::FromText(TEXT("test_camouflage")).GetValue();
		Spec.Kind = EVeyraStatusKind::Camouflage;
		Spec.Magnitude = DetectionRadius;
		Spec.DurationSeconds = LongSeconds;
		return Spec;
	}

	/** Camouflages Unit, a unit with an Ability System Component, with DetectionRadius. */
	inline bool Camouflage(AActor& Unit, double DetectionRadius)
	{
		UAbilitySystemComponent* Abilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
		return Abilities && VeyraCombat::ApplyStatus(*Abilities, *Abilities, CamouflageSpec(DetectionRadius));
	}

	// Test effects are built at runtime and take their magnitudes as plain values, so every
	// application uses the Gameplay Ability System's default effect level.
	static constexpr float TestEffectLevel = 1.0f;

	/** A PlayerState in the test world, configured for combat exactly as in a match. */
	inline UAbilitySystemComponent& SpawnCombatant(FActorTestSpawner& Spawner)
	{
		AVeyraPlayerState& PlayerState = Spawner.SpawnActor<AVeyraPlayerState>();
		return *PlayerState.GetAbilitySystemComponent();
	}

	/** A complete, valid stat block. Test fixture values, not tuning. */
	inline FVeyraStatBlock ExampleStats()
	{
		FVeyraStatBlock Stats;
		Stats.MaxHealth = 600.0;
		Stats.MaxResource = 300.0;
		Stats.ResourceRegen = 4.0;
		Stats.Armor = 30.0;
		Stats.MagicResist = 25.0;
		Stats.PhysicalPower = 55.0;
		Stats.MagicPower = 10.0;
		Stats.AttackSpeed = 0.65;
		Stats.MoveSpeed = 335.0;
		return Stats;
	}

	struct FTestModifier
	{
		FGameplayAttribute Attribute;
		EGameplayModOp::Type Operation;
		float Magnitude;
	};

	/** A transient effect with constant-magnitude modifiers. */
	inline UGameplayEffect& NewTestEffect(EGameplayEffectDurationType Duration, TConstArrayView<FTestModifier> Modifiers)
	{
		UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
		Effect->DurationPolicy = Duration;
		for (const FTestModifier& Modifier : Modifiers)
		{
			FGameplayModifierInfo& Info = Effect->Modifiers.AddDefaulted_GetRef();
			Info.Attribute = Modifier.Attribute;
			Info.ModifierOp = Modifier.Operation;
			Info.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Modifier.Magnitude));
		}
		return *Effect;
	}

	inline FActiveGameplayEffectHandle ApplyToSelf(UAbilitySystemComponent& Unit, const UGameplayEffect& Effect)
	{
		return Unit.ApplyGameplayEffectToSelf(&Effect, TestEffectLevel, Unit.MakeEffectContext());
	}
}
