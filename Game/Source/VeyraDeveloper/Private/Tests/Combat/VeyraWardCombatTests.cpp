// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "AbilitySystemGlobals.h"
#include "CQTest.h"
#include "Shapes/VeyraShapes.h"
#include "Targeting/VeyraTargeting.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tests/Abilities/VeyraTestFluxborn.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraWardCombatTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	FVeyraRawDamageEvent Blow(double Amount, EVeyraDamageDelivery Delivery)
	{
		FVeyraRawDamageEvent Damage;
		Damage.Components.Add({ EVeyraDamageType::Physical, Amount });
		Damage.Delivery = Delivery;
		return Damage;
	}

	UAbilitySystemComponent& AbilitySystemOf(AActor& Unit)
	{
		return *UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
	}

	// Veyra.Combat.WardHits.*: a ward counts hits, not damage. Only a Vanguard's basic attack reaches
	// it, one point of Health each; abilities, procs, other units, statuses and areas pass it by
	// (ADR-016 §6, League's wards).
	TEST_CLASS(WardHits, "Veyra.Combat")
	{
		// Fixture values, not tuning.
		static constexpr double Small = 1.0;
		static constexpr double Huge = 5000.0;
		static constexpr double Range = 1000.0;
		static constexpr double LongSeconds = 60.0;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Attacker = nullptr;
		AVeyraTestFluxborn* Minion = nullptr;
		AVeyraTestWard* Ward = nullptr;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Attacker = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Minion = &World.SpawnFluxborn(EVeyraTeam::A, FVector(0.0, 300.0, 0.0));
			Ward = &Spawner.SpawnActorAt<AVeyraTestWard>(FVector(300.0, 0.0, 0.0), FRotator::ZeroRotator);
			Ward->SetVeyraTeam(EVeyraTeam::B);
			VeyraCombat::InitializeStats(*Ward->GetAbilitySystemComponent(), VeyraCombatTests::ExampleStats());
		}

		TEST_METHOD(AVanguardsBasicAttackTakesOnePointWhateverItsDamage)
		{
			UAbilitySystemComponent& Target = AbilitySystemOf(*Ward);
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(AbilitySystemOf(*Attacker), Target, Blow(Huge, EVeyraDamageDelivery::BasicAttack))));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(*Ward) == 1.0, TEXT("a huge blow is one hit")));
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(AbilitySystemOf(*Attacker), Target, Blow(Small, EVeyraDamageDelivery::BasicAttack))));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(*Ward) == 2.0, TEXT("so is a small one")));
		}

		TEST_METHOD(NothingButAVanguardsBasicAttackReachesAWard)
		{
			UAbilitySystemComponent& Target = AbilitySystemOf(*Ward);
			for (const EVeyraDamageDelivery Refused : { EVeyraDamageDelivery::Ability, EVeyraDamageDelivery::Proc, EVeyraDamageDelivery::StructureAttack,
				EVeyraDamageDelivery::Developer })
			{
				ASSERT_THAT(IsFalse(VeyraCombat::DealDamage(AbilitySystemOf(*Attacker), Target, Blow(Huge, Refused))));
			}
			ASSERT_THAT(IsFalse(VeyraCombat::DealDamage(AbilitySystemOf(*Minion), Target, Blow(Huge, EVeyraDamageDelivery::BasicAttack)),
				TEXT("a Fluxborn's basic attack")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(*Ward) == 0.0));
		}

		TEST_METHOD(NoStatusAffectsAWard)
		{
			FVeyraStatusSpec Chill;
			Chill.Id = FVeyraContentId::FromText(TEXT("chill")).GetValue();
			Chill.Kind = EVeyraStatusKind::Slow;
			Chill.Magnitude = 0.3;
			Chill.DurationSeconds = LongSeconds;
			UAbilitySystemComponent& Target = AbilitySystemOf(*Ward);
			ASSERT_THAT(IsFalse(VeyraCombat::ApplyStatus(AbilitySystemOf(*Attacker), Target, Chill)));
			ASSERT_THAT(IsFalse(VeyraCombat::ApplyStatus(Target, Target, Chill), TEXT("not even its own")));
		}

		TEST_METHOD(OnlyABasicAttackMayTargetAWard)
		{
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(*Attacker, Ward, Range) == EVeyraTargetValidity::Ward));
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(*Attacker, Ward, Range, EVeyraStructureTargeting::Allow) == EVeyraTargetValidity::Valid));
		}

		TEST_METHOD(NoAreaOrAttackMoveGathersAWard)
		{
			FVeyraShape Everything;
			Everything.Kind = EVeyraShapeKind::Circle;
			Everything.Radius = Range;
			const FVeyraPlacedShape Placed{ Everything, FVector::ZeroVector, FVector::ForwardVector };
			for (const EVeyraStructureTargeting Structures : { EVeyraStructureTargeting::Refuse, EVeyraStructureTargeting::Allow })
			{
				const TArray<AActor*> Gathered = VeyraShapes::GatherUnits(Spawner.GetWorld(), Placed, [](const AActor&) { return true; }, Structures);
				ASSERT_THAT(IsTrue(Gathered.Contains(Minion) && !Gathered.Contains(Ward)));
			}
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
