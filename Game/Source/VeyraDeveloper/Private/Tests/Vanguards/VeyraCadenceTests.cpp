// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraBasicAttackComponent.h"
#include "CQTest.h"
#include "Delivery/VeyraProjectile.h"
#include "EngineUtils.h"
#include "Passives/VeyraCadencePassive.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraVanguards.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	// Veyra.Vanguards.Cadence.*: Vera's passive (Roster Bible §7), from the committed tuning.
	TEST_CLASS(Cadence, "Veyra.Vanguards")
	{
		// Fixture values: where the target stands, a bystander behind it, and how long shots get to land.
		static constexpr double Apart = 300.0;
		static constexpr double JustBehind = 150.0;
		static constexpr double FlightSeconds = 2.0;
		static constexpr float WorldStep = 0.05f;
		static constexpr double Tolerance = 1e-3;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Body = nullptr;
		UVeyraBasicAttackComponent* Attacks = nullptr;
		UVeyraCadencePassive* Passive = nullptr;

		BEFORE_EACH()
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			Body = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraPlayerState* Participant = Body->GetPlayerState<AVeyraPlayerState>();
			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(*Participant->GetAbilitySystemComponent(), FVeyraContentId::FromText(TEXT("vera")).GetValue());
			ASSERT_THAT(IsTrue(Prepared.bPrepared && Prepared.Passive));
			Participant->SetPassive(Prepared.Passive);
			Passive = Cast<UVeyraCadencePassive>(Prepared.Passive);
			ASSERT_THAT(IsNotNull(Passive));
			Attacks = Participant->FindComponentByClass<UVeyraBasicAttackComponent>();
		}

		static const FVeyraCadenceTuning& Tuning()
		{
			return *UVeyraVanguardsTuningSubsystem::FindCadence(FVeyraContentId::FromText(TEXT("vera_cadence")).GetValue());
		}

		static FVeyraStatusSpec Stack()
		{
			return UVeyraAbilitiesTuningSubsystem::FindStatus(Tuning().Status).GetValue();
		}

		/** Moves world time on by Seconds in small steps, timers included. */
		void AdvanceWorld(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, WorldStep);
				++GFrameCounter;
				World.GetTimerManager().Tick(WorldStep);
			}
		}

		/** Flies every shot at Target home. */
		void Land(AActor& Target)
		{
			for (TActorIterator<AVeyraProjectile> It(&Spawner.GetWorld()); It; ++It)
			{
				if (It->GetHomingTarget() == &Target)
				{
					It->AdvanceBy(FlightSeconds);
				}
			}
		}

		/** Waits for the next attack with no timer passing, attacks Target, ends the windup and lands the shot. */
		bool Shoot(AActor& Target)
		{
			UWorld& World = Spawner.GetWorld();
			while (World.GetTimeSeconds() < Attacks->GetNextAttackAt())
			{
				World.Tick(LEVELTICK_TimeOnly, WorldStep);
			}
			if (Attacks->StartAttack(Target) != EVeyraAttackRejection::None)
			{
				return false;
			}
			Attacks->Commit();
			Land(Target);
			return true;
		}

		static double Lost(const AActor& Unit)
		{
			return VeyraAbilitiesTests::FArchetypeTestWorld::HealthLost(Unit);
		}

		TEST_METHOD(EachAttackAddsAStackThatDecaysOneAtATime)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Apart, 0.0, 0.0));
			constexpr int32 Attacked = 3;
			for (int32 Shot = 0; Shot < Attacked; ++Shot)
			{
				ASSERT_THAT(IsTrue(Shoot(Enemy)));
			}
			ASSERT_THAT(AreEqual(Attacked, Passive->GetStacks()));
			AdvanceWorld(Stack().DurationSeconds + WorldStep);
			ASSERT_THAT(AreEqual(Attacked - 1, Passive->GetStacks(), TEXT("one stack goes once she stops")));
			AdvanceWorld(Stack().StackDecaySeconds + WorldStep);
			ASSERT_THAT(AreEqual(Attacked - 2, Passive->GetStacks()));
		}

		TEST_METHOD(DugInItDecaysMoreSlowly)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Apart, 0.0, 0.0));
			UAbilitySystemComponent& Vera = *Body->GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Vera, Vera, UVeyraAbilitiesTuningSubsystem::FindStatus(Tuning().SteadyStatus).GetValue())));
			ASSERT_THAT(IsTrue(Shoot(Enemy) && Shoot(Enemy)));
			AdvanceWorld(Stack().DurationSeconds + WorldStep);
			ASSERT_THAT(AreEqual(1, Passive->GetStacks()));
			AdvanceWorld(Stack().StackDecaySeconds + WorldStep);
			ASSERT_THAT(AreEqual(1, Passive->GetStacks(), TEXT("a stack lasts longer dug in")));
			AdvanceWorld(Stack().StackDecaySeconds * (Tuning().SteadyDecayMultiplier - 1.0) + WorldStep);
			ASSERT_THAT(AreEqual(0, Passive->GetStacks()));
		}

		TEST_METHOD(AnAttackOnARangedTargetAddsASecondStack)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Apart, 0.0, 0.0));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Body->GetAbilitySystemComponent(), *Enemy.GetAbilitySystemComponent(),
				UVeyraAbilitiesTuningSubsystem::FindStatus(Tuning().ExtraStackOn).GetValue())));
			ASSERT_THAT(IsTrue(Shoot(Enemy)));
			ASSERT_THAT(AreEqual(2, Passive->GetStacks()));
		}

		TEST_METHOD(AtFullCadenceAnEchoRepeatsEachAttackAndAddsNoStack)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Apart, 0.0, 0.0));
			double Before = Lost(Enemy);
			ASSERT_THAT(IsTrue(Shoot(Enemy)));
			const double Plain = Lost(Enemy) - Before;
			AdvanceWorld(Tuning().FiringLine.DelaySeconds + WorldStep);
			Land(Enemy);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Lost(Enemy) - Before, Plain, Tolerance), TEXT("no echo below full Cadence")));
			while (!Passive->IsFull())
			{
				ASSERT_THAT(IsTrue(Shoot(Enemy)));
			}
			const int32 Full = Passive->GetStacks();
			Before = Lost(Enemy);
			ASSERT_THAT(IsTrue(Shoot(Enemy)));
			const double Attack = Lost(Enemy) - Before;
			AdvanceWorld(Tuning().FiringLine.DelaySeconds + WorldStep);
			Land(Enemy);
			ASSERT_THAT(IsTrue(Lost(Enemy) - Before > Attack + Tolerance, TEXT("the echo lands after it")));
			ASSERT_THAT(AreEqual(Full, Passive->GetStacks(), TEXT("an echo is no attack")));
		}

		TEST_METHOD(TheLastVolleyHoldsCadenceFullAndFiresEveryThirdAttackThroughTheTarget)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Apart, 0.0, 0.0));
			AVeyraVanguardCharacter& Behind = World.Spawn(EVeyraTeam::B, FVector(Apart + JustBehind, 0.0, 0.0));
			UAbilitySystemComponent& Vera = *Body->GetAbilitySystemComponent();
			const FVeyraStatusSpec Volley = UVeyraAbilitiesTuningSubsystem::FindStatus(Tuning().FullStatus).GetValue();
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Vera, Vera, Volley)));
			ASSERT_THAT(IsTrue(Passive->IsFull() && Passive->GetStacks() == Stack().MaxStacks, TEXT("full at once")));
			AdvanceWorld(Stack().DurationSeconds + Stack().StackDecaySeconds);
			ASSERT_THAT(AreEqual(Stack().MaxStacks, Passive->GetStacks(), TEXT("and it cannot fall")));

			for (int32 Attack = 1; Attack <= Tuning().SpectralRank.EveryAttacks; ++Attack)
			{
				const double Before = Lost(Behind);
				ASSERT_THAT(IsTrue(Shoot(Enemy)));
				const bool bRank = Attack == Tuning().SpectralRank.EveryAttacks;
				ASSERT_THAT(IsTrue((Lost(Behind) > Before) == bRank, FString::Printf(TEXT("attack %d: the rank through the target"), Attack)));
			}
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
