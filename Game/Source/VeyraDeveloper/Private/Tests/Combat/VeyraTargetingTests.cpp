// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "Life/VeyraLifeComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Tests/Abilities/VeyraTestFluxborn.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardCharacter.h"
#include "Targeting/VeyraVisibility.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	/** A world's vision that hides one unit from everyone, as fog or stealth would. */
	class FHidingVisibility final : public IVeyraVisibility
	{
	public:
		explicit FHidingVisibility(const AActor& InHidden)
			: Hidden(&InHidden)
		{
		}

		virtual bool CanSee(const UObject& /*Observer*/, const AActor& Target) const override { return &Target != Hidden; }
		virtual bool IsVisibleToTeam(EVeyraTeam /*Team*/, const AActor& Target) const override { return &Target != Hidden; }

	private:
		const AActor* Hidden;
	};

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

		TEST_METHOD(AnEnemyItCannotSeeIsNotATarget)
		{
			AVeyraVanguardCharacter& Caster = SpawnVanguard(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Hidden = SpawnVanguard(EVeyraTeam::B, FVector(Separation, 0.0, 0.0));
			AVeyraVanguardCharacter& Seen = SpawnVanguard(EVeyraTeam::B, FVector(0.0, Separation, 0.0));
			UVeyraVisibilityRegistry* Registry = Spawner.GetWorld().GetSubsystem<UVeyraVisibilityRegistry>();
			ASSERT_THAT(IsNotNull(Registry));
			// Without vision in the world, everything is visible (unit tests and development maps).
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(Caster, &Hidden, Separation) == EVeyraTargetValidity::Valid));

			// Knowing where a unit is does not permit targeting it (Vision Bible §1).
			FHidingVisibility Vision(Hidden);
			Registry->Register(Vision);
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(Caster, &Hidden, Separation) == EVeyraTargetValidity::NotVisible));
			ASSERT_THAT(IsFalse(VeyraTargeting::CanAcquire(&Caster, Hidden)));
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(Caster, &Seen, Separation) == EVeyraTargetValidity::Valid));
			ASSERT_THAT(IsFalse(VeyraVisibility::IsVisibleToTeam(EVeyraTeam::A, Hidden)));
			Registry->Unregister(Vision);
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(Caster, &Hidden, Separation) == EVeyraTargetValidity::Valid));
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

		TEST_METHOD(NeutralUnitsAreHostileToVanguardsButNotToLaneUnits)
		{
			// A neutral unit is the one explicit category on no side (ADR-014 §1).
			AVeyraVanguardCharacter& Caster = SpawnVanguard(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Enemy = SpawnVanguard(EVeyraTeam::B, FVector(0.0, Separation, 0.0));
			AVeyraTestWildlife& Creature = Spawner.SpawnActorAt<AVeyraTestWildlife>(FVector(Separation, 0.0, 0.0), FRotator::ZeroRotator);
			AVeyraTestWildlife& OtherCreature = Spawner.SpawnActorAt<AVeyraTestWildlife>(FVector(-Separation, 0.0, 0.0), FRotator::ZeroRotator);
			AVeyraTestFluxborn& Minion = Spawner.SpawnActorAt<AVeyraTestFluxborn>(FVector(0.0, -Separation, 0.0), FRotator::ZeroRotator);
			Minion.SetVeyraTeam(EVeyraTeam::B);
			AVeyraTestStructure& Tower = Spawner.SpawnActorAt<AVeyraTestStructure>(FVector(Separation, Separation, 0.0), FRotator::ZeroRotator);
			Tower.SetVeyraTeam(EVeyraTeam::B);
			ASSERT_THAT(IsTrue(VeyraTargeting::AreHostile(&Caster, &Creature) && VeyraTargeting::AreHostile(&Creature, &Enemy), TEXT("both sides fight it, and it fights back")));
			ASSERT_THAT(IsFalse(VeyraTargeting::AreHostile(&Minion, &Creature) || VeyraTargeting::AreHostile(&Creature, &Minion), TEXT("not lane Fluxborn")));
			ASSERT_THAT(IsFalse(VeyraTargeting::AreHostile(&Tower, &Creature) || VeyraTargeting::AreHostile(&Creature, &Tower), TEXT("not structures")));
			ASSERT_THAT(IsFalse(VeyraTargeting::AreHostile(&Creature, &OtherCreature), TEXT("not another neutral unit")));
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(Caster, &Creature, Separation) == EVeyraTargetValidity::Valid));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
