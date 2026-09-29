// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraBasicAttackComponent.h"
#include "CQTest.h"
#include "Delivery/VeyraProjectile.h"
#include "EngineUtils.h"
#include "Passives/VeyraMovingTargetPassive.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraVanguards.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	// Veyra.Vanguards.MovingTarget.*: Kade's passive (Roster Bible §2), from the committed tuning. An
	// ally's displacement Tracks an enemy for Kade: his reach and damage against it grow, and the
	// displacement banks toward Dead Reckoning.
	TEST_CLASS(MovingTarget, "Veyra.Vanguards")
	{
		// Fixture values: a short push, how fast, and how long a shot gets to land.
		static constexpr double Push = 100.0;
		static constexpr double PushSpeed = 1000.0;
		static constexpr double FlightSeconds = 2.0;
		static constexpr double Tolerance = 1e-3;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Body = nullptr;
		UVeyraBasicAttackComponent* Attacks = nullptr;
		UVeyraMovingTargetPassive* Passive = nullptr;

		BEFORE_EACH()
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			Body = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraPlayerState* Participant = Body->GetPlayerState<AVeyraPlayerState>();
			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(*Participant->GetAbilitySystemComponent(), FVeyraContentId::FromText(TEXT("kade")).GetValue());
			ASSERT_THAT(IsTrue(Prepared.bPrepared && Prepared.Passive));
			Participant->SetPassive(Prepared.Passive);
			Passive = Cast<UVeyraMovingTargetPassive>(Prepared.Passive);
			ASSERT_THAT(IsNotNull(Passive));
			Attacks = Participant->FindComponentByClass<UVeyraBasicAttackComponent>();
		}

		static const FVeyraMovingTargetTuning& Tuning()
		{
			return *UVeyraVanguardsTuningSubsystem::FindMovingTarget(FVeyraContentId::FromText(TEXT("kade_moving_target")).GetValue());
		}

		/** How much further Kade reaches a target he has Tracked. */
		static double TrackedReach()
		{
			return UVeyraAbilitiesTuningSubsystem::FindStatus(Tuning().TrackedStatus)->Magnitude;
		}

		bool IsTracked(const AActor& Unit) const
		{
			const UAbilitySystemComponent* Abilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
			const UVeyraStatusComponent* Marks = Abilities ? Abilities->GetOwner()->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
			return Marks && Marks->HasFrom(Tuning().TrackedStatus, *Body->GetAbilitySystemComponent());
		}

		static bool PushBy(AActor& From, AActor& Whom, double Distance)
		{
			UAbilitySystemComponent* Source = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&From);
			UAbilitySystemComponent* Target = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Whom);
			return Source && Target && VeyraCombat::Displace(*Source, *Target, FVeyraDisplacement{ FVector::RightVector, Distance, PushSpeed });
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

		TEST_METHOD(AnEnemyAnAllyDisplacesIsTrackedWithinKadesLongerReach)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			const double Range = Attacks->GetRange(nullptr);
			AVeyraVanguardCharacter& Ally = World.Spawn(EVeyraTeam::A, FVector(0.0, Range / 2.0, 0.0));
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Range + TrackedReach() / 2.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(Attacks->CheckAttack(&Enemy) == EVeyraAttackRejection::OutOfRange));
			ASSERT_THAT(IsTrue(PushBy(Ally, Enemy, Push) && IsTracked(Enemy)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Attacks->GetRange(&Enemy), Range + TrackedReach(), Tolerance), TEXT("his reach alone")));
			ASSERT_THAT(IsTrue(Attacks->CheckAttack(&Enemy) == EVeyraAttackRejection::None));

			// The enemy's own side moving its own tracks nothing.
			AVeyraVanguardCharacter& EnemyAlly = World.Spawn(EVeyraTeam::B, FVector(Range, Range, 0.0));
			AVeyraVanguardCharacter& Other = World.Spawn(EVeyraTeam::B, FVector(Range / 2.0, -Range / 2.0, 0.0));
			ASSERT_THAT(IsTrue(PushBy(EnemyAlly, Other, Push) && !IsTracked(Other)));
		}

		TEST_METHOD(AttacksOnATrackedTargetDealBonusDamage)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			const double Range = Attacks->GetRange(nullptr);
			AVeyraVanguardCharacter& Ally = World.Spawn(EVeyraTeam::A, FVector(0.0, Range / 2.0, 0.0));
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Range / 2.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(Shoot(Enemy)));
			const double Plain = Lost(Enemy);
			ASSERT_THAT(IsTrue(PushBy(Ally, Enemy, Push)));
			ASSERT_THAT(IsTrue(Shoot(Enemy)));
			const double Dealt = Lost(Enemy) - Plain;
			ASSERT_THAT(IsTrue(Dealt > Plain + Tolerance, FString::Printf(TEXT("Tracked %g, plain %g"), Dealt, Plain)));
		}

		TEST_METHOD(DeadReckoningSpendsWhatIsBankedOnATrackedTarget)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			const double Range = Attacks->GetRange(nullptr);
			const FVeyraDeadReckoningTuning& Reckoning = Tuning().DeadReckoning;
			AVeyraVanguardCharacter& Ally = World.Spawn(EVeyraTeam::A, FVector(0.0, Range / 2.0, 0.0));
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Range / 2.0, 0.0, 0.0));

			// Below the threshold, nothing is spent.
			ASSERT_THAT(IsTrue(PushBy(Ally, Enemy, Push) && FMath::IsNearlyEqual(Passive->GetBanked(), Push, Tolerance)));
			double Before = Lost(Enemy);
			ASSERT_THAT(IsTrue(Shoot(Enemy)));
			const double TrackedShot = Lost(Enemy) - Before;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Passive->GetBanked(), Push, Tolerance)));

			// Past it, the next attack on a Tracked target spends it all.
			ASSERT_THAT(IsTrue(PushBy(Ally, Enemy, Reckoning.ThresholdUnits)));
			Before = Lost(Enemy);
			ASSERT_THAT(IsTrue(Shoot(Enemy)));
			const double Reckoned = Lost(Enemy) - Before;
			ASSERT_THAT(IsTrue(Reckoned > TrackedShot + Tolerance && Passive->GetBanked() == 0.0, FString::Printf(TEXT("reckoned %g, tracked %g"), Reckoned, TrackedShot)));

			// It banks no more than its cap.
			for (int32 Pushes = 0; Pushes < 2; ++Pushes)
			{
				ASSERT_THAT(IsTrue(PushBy(Ally, Enemy, Reckoning.CapUnits)));
			}
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Passive->GetBanked(), Reckoning.CapUnits, Tolerance)));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
