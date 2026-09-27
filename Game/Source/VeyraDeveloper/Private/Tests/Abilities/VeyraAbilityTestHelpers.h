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

		/** Grants Ability in Slot and spends the level-1 skill point on it. */
		static bool Learn(AVeyraVanguardCharacter& Vanguard, EVeyraAbilitySlot Slot, const FVeyraContentId& Ability)
		{
			APlayerState* PlayerState = Vanguard.GetPlayerState();
			UVeyraProgressionComponent* Progression = PlayerState->FindComponentByClass<UVeyraProgressionComponent>();
			Progression->Initialize(FVeyraStatGrowth(), 0.0);
			return PlayerState->FindComponentByClass<UVeyraAbilityLoadoutComponent>()->Grant(*Vanguard.GetAbilitySystemComponent(), Slot, Ability)
				&& Progression->AllocateRank(Slot) == EVeyraRankRefusal::None;
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
