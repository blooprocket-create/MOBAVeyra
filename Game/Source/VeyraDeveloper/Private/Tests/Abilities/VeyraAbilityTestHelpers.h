// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Components/ActorTestSpawner.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Movement/VeyraMovementComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Shapes/VeyraShapes.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Tests/Abilities/VeyraTestFluxborn.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuning.h"
#include "VeyraAbilitiesVerbs.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardCharacter.h"

namespace VeyraAbilitiesTests
{
	inline FVeyraContentId ArchetypeTestId(const TCHAR* Text)
	{
		return FVeyraContentId::FromText(Text).GetValue();
	}

	inline FVeyraShape CircleOf(double Radius)
	{
		FVeyraShape Shape;
		Shape.Kind = EVeyraShapeKind::Circle;
		Shape.Radius = Radius;
		return Shape;
	}

	/** A status record for test tuning. Test fixture values. */
	inline FVeyraStatusTuning StatusOf(EVeyraStatusKind Kind, double Magnitude, double DurationSeconds)
	{
		FVeyraStatusTuning Status;
		Status.Kind = Kind;
		Status.Magnitude = Magnitude;
		Status.DurationSeconds = DurationSeconds;
		return Status;
	}

	/** An instant cast: no windup, no recovery, one value for every rank. */
	inline FVeyraCastTuning InstantCast(double CastRange, double CooldownSeconds, double ResourceCost)
	{
		FVeyraCastTuning Cast;
		Cast.CooldownSecondsByRank = { CooldownSeconds };
		Cast.ResourceCostByRank = { ResourceCost };
		Cast.CastRange = CastRange;
		return Cast;
	}

	/** Fixture values for a test companion (ADR-034 §3) and the tests about it, not tuning. */
	namespace CompanionFixture
	{
		constexpr double Radius = 40.0;
		constexpr double HalfHeight = 60.0;
		constexpr double Health = 400.0;
		constexpr double HealthGrowth = 50.0;
		constexpr double Power = 20.0;
		constexpr double PowerGrowth = 5.0;
		constexpr double Share = 0.5;
		constexpr double OwnerPower = 100.0;
		constexpr double Follow = 200.0;
		constexpr double Leash = 1000.0;
		constexpr double Acquire = 400.0;
		constexpr double OwnerTarget = 3.0;
		constexpr double Reform = 5.0;
		constexpr double Think = 0.25;
		constexpr double Reach = 125.0;
		constexpr double Windup = 0.3;
		constexpr double Near = 150.0;
		constexpr double Far = 3000.0;
		constexpr double Graze = 10.0;
		constexpr double Lethal = 100000.0;
		constexpr double ManyLevels = 5000.0;
		constexpr double Tolerance = 1e-3;
		constexpr float Step = 0.1f;
	}

	/** A melee companion with the example stats and no resource, its own values from CompanionFixture. */
	inline FVeyraCompanionTuning ExampleCompanion()
	{
		using namespace CompanionFixture;
		FVeyraCompanionTuning Companion;
		Companion.CapsuleRadius = Radius;
		Companion.CapsuleHalfHeight = HalfHeight;
		Companion.Stats = VeyraCombatTests::ExampleStats();
		Companion.Stats.MaxHealth = Health;
		Companion.Stats.MaxResource = 0.0;
		Companion.Stats.ResourceRegen = 0.0;
		Companion.Stats.MagicPower = Power;
		Companion.Growth.MaxHealth = HealthGrowth;
		Companion.Growth.MagicPower = PowerGrowth;
		Companion.OwnerMagicPowerShare = Share;
		Companion.BasicAttack.Range = Reach;
		Companion.BasicAttack.DamageType = EVeyraDamageType::Magic;
		Companion.BasicAttack.MagicPowerRatio = 1.0;
		Companion.BasicAttack.WindupFraction = Windup;
		Companion.BasicAttack.AcquisitionRadius = Acquire;
		Companion.FollowDistance = Follow;
		Companion.LeashRange = Leash;
		Companion.AcquireRange = Acquire;
		Companion.OwnerTargetSeconds = OwnerTarget;
		Companion.ReformSeconds = Reform;
		Companion.ThinkSeconds = Think;
		return Companion;
	}

	/** Units in a test world, each with the example stats, on a side, at a place. */
	struct FArchetypeTestWorld
	{
		FActorTestSpawner& Spawner;

