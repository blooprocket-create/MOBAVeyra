// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Components/ActorTestSpawner.h"
#include "CQTest.h"
#include "Delivery/VeyraProjectile.h"
#include "EngineUtils.h"
#include "Rules/VeyraTowerRules.h"
#include "Structures/VeyraStructure.h"
#include "Structures/VeyraStructureAttackComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tests/World/VeyraBattlegroundTestLayout.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "VeyraBattlegroundSubsystem.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraWorldTests
{
	// Veyra.World.TowerTargeting.*: whom a lane Spire or base-defense tower shoots (Combat Bible §33).
	TEST_CLASS(TowerTargeting, "Veyra.World")
	{
		FActorTestSpawner Spawner;

		// Fixture distances, edge to edge.
		static constexpr double Near = 100.0;
		static constexpr double Far = 400.0;

		const AActor* Unit()
		{
			return &Spawner.SpawnActor<AActor>();
		}

		static FVeyraTowerCandidate Fluxborn(const AActor* Unit, double Distance, uint32 Id = 0)
		{
			return { Unit, false, Distance, Id };
		}

		static FVeyraTowerCandidate Vanguard(const AActor* Unit, double Distance, uint32 Id = 0)
		{
			return { Unit, true, Distance, Id };
		}

		TEST_METHOD(FluxbornComeBeforeANearerVanguard)
		{
			const AActor* Minion = Unit();
			const AActor* Champion = Unit();
			const FVeyraTowerChoice Choice = VeyraTowerRules::Choose(nullptr, false, nullptr, { Vanguard(Champion, Near), Fluxborn(Minion, Far) });
			ASSERT_THAT(IsTrue(Choice.Target == Minion && !Choice.bPriority));
		}

		TEST_METHOD(WithNoFluxbornTheNearestVanguard)
		{
			const AActor* Nearer = Unit();
			const AActor* Farther = Unit();
			const FVeyraTowerChoice Choice = VeyraTowerRules::Choose(nullptr, false, nullptr, { Vanguard(Farther, Far), Vanguard(Nearer, Near) });
			ASSERT_THAT(IsTrue(Choice.Target == Nearer && !Choice.bPriority, TEXT("normal targeting, not priority")));
		}

		TEST_METHOD(ItKeepsAValidTarget)
		{
			const AActor* Current = Unit();
			const AActor* Nearer = Unit();
			const FVeyraTowerChoice Choice = VeyraTowerRules::Choose(Current, false, nullptr, { Fluxborn(Current, Far), Fluxborn(Nearer, Near) });
			ASSERT_THAT(IsTrue(Choice.Target == Current));
		}

		TEST_METHOD(AnAttackerOfADefenderTakesPriorityOverFluxborn)
		{
			const AActor* Minion = Unit();
			const AActor* Attacker = Unit();
			const FVeyraTowerChoice Choice = VeyraTowerRules::Choose(Minion, false, Attacker, { Fluxborn(Minion, Near), Vanguard(Attacker, Far) });
			ASSERT_THAT(IsTrue(Choice.Target == Attacker && Choice.bPriority));
		}

		TEST_METHOD(ASecondAttackerDoesNotStealPriority)
		{
			const AActor* First = Unit();
			const AActor* Second = Unit();
			const FVeyraTowerChoice Choice = VeyraTowerRules::Choose(First, true, Second, { Vanguard(First, Far), Vanguard(Second, Near) });
			ASSERT_THAT(IsTrue(Choice.Target == First && Choice.bPriority));
		}

		TEST_METHOD(PriorityEndsWhenItsTargetLeavesRange)
		{
			const AActor* Gone = Unit();
			const AActor* Minion = Unit();
			const FVeyraTowerChoice Choice = VeyraTowerRules::Choose(Gone, true, nullptr, { Fluxborn(Minion, Far) });
			ASSERT_THAT(IsTrue(Choice.Target == Minion && !Choice.bPriority));
			const FVeyraTowerChoice Back = VeyraTowerRules::Choose(Minion, false, nullptr, { Fluxborn(Minion, Far), Vanguard(Gone, Near) });
			ASSERT_THAT(IsTrue(Back.Target == Minion && !Back.bPriority, TEXT("re-entering range restores nothing without a new attack")));
		}

		TEST_METHOD(AnAttackerOutOfRangeClaimsNothing)
		{
			const AActor* Minion = Unit();
			const AActor* Attacker = Unit();
			const FVeyraTowerChoice Choice = VeyraTowerRules::Choose(Minion, false, Attacker, { Fluxborn(Minion, Near) });
			ASSERT_THAT(IsTrue(Choice.Target == Minion && !Choice.bPriority));
		}

		TEST_METHOD(EquallyNearUnitsResolveByTheirStableIds)
		{
			const AActor* Low = Unit();
			const AActor* High = Unit();
			const FVeyraTowerChoice Choice = VeyraTowerRules::Choose(nullptr, false, nullptr, { Fluxborn(High, Near, 2), Fluxborn(Low, Near, 1) });
			ASSERT_THAT(IsTrue(Choice.Target == Low));
		}

		TEST_METHOD(NothingInRangeNothingChosen)
		{
			const FVeyraTowerChoice Choice = VeyraTowerRules::Choose(Unit(), true, Unit(), {});
			ASSERT_THAT(IsTrue(Choice.Target == nullptr && !Choice.bPriority));
		}
	};

	// Veyra.World.TowerRamp.*: consecutive shots at the same Vanguard escalate (Combat Bible §33).
	TEST_CLASS(TowerRamp, "Veyra.World")
	{
		FActorTestSpawner Spawner;

		// Fixture values, not tuning.
		static constexpr int32 MaxStacks = 5;
		static constexpr double PerShot = 0.2;

		TEST_METHOD(ShotsAtTheSameVanguardStackUpToTheCap)
		{
			const AActor* Vanguard = &Spawner.SpawnActor<AActor>();
			const AActor* Last = nullptr;
			int32 Stacks = 0;
			TArray<int32> Seen;
			for (int32 Shot = 0; Shot < MaxStacks + 2; ++Shot)
			{
				Stacks = VeyraTowerRules::NextRampStacks(Last, Stacks, Vanguard, /*bTargetIsVanguard*/ true, MaxStacks);
				Last = Vanguard;
				Seen.Add(Stacks);
			}
			ASSERT_THAT(IsTrue(Seen == TArray<int32>({ 0, 1, 2, 3, 4, 5, 5 })));
		}

		TEST_METHOD(ASwitchOrAFluxbornResetsIt)
		{
			const AActor* First = &Spawner.SpawnActor<AActor>();
			const AActor* Second = &Spawner.SpawnActor<AActor>();
			ASSERT_THAT(AreEqual(0, VeyraTowerRules::NextRampStacks(First, 3, Second, true, MaxStacks)));
			ASSERT_THAT(AreEqual(0, VeyraTowerRules::NextRampStacks(First, 3, First, /*bTargetIsVanguard*/ false, MaxStacks)));
		}

		TEST_METHOD(EachStackAddsItsShare)
		{
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraTowerRules::RampMultiplier(0, PerShot), 1.0)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraTowerRules::RampMultiplier(3, PerShot), 1.6)));
		}
	};

	// Veyra.World.TowerAttacks.*: the battleground's towers shoot homing shots at the target the rules
	// pick, ramping on a Vanguard, and a Vanguard who hurts a defender draws their priority (§33, §55).
	TEST_CLASS(TowerAttacks, "Veyra.World")
	{
		static constexpr double Tolerance = 1e-3;

		// Fixture value: a hit on a defender, to draw aggression.
		static constexpr double Hit = 10.0;

		FActorTestSpawner Spawner;
		UVeyraBattlegroundSubsystem* Battleground = nullptr;
		AVeyraStructure* Spire = nullptr;
		UVeyraStructureAttackComponent* Attack = nullptr;

		BEFORE_EACH()
		{
			Battleground = Spawner.GetWorld().GetSubsystem<UVeyraBattlegroundSubsystem>();
			ASSERT_THAT(IsNotNull(Battleground));
			SpawnCompactGround(Spawner.GetWorld());
			Battleground->SpawnStructures(CompactBattleground());
			Spire = Battleground->FindStructure(EVeyraTeam::B, EVeyraStructureKind::LaneSpire, EVeyraLane::Mid, 0);
			ASSERT_THAT(IsNotNull(Spire));
			Attack = Spire->GetAttack();
			ASSERT_THAT(IsNotNull(Attack));
		}

		/** A point on the ground in the Spire's range, Along its range toward Team A's side. */
		FVector InRange(double Along = 0.5, double Across = 0.0) const
		{
			const double Reach = Spire->GetSimpleCollisionRadius() + UVeyraWorldTuningSubsystem::Get().TowerAttack.Range * Along;
			const FVector Centre = Spire->GetActorLocation();
			return FVector(Centre.X - Reach, Centre.Y + Across, 0.0);
		}

		/** Lets at least Seconds of world time pass; no timer or actor ticks. */
		void Wait(double Seconds)
		{
			// Fixture value: a step below the longest frame the world accepts in one tick.
			constexpr float StepSeconds = 0.1f;
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, StepSeconds);
			}
		}

		/** Lands every shot in flight. */
		void LandShots()
		{
			// Fixture value: longer than any shot's flight here.
			constexpr double LongSeconds = 10.0;
			for (TActorIterator<AVeyraProjectile> It(&Spawner.GetWorld()); It; ++It)
			{
				if (!It->IsActorBeingDestroyed())
				{
					It->AdvanceBy(LongSeconds);
				}
			}
		}

		TEST_METHOD(OnlySpiresAndBaseTowersShoot)
		{
			ASSERT_THAT(IsNotNull(Battleground->FindStructure(EVeyraTeam::B, EVeyraStructureKind::BaseTower, {}, 0)->GetAttack()));
			ASSERT_THAT(IsNull(Battleground->FindStructure(EVeyraTeam::B, EVeyraStructureKind::Inhibitor, EVeyraLane::Mid, 3)->GetAttack()));
			ASSERT_THAT(IsNull(Battleground->FindStructure(EVeyraTeam::B, EVeyraStructureKind::PrimeWell, {}, 0)->GetAttack()));
		}

		TEST_METHOD(ItShootsAFluxbornBeforeAVanguard)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Vanguard = World.Spawn(EVeyraTeam::A, InRange(0.2));
			AVeyraTestFluxborn& Minion = World.SpawnFluxborn(EVeyraTeam::A, InRange(0.6, 200.0));
			World.SpawnFluxborn(EVeyraTeam::B, InRange(0.4, -200.0));
			Attack->Think();
			ASSERT_THAT(IsTrue(Attack->GetTarget() == &Minion && !Attack->HasPriority()));
			LandShots();
			ASSERT_THAT(IsTrue(World.HealthLost(Minion) > 0.0 && World.HealthLost(Vanguard) == 0.0));
		}

		TEST_METHOD(ConsecutiveShotsRampOnTheSameVanguard)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Vanguard = World.Spawn(EVeyraTeam::A, InRange());
			const FVeyraWorldTuning& Tuning = UVeyraWorldTuningSubsystem::Get();

			Attack->Think();
			LandShots();
			const double First = World.HealthLost(Vanguard);
			ASSERT_THAT(IsTrue(First > 0.0 && Attack->GetRampStacks() == 0));

			Attack->Think();
			LandShots();
			ASSERT_THAT(IsTrue(World.HealthLost(Vanguard) == First, TEXT("nothing more before its interval")));

			Wait(Tuning.TowerAttack.IntervalSeconds);
			Attack->Think();
			LandShots();
			const double Second = World.HealthLost(Vanguard) - First;
			ASSERT_THAT(IsTrue(Attack->GetRampStacks() == 1 && FMath::IsNearlyEqual(Second, First * (1.0 + Tuning.TowerRamp.PerShot), Tolerance)));

			// Out of range: the target and its ramp are lost.
			Vanguard.SetActorLocation(InRange(3.0));
			Wait(Tuning.TowerAttack.IntervalSeconds);
			Attack->Think();
			ASSERT_THAT(IsTrue(Attack->GetTarget() == nullptr && Attack->GetRampStacks() == 0));
		}

		TEST_METHOD(AVanguardWhoHurtsADefenderDrawsPriority)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraTestFluxborn& Minion = World.SpawnFluxborn(EVeyraTeam::A, InRange(0.2));
			AVeyraVanguardCharacter& Attacker = World.Spawn(EVeyraTeam::A, InRange(0.8, 200.0));
			AVeyraVanguardCharacter& Defender = World.Spawn(EVeyraTeam::B, InRange(0.5, -200.0));
			Attack->Think();
			ASSERT_THAT(IsTrue(Attack->GetTarget() == &Minion));

			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Hit });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Attacker.GetAbilitySystemComponent(), *Defender.GetAbilitySystemComponent(), Damage)));
			Attack->Think();
			ASSERT_THAT(IsTrue(Attack->GetTarget() == &Attacker && Attack->HasPriority()));

			// Hurting a defender beyond the tower's range draws nothing.
			AVeyraStructure* Other = Battleground->FindStructure(EVeyraTeam::B, EVeyraStructureKind::BaseTower, {}, 0);
			Other->GetAttack()->Think();
			ASSERT_THAT(IsFalse(Other->GetAttack()->HasPriority()));
		}

		TEST_METHOD(ADestroyedTowerStopsShooting)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::A, InRange());
			FVeyraRawDamageEvent Lethal;
			Lethal.Components.Add({ EVeyraDamageType::TrueDamage, Spire->GetTuning().MaxHealth * 2.0 });
			Lethal.Delivery = EVeyraDamageDelivery::Developer;
			VeyraCombat::DealDamage(*Enemy.GetAbilitySystemComponent(), *Spire->GetAbilitySystemComponent(), Lethal);
			ASSERT_THAT(IsTrue(Spire->IsDestroyed()));
			Attack->Think();
			ASSERT_THAT(IsTrue(Attack->GetTarget() == nullptr));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
