// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "Life/VeyraLifeComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardCharacter.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	// Veyra.Combat.TargetRules.*: who may target whom, measured edge to edge with the server's latency
	// tolerance (Combat Bible §29, §30, §40).
	TEST_CLASS(TargetRules, "Veyra.Combat")
	{
		static constexpr double StartingMaxHealth = 100.0;
		static constexpr double Separation = 1000.0;

		FActorTestSpawner Spawner;

		/** A Vanguard on Team, at Location, with a living combatant PlayerState. */
		AVeyraVanguardCharacter& SpawnVanguard(EVeyraTeam Team, const FVector& Location)
		{
			AVeyraPlayerState& PlayerState = Spawner.SpawnActor<AVeyraPlayerState>();
			PlayerState.SetVeyraTeam(Team);
			VeyraCombat::InitializeVitals(*PlayerState.GetAbilitySystemComponent(), StartingMaxHealth);
			AVeyraVanguardCharacter& Vanguard = Spawner.SpawnActorAt<AVeyraVanguardCharacter>(Location, FRotator::ZeroRotator);
			Vanguard.SetPlayerState(&PlayerState);
			return Vanguard;
		}

		double Tolerance() const
		{
			return UVeyraCombatTuningSubsystem::Get().Targeting.ServerRangeTolerance;
		}

		TEST_METHOD(RangeIsMeasuredBetweenEdges)
		{
			const AVeyraVanguardCharacter& Caster = SpawnVanguard(EVeyraTeam::A, FVector::ZeroVector);
			const AVeyraVanguardCharacter& Target = SpawnVanguard(EVeyraTeam::B, FVector(Separation, 0.0, 0.0));
			const double Edges = Separation - Caster.GetSimpleCollisionRadius() - Target.GetSimpleCollisionRadius();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraTargeting::EdgeToEdgeDistance(Caster, Target), Edges)));
		}

		TEST_METHOD(TheServerAllowsItsTolerance)
		{
			const AVeyraVanguardCharacter& Caster = SpawnVanguard(EVeyraTeam::A, FVector::ZeroVector);
			const AVeyraVanguardCharacter& Target = SpawnVanguard(EVeyraTeam::B, FVector(Separation, 0.0, 0.0));
			const double Edges = VeyraTargeting::EdgeToEdgeDistance(Caster, Target);
			ASSERT_THAT(IsTrue(VeyraTargeting::IsWithinCastRange(Caster, Target, Edges)));
			ASSERT_THAT(IsTrue(VeyraTargeting::IsWithinCastRange(Caster, Target, Edges - Tolerance())));
			ASSERT_THAT(IsFalse(VeyraTargeting::IsWithinCastRange(Caster, Target, Edges - Tolerance() - 1.0)));
		}

		TEST_METHOD(ChecksAnEnemyTarget)
		{
			AVeyraVanguardCharacter& Caster = SpawnVanguard(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Ally = SpawnVanguard(EVeyraTeam::A, FVector(0.0, Separation, 0.0));
			AVeyraVanguardCharacter& Enemy = SpawnVanguard(EVeyraTeam::B, FVector(Separation, 0.0, 0.0));
			const double Range = Separation;
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(Caster, &Enemy, Range) == EVeyraTargetValidity::Valid));
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(Caster, &Caster, Range) == EVeyraTargetValidity::Caster));
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(Caster, &Ally, Range) == EVeyraTargetValidity::NotHostile));
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(Caster, nullptr, Range) == EVeyraTargetValidity::NotACombatant));
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(Caster, &Enemy, Range / 2.0) == EVeyraTargetValidity::OutOfRange));

			Enemy.GetPlayerState()->FindComponentByClass<UVeyraLifeComponent>()->SetState(EVeyraLifeState::Dead);
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(Caster, &Enemy, Range) == EVeyraTargetValidity::Dead));
		}

		TEST_METHOD(NoSideIsNeverAnImplicitEnemy)
		{
			AVeyraVanguardCharacter& Caster = SpawnVanguard(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Neutral = SpawnVanguard(EVeyraTeam::None, FVector(Separation, 0.0, 0.0));
			AVeyraVanguardCharacter& Enemy = SpawnVanguard(EVeyraTeam::B, FVector(0.0, Separation, 0.0));
			ASSERT_THAT(IsFalse(VeyraTargeting::AreHostile(&Caster, &Neutral)));
			ASSERT_THAT(IsFalse(VeyraTargeting::AreHostile(&Neutral, &Enemy)));
			ASSERT_THAT(IsTrue(VeyraTargeting::AreHostile(&Caster, &Enemy)));
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(Caster, &Neutral, Separation) == EVeyraTargetValidity::NotHostile));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