		AVeyraVanguardCharacter& Spawn(EVeyraTeam Team, const FVector& Location)
		{
			AVeyraPlayerState& PlayerState = Spawner.SpawnActor<AVeyraPlayerState>();
			PlayerState.SetVeyraTeam(Team);
			VeyraCombat::InitializeStats(*PlayerState.GetAbilitySystemComponent(), VeyraCombatTests::ExampleStats());
			AVeyraVanguardCharacter& Vanguard = Spawner.SpawnActorAt<AVeyraVanguardCharacter>(Location, FRotator::ZeroRotator);
			Vanguard.SetPlayerState(&PlayerState);
			Vanguard.GetVeyraMovement()->SetMovementMode(MOVE_Walking);
			return Vanguard;
		}

		/** A unit that is not a Vanguard. */
		AVeyraTestFluxborn& SpawnFluxborn(EVeyraTeam Team, const FVector& Location)
		{
			AVeyraTestFluxborn& Fluxborn = Spawner.SpawnActorAt<AVeyraTestFluxborn>(Location, FRotator::ZeroRotator);
			Fluxborn.SetVeyraTeam(Team);
			VeyraCombat::InitializeStats(*Fluxborn.GetAbilitySystemComponent(), VeyraCombatTests::ExampleStats());
			Fluxborn.GetVeyraMovement()->SetMovementMode(MOVE_Walking);
			return Fluxborn;
		}

		/** A structure (Combat Bible §33), with the example stats. */
		AVeyraTestStructure& SpawnStructure(EVeyraTeam Team, const FVector& Location)
		{
			AVeyraTestStructure& Structure = Spawner.SpawnActorAt<AVeyraTestStructure>(Location, FRotator::ZeroRotator);
			Structure.SetVeyraTeam(Team);
			VeyraCombat::InitializeStats(*Structure.GetAbilitySystemComponent(), VeyraCombatTests::ExampleStats());
			return Structure;
		}

		/** Grants Ability in Slot and spends the level-1 skill point on it. */
		static bool Learn(AVeyraVanguardCharacter& Vanguard, EVeyraAbilitySlot Slot, const FVeyraContentId& Ability)
		{
			APlayerState* PlayerState = Vanguard.GetPlayerState();
			UVeyraProgressionComponent* Progression = PlayerState->FindComponentByClass<UVeyraProgressionComponent>();
			Progression->Initialize(FVeyraStatGrowth(), 0.0);
			return PlayerState->FindComponentByClass<UVeyraAbilityLoadoutComponent>()->Grant(*Vanguard.GetAbilitySystemComponent(), Slot, Ability)
				&& Progression->AllocateRank(Slot) == EVeyraRankRefusal::None;
		}

		/** Grants Ability in a slot that takes no ranks, such as a Flux Spell's (ADR-015 §1), at Level 1. */
		static bool Equip(AVeyraVanguardCharacter& Vanguard, EVeyraAbilitySlot Slot, const FVeyraContentId& Ability)
		{
			APlayerState* PlayerState = Vanguard.GetPlayerState();
			UVeyraProgressionComponent* Progression = PlayerState->FindComponentByClass<UVeyraProgressionComponent>();
			if (!Progression->IsInitialized())
			{
				Progression->Initialize(FVeyraStatGrowth(), 0.0);
			}
			return PlayerState->FindComponentByClass<UVeyraAbilityLoadoutComponent>()->Grant(*Vanguard.GetAbilitySystemComponent(), Slot, Ability);
		}

		static EVeyraCastRejection CastAt(AVeyraVanguardCharacter& Caster, EVeyraAbilitySlot Slot, const FVector& Point)
		{
			FVeyraCastTarget Target;
			Target.bHasLocation = true;
			Target.Location = Point;
			return VeyraAbilities::TryCast(*Caster.GetAbilitySystemComponent(), Slot, Target);
		}

		/** Whether Unit has the status with ID Status. */
		static bool Has(const AActor& Unit, const TCHAR* Status)
		{
			const FVeyraContentId Id = ArchetypeTestId(Status);
			const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
			const UVeyraStatusComponent* Statuses = AbilitySystem ? AbilitySystem->GetOwner()->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
			return Statuses && Statuses->GetLedger().Entries.ContainsByPredicate([&Id](const FVeyraStatusEntry& Entry) { return Entry.Id == Id; });
		}

		static double HealthLost(const AActor& Unit)
		{
			const UAbilitySystemComponent& AbilitySystem = *UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
			return AbilitySystem.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) - AbilitySystem.GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
		}
	};
}
