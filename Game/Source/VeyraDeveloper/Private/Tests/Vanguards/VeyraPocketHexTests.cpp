// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraBasicAttackComponent.h"
#include "CQTest.h"
#include "Delivery/VeyraProjectile.h"
#include "EngineUtils.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tests/Abilities/VeyraTestFluxborn.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraVanguards.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	// Veyra.Vanguards.PocketHex.*: Mimzi's passive, the shared markProc (Roster Bible §21), from the
	// committed tuning.
	TEST_CLASS(PocketHex, "Veyra.Vanguards")
	{
		// Fixture values: where targets stand, and how long shots get to land.
		static constexpr double Apart = 300.0;
		static constexpr double FlightSeconds = 2.0;
		static constexpr float WorldStep = 0.05f;
		static constexpr double Tolerance = 1e-3;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Body = nullptr;
		UVeyraBasicAttackComponent* Attacks = nullptr;

		BEFORE_EACH()
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			Body = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraPlayerState* Participant = Body->GetPlayerState<AVeyraPlayerState>();
			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(*Participant->GetAbilitySystemComponent(), FVeyraContentId::FromText(TEXT("mimzi")).GetValue());
			ASSERT_THAT(IsTrue(Prepared.bPrepared && Prepared.Passive));
			Participant->SetPassive(Prepared.Passive);
			Attacks = Participant->FindComponentByClass<UVeyraBasicAttackComponent>();
		}

		static const FVeyraMarkProcTuning& Tuning()
		{
			return *UVeyraVanguardsTuningSubsystem::FindMarkProc(FVeyraContentId::FromText(TEXT("mimzi_pocket_hex")).GetValue());
		}

		static FVeyraStatusSpec Hex()
		{
			return UVeyraAbilitiesTuningSubsystem::FindStatus(Tuning().Mark).GetValue();
		}

		int32 Stacks(const AActor& Unit) const
		{
			const UAbilitySystemComponent* Abilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
			const UVeyraStatusComponent* Marks = Abilities ? Abilities->GetOwner()->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
			return Marks ? Marks->GetStacksFrom(Tuning().Mark, *Body->GetAbilitySystemComponent()) : 0;
		}

		void Prime(AActor& Unit)
		{
			UAbilitySystemComponent* Target = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
			for (int32 Stack = 0; Stack < Hex().MaxStacks; ++Stack)
			{
				ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Body->GetAbilitySystemComponent(), *Target, Hex())));
			}
		}

		void Land()
		{
			for (TActorIterator<AVeyraProjectile> It(&Spawner.GetWorld()); It; ++It)
			{
				It->AdvanceBy(FlightSeconds);
			}
		}

		/** Waits for the next attack with no timer passing, attacks Target, ends the windup and lands every shot. */
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
			Land();
			return true;
		}

		static double Lost(const AActor& Unit)
		{
			return VeyraAbilitiesTests::FArchetypeTestWorld::HealthLost(Unit);
		}

		TEST_METHOD(TwoAttacksPrimeAndTheThirdSpendsTheHexForItsProc)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Apart, 0.0, 0.0));
			ASSERT_THAT(IsTrue(Shoot(Enemy)));
			const double Plain = Lost(Enemy);
			ASSERT_THAT(AreEqual(1, Stacks(Enemy)));
			ASSERT_THAT(IsTrue(Shoot(Enemy)));
			ASSERT_THAT(AreEqual(Hex().MaxStacks, Stacks(Enemy)));
			const double Before = Lost(Enemy);
			ASSERT_THAT(IsTrue(Shoot(Enemy)));
			ASSERT_THAT(IsTrue(Lost(Enemy) - Before > Plain + Tolerance, TEXT("the proc adds to the attack")));
			ASSERT_THAT(AreEqual(0, Stacks(Enemy), TEXT("spent, and the proc's attack adds no stack")));
		}

		TEST_METHOD(OnlyAnEnemyVanguardIsMarked)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraTestFluxborn& Minion = World.SpawnFluxborn(EVeyraTeam::B, FVector(Apart, 0.0, 0.0));
			ASSERT_THAT(IsTrue(Shoot(Minion)));
			ASSERT_THAT(AreEqual(0, Stacks(Minion)));
		}

		TEST_METHOD(TheFirstAttackOutOfCamouflagePrimesAnUnprimedTargetWithBonusDamage)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Apart, 0.0, 0.0));
			AVeyraVanguardCharacter& Other = World.Spawn(EVeyraTeam::B, FVector(0.0, Apart, 0.0));
			ASSERT_THAT(IsTrue(Shoot(Other)));
			const double Plain = Lost(Other);

			UAbilitySystemComponent& Mimzi = *Body->GetAbilitySystemComponent();
			const FVeyraStatusSpec Veil = UVeyraAbilitiesTuningSubsystem::FindStatus(Tuning().Emergence[0].Status).GetValue();
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Mimzi, Mimzi, Veil)));
			ASSERT_THAT(IsTrue(Shoot(Enemy)));
			ASSERT_THAT(IsTrue(Stacks(Enemy) == Hex().MaxStacks && Lost(Enemy) > Plain + Tolerance, TEXT("primed at once, with its bonus")));
			const double Before = Lost(Other);
			ASSERT_THAT(IsTrue(Shoot(Other) && Stacks(Other) == 2 && FMath::IsNearlyEqual(Lost(Other) - Before, Plain, Tolerance), TEXT("only the first: an ordinary stack on its earlier one")));
		}

		TEST_METHOD(OutOfCamouflageOnAPrimedTargetItProcsOnceAndDoesNotReprime)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Apart, 0.0, 0.0));
			Prime(Enemy);
			UAbilitySystemComponent& Mimzi = *Body->GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Mimzi, Mimzi, UVeyraAbilitiesTuningSubsystem::FindStatus(Tuning().Emergence[0].Status).GetValue())));
			ASSERT_THAT(IsTrue(Shoot(Enemy)));
			ASSERT_THAT(AreEqual(0, Stacks(Enemy)));
		}

		TEST_METHOD(AfterGrandPrankEachProcSendsABoltThatMarksNothing)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Apart, 0.0, 0.0));
			AVeyraVanguardCharacter& Nearby = World.Spawn(EVeyraTeam::B, FVector(Apart, Tuning().ProcBolts[0].Radius / 2.0, 0.0));
			// Without the window, a proc sends nothing.
			Prime(Enemy);
			ASSERT_THAT(IsTrue(Shoot(Enemy) && Lost(Nearby) == 0.0));

			Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnCastCommitted.Broadcast(
				FVeyraCastEvent{ Body->GetAbilitySystemComponent(), Tuning().ProcBolts[0].Ability, true });
			Prime(Enemy);
			ASSERT_THAT(IsTrue(Shoot(Enemy)));
			ASSERT_THAT(IsTrue(Lost(Nearby) > 0.0, TEXT("a bolt at the nearby enemy Vanguard")));
			ASSERT_THAT(AreEqual(0, Stacks(Nearby), TEXT("and it marks nothing")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
