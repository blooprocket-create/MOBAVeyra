// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraBasicAttackComponent.h"
#include "CQTest.h"
#include "Delivery/VeyraProjectile.h"
#include "EngineUtils.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraVanguards.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	// Veyra.Vanguards.Breach.*: Bryn's passive, by what her shots do (Character Bible §19; ADR-009 §5).
	// Bystanders stand behind her target, where explosions land, and each shot's projectile is flown
	// to its target. Expectations come from the committed tuning.
	TEST_CLASS(Breach, "Veyra.Vanguards")
	{
		// Fixture values: where the target stands; bystanders just behind it (inside Breach's
		// explosion) and further back (inside only Breach Round's longer blast); and how long a shot
		// gets to land.
		static constexpr double Apart = 300.0;
		static constexpr double JustBehind = 100.0;
		static constexpr double FarBehind = 250.0;
		static constexpr double FlightSeconds = 2.0;
		static constexpr double Tolerance = 1e-3;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Body = nullptr;
		UVeyraBasicAttackComponent* Attacks = nullptr;

		BEFORE_EACH()
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			Body = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraPlayerState* Participant = Body->GetPlayerState<AVeyraPlayerState>();
			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(*Participant->GetAbilitySystemComponent(), FVeyraContentId::FromText(TEXT("bryn")).GetValue());
			ASSERT_THAT(IsTrue(Prepared.bPrepared && Prepared.Passive));
			Participant->SetPassive(Prepared.Passive);
			Attacks = Participant->FindComponentByClass<UVeyraBasicAttackComponent>();
		}

		static int32 HitsToBreach()
		{
			return UVeyraVanguardsTuningSubsystem::FindBreach(FVeyraContentId::FromText(TEXT("bryn_breach")).GetValue())->HitsToBreach;
		}

		/** Waits for the next attack, attacks Target, ends the windup at once and flies the shot home. */
		bool Shoot(AActor& Target)
		{
			// Fixture value: a step below the longest frame the world accepts in one tick.
			constexpr float StepSeconds = 0.1f;
			UWorld& World = Spawner.GetWorld();
			while (World.GetTimeSeconds() < Attacks->GetNextAttackAt())
			{
				World.Tick(LEVELTICK_TimeOnly, StepSeconds);
			}
			if (Attacks->StartAttack(Target) != EVeyraAttackRejection::None)
			{
				return false;
			}
			Attacks->Commit();
			for (TActorIterator<AVeyraProjectile> It(&World); It; ++It)
			{
				if (It->GetHomingTarget() == &Target)
				{
					It->AdvanceBy(FlightSeconds);
				}
			}
			return true;
		}

		static double Lost(const AActor& Unit)
		{
			return VeyraAbilitiesTests::FArchetypeTestWorld::HealthLost(Unit);
		}

		TEST_METHOD(EveryThirdConsecutiveShotBreaches)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Apart, 0.0, 0.0));
			AVeyraVanguardCharacter& Bystander = World.Spawn(EVeyraTeam::B, FVector(Apart + JustBehind, 0.0, 0.0));
			double PlainShot = 0.0;
			for (int32 Shot = 1; Shot <= HitsToBreach() * 2; ++Shot)
			{
				const double TargetBefore = Lost(Enemy);
				const double BystanderBefore = Lost(Bystander);
				ASSERT_THAT(IsTrue(Shoot(Enemy)));
				const double Dealt = Lost(Enemy) - TargetBefore;
				const bool bBreaches = Shot % HitsToBreach() == 0;
				ASSERT_THAT(IsTrue((Lost(Bystander) > BystanderBefore) == bBreaches, FString::Printf(TEXT("shot %d: the explosion behind the target"), Shot)));
				if (Shot == 1)
				{
					PlainShot = Dealt;
				}
				ASSERT_THAT(IsTrue(bBreaches ? Dealt > PlainShot + Tolerance : FMath::IsNearlyEqual(Dealt, PlainShot, Tolerance),
					FString::Printf(TEXT("shot %d dealt %g, a plain shot %g"), Shot, Dealt, PlainShot)));
			}
		}

		TEST_METHOD(ChangingTargetsStartsTheCountAgain)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& First = World.Spawn(EVeyraTeam::B, FVector(Apart, 0.0, 0.0));
			AVeyraVanguardCharacter& Second = World.Spawn(EVeyraTeam::B, FVector(0.0, Apart, 0.0));
			AVeyraVanguardCharacter& Bystander = World.Spawn(EVeyraTeam::B, FVector(0.0, Apart + JustBehind, 0.0));
			for (int32 Shot = 1; Shot < HitsToBreach(); ++Shot)
			{
				ASSERT_THAT(IsTrue(Shoot(First)));
			}
			ASSERT_THAT(IsTrue(Shoot(Second)));
			ASSERT_THAT(IsTrue(Attacks->GetChain() == 1 && Lost(Bystander) == 0.0, TEXT("the new target has no Breach yet")));
		}

		TEST_METHOD(BreachRoundsBlastReplacesTheBreachExplosion)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Apart, 0.0, 0.0));
			AVeyraVanguardCharacter& Near = World.Spawn(EVeyraTeam::B, FVector(Apart + JustBehind, 0.0, 0.0));
			AVeyraVanguardCharacter& Far = World.Spawn(EVeyraTeam::B, FVector(Apart + FarBehind, 0.0, 0.0));
			for (int32 Shot = 1; Shot < HitsToBreach(); ++Shot)
			{
				ASSERT_THAT(IsTrue(Shoot(Enemy)));
			}
			// Her kit already holds the round in Q; the level-1 skill point learns it.
			UVeyraProgressionComponent* Progression = Body->GetPlayerState()->FindComponentByClass<UVeyraProgressionComponent>();
			ASSERT_THAT(IsTrue(Progression->AllocateRank(EVeyraAbilitySlot::Q) == EVeyraRankRefusal::None));
			ASSERT_THAT(IsTrue(VeyraAbilitiesTests::FArchetypeTestWorld::CastAt(*Body, EVeyraAbilitySlot::Q, Enemy.GetActorLocation()) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Shoot(Enemy)));

			// The round's longer blast reaches the far bystander, and the near one takes that blast
			// alone, as much as the far one: one explosion, not two.
			ASSERT_THAT(IsTrue(Lost(Far) > 0.0, TEXT("the round's blast replaced Breach's")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Lost(Near), Lost(Far), Tolerance), FString::Printf(TEXT("near %g, far %g"), Lost(Near), Lost(Far))));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
