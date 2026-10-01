// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Events/VeyraAbilityEvents.h"
#include "Passives/VeyraStressTemperPassive.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	// Veyra.Vanguards.StressTemper.*: a coated enemy that dashes or blinks is struck where it lands, once
	// per lockout (Roster Bible §14; ADR-032 §2), from the committed tuning.
	TEST_CLASS(StressTemper, "Veyra.Vanguards")
	{
		// Fixture values: where units stand, how far and fast they dash, and steps of time and movement.
		static constexpr double Near = 300.0;
		static constexpr double Far = 300.0;
		static constexpr double Speed = 1000.0;
		static constexpr float Step = 0.05f;
		static constexpr int32 MaxSteps = 100;
		static constexpr double Tolerance = 1e-3;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Varkesh = nullptr;
		AVeyraVanguardCharacter* Enemy = nullptr;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Varkesh = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Enemy = &World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			AVeyraPlayerState* Participant = Varkesh->GetPlayerState<AVeyraPlayerState>();
			ASSERT_THAT(IsNotNull(UVeyraVanguardsTuningSubsystem::FindStressTemper(PassiveId())));
			UVeyraStressTemperPassive* Passive = NewObject<UVeyraStressTemperPassive>(Participant);
			Passive->Start(*Participant->GetAbilitySystemComponent(), PassiveId());
			Participant->SetPassive(Passive);
		}

		static FVeyraContentId PassiveId()
		{
			return FVeyraContentId::FromText(TEXT("varkesh_stress_temper")).GetValue();
		}

		static const FVeyraStressTemperTuning& Tuning()
		{
			return *UVeyraVanguardsTuningSubsystem::FindStressTemper(PassiveId());
		}

		/** One of his abilities connects with Unit. */
		void Hit(AActor& Unit, bool bDamaging)
		{
			FVeyraAbilityHit Connects;
			Connects.Caster = Varkesh->GetAbilitySystemComponent();
			Connects.Target = &Unit;
			Connects.bDamaging = bDamaging;
			Spawner.GetWorld().GetSubsystem<UVeyraAbilityEventSubsystem>()->OnAbilityHit.Broadcast(Connects);
		}

		bool Holds(const AActor& Unit, const FVeyraContentId& Status) const
		{
			return VeyraCombat::HasStatusFrom(&Unit, Status, *Varkesh->GetAbilitySystemComponent());
		}

		void DashAway(AVeyraVanguardCharacter& Unit)
		{
			FVeyraDash Dash;
			Dash.Direction = FVector::ForwardVector;
			Dash.Distance = Far;
			Dash.Speed = Speed;
			ASSERT_THAT(IsTrue(VeyraCombat::Dash(*Unit.GetAbilitySystemComponent(), Dash)));
			UVeyraMovementComponent* Movement = Unit.GetVeyraMovement();
			for (int32 Steps = 0; Movement->IsDashing() && Steps < MaxSteps; ++Steps)
			{
				Movement->AdvanceForcedMove(Step);
			}
		}

		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, Step);
				++GFrameCounter;
				World.GetTimerManager().Tick(Step);
			}
		}

		TEST_METHOD(ACoatedEnemyThatDashesIsStruckAndRootedWhereItLands)
		{
			Hit(*Enemy, true);
			ASSERT_THAT(IsTrue(Holds(*Enemy, Tuning().Coating), TEXT("a damaging hit coats it")));
			DashAway(*Enemy);
			ASSERT_THAT(IsFalse(Holds(*Enemy, Tuning().Coating), TEXT("the strike spends the coating")));
			for (const FVeyraContentId& Status : Tuning().StrikeStatuses)
			{
				ASSERT_THAT(IsTrue(Holds(*Enemy, Status), *FString::Printf(TEXT("it holds %s"), *Status.ToString())));
			}
			ASSERT_THAT(IsTrue(Holds(*Enemy, Tuning().Lockout), TEXT("and the lockout")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(*Enemy) > 0.0, TEXT("and the strike hurts it")));
		}

		TEST_METHOD(TheLockoutSparesASecondStrike)
		{
			Hit(*Enemy, true);
			DashAway(*Enemy);
			const double Lost = FArchetypeTestWorld::HealthLost(*Enemy);
			// Long enough for the strike's statuses to wear off, not the lockout.
			double Held = 0.0;
			for (const FVeyraContentId& Status : Tuning().StrikeStatuses)
			{
				Held = FMath::Max(Held, UVeyraAbilitiesTuningSubsystem::Get().Statuses.FindChecked(Status).DurationSeconds);
			}
			Wait(Held + Step);
			ASSERT_THAT(IsTrue(Holds(*Enemy, Tuning().Lockout), TEXT("the lockout outlasts the root")));
			Hit(*Enemy, true);
			ASSERT_THAT(IsTrue(VeyraCombat::Blink(*Enemy->GetAbilitySystemComponent(), Enemy->GetActorLocation() + FVector(Far, 0.0, 0.0))));
			ASSERT_THAT(IsTrue(Holds(*Enemy, Tuning().Coating), TEXT("the coating waits out the lockout")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(*Enemy) <= Lost + Tolerance, TEXT("and no second strike lands")));
		}

		TEST_METHOD(OnlyADamagingHitOnAnEnemyVanguardCoats)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Ally = World.Spawn(EVeyraTeam::A, FVector(-Near, 0.0, 0.0));
			AVeyraTestFluxborn& Fluxborn = World.SpawnFluxborn(EVeyraTeam::B, FVector(0.0, Near, 0.0));
			Hit(*Enemy, false);
			Hit(Ally, true);
			Hit(Fluxborn, true);
			ASSERT_THAT(IsFalse(Holds(*Enemy, Tuning().Coating), TEXT("not by a hit without damage")));
			ASSERT_THAT(IsFalse(Holds(Ally, Tuning().Coating), TEXT("nor an ally")));
			ASSERT_THAT(IsFalse(Holds(Fluxborn, Tuning().Coating), TEXT("nor a unit other than a Vanguard")));
			DashAway(*Enemy);
			ASSERT_THAT(IsTrue(FMath::IsNearlyZero(FArchetypeTestWorld::HealthLost(*Enemy)), TEXT("an uncoated dash is spared")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
