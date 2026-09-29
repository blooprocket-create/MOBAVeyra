// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraBasicAttackComponent.h"
#include "CQTest.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	// Veyra.Abilities.Guards.*: a directional guard and an attack amplification, as damage lands (ADR-018 §2).
	TEST_CLASS(Guards, "Veyra.Abilities")
	{
		// Fixture values.
		static constexpr double Apart = 200.0;
		static constexpr double Hit = 100.0;
		static constexpr double Guard = 0.5;
		static constexpr double Arc = 90.0;
		static constexpr double Amplified = 0.5;
		static constexpr double LongSeconds = 60.0;
		static constexpr double Tolerance = 1e-3;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Guarded = nullptr;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Guarded = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
		}

		static FVeyraRawDamageEvent PhysicalHit()
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::Physical, Hit });
			return Damage;
		}

		TEST_METHOD(AGuardTakesLessFromTheFrontThanFromBehind)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Ahead = World.Spawn(EVeyraTeam::B, FVector(Apart, 0.0, 0.0));
			AVeyraVanguardCharacter& Behind = World.Spawn(EVeyraTeam::B, FVector(-Apart, 0.0, 0.0));
			UAbilitySystemComponent& Self = *Guarded->GetAbilitySystemComponent();
			FVeyraStatusSpec Countersteer;
			Countersteer.Id = ArchetypeTestId(TEXT("test_countersteer"));
			Countersteer.Kind = EVeyraStatusKind::DirectionalDamageReduction;
			Countersteer.Magnitude = Guard;
			Countersteer.ArcDegrees = Arc;
			Countersteer.DurationSeconds = LongSeconds;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Self, Self, Countersteer)));
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Ahead.GetAbilitySystemComponent(), Self, PhysicalHit())));
			const double FromAhead = World.HealthLost(*Guarded);
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Behind.GetAbilitySystemComponent(), Self, PhysicalHit())));
			const double FromBehind = World.HealthLost(*Guarded) - FromAhead;
			ASSERT_THAT(IsTrue(FromAhead > 0.0 && FMath::IsNearlyEqual(FromAhead, FromBehind * (1.0 - Guard), Tolerance),
				FString::Printf(TEXT("ahead %g, behind %g"), FromAhead, FromBehind)));
		}

		TEST_METHOD(AnAmplifiedAttackDealsMoreAgainstTheKindsItNames)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Apart / 2.0, 0.0, 0.0));
			AVeyraTestFluxborn& Minion = World.SpawnFluxborn(EVeyraTeam::B, FVector(0.0, Apart / 2.0, 0.0));
			UVeyraBasicAttackComponent* Attacks = Guarded->GetPlayerState()->FindComponentByClass<UVeyraBasicAttackComponent>();
			FVeyraBasicAttackProfile Profile;
			Profile.Range = Apart;
			Profile.DamageType = EVeyraDamageType::TrueDamage;
			Profile.PhysicalPowerRatio = 1.0;
			Profile.WindupFraction = 0.25;
			Profile.AcquisitionRadius = Apart;
			ASSERT_THAT(IsTrue(Attacks && Attacks->SetProfile(Profile)));
			UAbilitySystemComponent& Self = *Guarded->GetAbilitySystemComponent();
			FVeyraStatusSpec Hunt;
			Hunt.Id = ArchetypeTestId(TEXT("test_hunt"));
			Hunt.Kind = EVeyraStatusKind::AttackDamageAmplification;
			Hunt.Magnitude = Amplified;
			Hunt.DurationSeconds = LongSeconds;
			Hunt.UnitKinds = { EVeyraUnitKind::Vanguard };
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Self, Self, Hunt)));
			const double Base = VeyraCombatTests::ExampleStats().PhysicalPower;

			ASSERT_THAT(IsTrue(Attacks->StartAttack(Enemy) == EVeyraAttackRejection::None));
			Attacks->Commit();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Enemy), Base * (1.0 + Amplified), Tolerance)));
			// Waits out the attack's interval, then strikes a Fluxborn: no amplification.
			UWorld& Clock = Spawner.GetWorld();
			while (Clock.GetTimeSeconds() < Attacks->GetNextAttackAt())
			{
				Clock.Tick(LEVELTICK_TimeOnly, 0.1f);
			}
			ASSERT_THAT(IsTrue(Attacks->StartAttack(Minion) == EVeyraAttackRejection::None));
			Attacks->Commit();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Minion), Base, Tolerance)));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
