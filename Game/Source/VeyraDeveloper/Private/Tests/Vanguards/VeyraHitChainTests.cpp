// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraBasicAttackComponent.h"
#include "CombatState/VeyraCombatStateComponent.h"
#include "CQTest.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraVanguards.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	// Veyra.Vanguards.HitChain.*: the generic hit-chain passive (ADR-008 §5), as Qazharr's Sea Dog runs
	// it (Character Bible §13): each attack in the chain adds to it, and the chain ending takes it
	// away. Expectations come from the committed tuning. The windup is ended by hand, as in
	// Veyra.Abilities.BasicAttack.
	TEST_CLASS(HitChain, "Veyra.Vanguards")
	{
		// Fixture value: where the enemies stand, inside a melee attack's range.
		static constexpr double Near = 100.0;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Body = nullptr;
		UVeyraBasicAttackComponent* Attacks = nullptr;

		BEFORE_EACH()
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			Body = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraPlayerState* Participant = Body->GetPlayerState<AVeyraPlayerState>();
			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(*Participant->GetAbilitySystemComponent(), FVeyraContentId::FromText(TEXT("qazharr")).GetValue());
			ASSERT_THAT(IsTrue(Prepared.bPrepared && Prepared.Passive));
			Participant->SetPassive(Prepared.Passive);
			Attacks = Participant->FindComponentByClass<UVeyraBasicAttackComponent>();
		}

		static FVeyraContentId SeaDogStatus()
		{
			return UVeyraVanguardsTuningSubsystem::FindHitChain(FVeyraContentId::FromText(TEXT("qazharr_sea_dog")).GetValue())->Status;
		}

		static int32 MaxStacks()
		{
			return UVeyraAbilitiesTuningSubsystem::FindStatus(SeaDogStatus())->MaxStacks;
		}

		/** Sea Dog's stacks on Qazharr now; 0 without it. */
		int32 Stacks() const
		{
			const FVeyraContentId Id = SeaDogStatus();
			const FVeyraStatusEntry* Entry = Body->GetPlayerState()->FindComponentByClass<UVeyraStatusComponent>()->GetLedger().Entries.FindByPredicate(
				[&Id](const FVeyraStatusEntry& Each) { return Each.Id == Id; });
			return Entry ? Entry->Stacks : 0;
		}

		/** Waits for the next attack, then attacks Target and ends the windup at once. */
		bool AttackAgain(AActor& Target)
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
			return true;
		}

		TEST_METHOD(EachAttackInTheChainAddsAStackUpToTheMaximum)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			const double BaseInterval = Attacks->GetTiming().IntervalSeconds;
			for (int32 Hit = 1; Hit <= MaxStacks() + 1; ++Hit)
			{
				ASSERT_THAT(IsTrue(AttackAgain(Enemy)));
				ASSERT_THAT(AreEqual(FMath::Min(Hit, MaxStacks()), Stacks()));
			}
			ASSERT_THAT(IsTrue(Attacks->GetTiming().IntervalSeconds < BaseInterval, TEXT("the stacks raise Attack Speed")));
		}

		TEST_METHOD(ASwitchOfTargetStartsItAgain)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& First = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			AVeyraVanguardCharacter& Second = World.Spawn(EVeyraTeam::B, FVector(0.0, Near, 0.0));
			ASSERT_THAT(IsTrue(AttackAgain(First) && AttackAgain(First)));
			ASSERT_THAT(AreEqual(2, Stacks()));
			ASSERT_THAT(IsTrue(AttackAgain(Second)));
			ASSERT_THAT(AreEqual(1, Stacks()));
		}

		TEST_METHOD(AnAttackOnAnotherUnitOrLeavingCombatEndsIt)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			AVeyraTestFluxborn& Minion = World.SpawnFluxborn(EVeyraTeam::B, FVector(-Near, 0.0, 0.0));
			ASSERT_THAT(IsTrue(AttackAgain(Enemy) && AttackAgain(Enemy)));
			ASSERT_THAT(IsTrue(AttackAgain(Minion)));
			ASSERT_THAT(AreEqual(0, Stacks()));

			ASSERT_THAT(IsTrue(AttackAgain(Enemy)));
			ASSERT_THAT(AreEqual(1, Stacks()));
			Body->GetPlayerState()->FindComponentByClass<UVeyraCombatStateComponent>()->Clear();
			ASSERT_THAT(AreEqual(0, Stacks()));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
