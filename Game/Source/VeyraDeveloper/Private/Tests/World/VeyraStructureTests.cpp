// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Components/ActorTestSpawner.h"
#include "Fluxborn/VeyraFluxborn.h"
#include "CQTest.h"
#include "Rules/VeyraStructureRules.h"
#include "Structures/VeyraStructure.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "Tests/World/VeyraBattlegroundTestLayout.h"
#include "TimerManager.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "VeyraBattlegroundSubsystem.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraWorldTests
{
	// Veyra.World.StructureRules.*: which structures may be damaged, and when a Prime Well regenerates
	// (Battleground Bible §10, §18; lane order ruled 2026-09-28).
	TEST_CLASS(StructureRules, "Veyra.World")
	{
		/** One team's structures on one lane: three Spires, the inhibitor, two base towers and the Prime Well. */
		static TArray<FVeyraStructureStatus> OneTeam()
		{
			TArray<FVeyraStructureStatus> All;
			for (int32 Order = 0; Order < 3; ++Order)
			{
				All.Add({ EVeyraStructureKind::LaneSpire, EVeyraTeam::A, EVeyraLane::Mid, Order, false });
			}
			All.Add({ EVeyraStructureKind::Inhibitor, EVeyraTeam::A, EVeyraLane::Mid, 3, false });
			All.Add({ EVeyraStructureKind::BaseTower, EVeyraTeam::A, {}, 0, false });
			All.Add({ EVeyraStructureKind::BaseTower, EVeyraTeam::A, {}, 1, false });
			All.Add({ EVeyraStructureKind::PrimeWell, EVeyraTeam::A, {}, 0, false });
			return All;
		}

		static TArray<bool> Invulnerable(const TArray<FVeyraStructureStatus>& All)
		{
			TArray<bool> Result;
			for (const FVeyraStructureStatus& Structure : All)
			{
				Result.Add(VeyraStructureRules::IsInvulnerable(Structure, All));
			}
			return Result;
		}

		TEST_METHOD(ALanesStructuresFallInOrder)
		{
			TArray<FVeyraStructureStatus> All = OneTeam();
			ASSERT_THAT(IsTrue(Invulnerable(All) == TArray<bool>({ false, true, true, true, true, true, true }), TEXT("only the outer Spire can be hit")));
			All[0].bDestroyed = true;
			ASSERT_THAT(IsTrue(Invulnerable(All)[1] == false && Invulnerable(All)[2]));
			All[1].bDestroyed = true;
			All[2].bDestroyed = true;
			ASSERT_THAT(IsFalse(Invulnerable(All)[3], TEXT("then the inhibitor")));
			ASSERT_THAT(IsTrue(Invulnerable(All)[4] && Invulnerable(All)[5] && Invulnerable(All)[6], TEXT("the base holds while every inhibitor stands")));
		}

		TEST_METHOD(AnInhibitorDownOpensTheBaseAndTheWellNeedsBothTowers)
		{
			TArray<FVeyraStructureStatus> All = OneTeam();
			for (int32 Index = 0; Index <= 3; ++Index)
			{
				All[Index].bDestroyed = true;
			}
			ASSERT_THAT(IsTrue(Invulnerable(All) == TArray<bool>({ false, false, false, false, false, false, true })));
			ASSERT_THAT(IsFalse(VeyraStructureRules::PrimeWellRegenerates(EVeyraTeam::A, All), TEXT("an inhibitor down stops the regeneration")));
			All[4].bDestroyed = true;
			ASSERT_THAT(IsTrue(Invulnerable(All)[6], TEXT("one base tower still stands")));
			All[5].bDestroyed = true;
			ASSERT_THAT(IsFalse(Invulnerable(All)[6], TEXT("both towers down and an inhibitor down: the Well can fall")));

			All[3].bDestroyed = false;
			ASSERT_THAT(IsTrue(Invulnerable(All)[6], TEXT("the last inhibitor rebuilt: invulnerable at once")));
			ASSERT_THAT(IsTrue(VeyraStructureRules::PrimeWellRegenerates(EVeyraTeam::A, All)));
			ASSERT_THAT(IsFalse(Invulnerable(All)[3], TEXT("a rebuilt inhibitor behind fallen Spires can be hit again")));
		}

		TEST_METHOD(TheSiegeTakesTheShortestWayToThePrimeWell)
		{
			TArray<FVeyraStructureStatus> All = OneTeam();
			// Another lane's outer Spire, which the way to the Well does not need.
			All.Add({ EVeyraStructureKind::LaneSpire, EVeyraTeam::A, EVeyraLane::Top, 0, false });
			TArray<int32> Order;
			while (const TOptional<int32> Next = VeyraStructureRules::NextToSiege(EVeyraTeam::A, All))
			{
				Order.Add(Next.GetValue());
				All[Next.GetValue()].bDestroyed = true;
			}
			ASSERT_THAT(IsTrue(Order == TArray<int32>({ 0, 1, 2, 3, 4, 5, 6, 7 }), TEXT("the mid lane, the base towers, the Well, then the other lanes")));
			ASSERT_THAT(IsFalse(VeyraStructureRules::NextToSiege(EVeyraTeam::B, OneTeam()).IsSet(), TEXT("team B has nothing here")));
		}

		TEST_METHOD(BackdoorProtectionClimbsAndDropsAtOnce)
		{
			// Fixture values: two thirds at most, over 5 s.
			constexpr double Max = 0.66;
			constexpr double Ramp = 5.0;
			double Protection = VeyraStructureRules::NextBackdoorProtection(0.0, false, Max, Ramp, Ramp / 2.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Protection, Max / 2.0)));
			Protection = VeyraStructureRules::NextBackdoorProtection(Protection, false, Max, Ramp, Ramp * 2.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Protection, Max), TEXT("it stops at the maximum")));
			ASSERT_THAT(IsTrue(VeyraStructureRules::NextBackdoorProtection(Protection, true, Max, Ramp, Ramp) == 0.0, TEXT("an attacking Fluxborn lifts it at once")));
			ASSERT_THAT(IsTrue(VeyraStructureRules::HasBackdoorProtection(EVeyraStructureKind::PrimeWell)
				&& !VeyraStructureRules::HasBackdoorProtection(EVeyraStructureKind::Inhibitor)));
		}

		TEST_METHOD(OneTeamsStructuresNeverGateTheOthers)
		{
			TArray<FVeyraStructureStatus> All = OneTeam();
			TArray<FVeyraStructureStatus> TeamB = OneTeam();
			for (FVeyraStructureStatus& Structure : TeamB)
			{
				Structure.Team = EVeyraTeam::B;
				Structure.bDestroyed = Structure.Kind != EVeyraStructureKind::PrimeWell;
			}
			All.Append(TeamB);
			ASSERT_THAT(IsFalse(VeyraStructureRules::IsInvulnerable(All.Last(), All), TEXT("team B's Well is open")));
			ASSERT_THAT(IsTrue(VeyraStructureRules::IsInvulnerable(All[1], All), TEXT("team A's lane is untouched")));
		}
	};

	// Veyra.World.Structures.*: the battleground subsystem spawns the layout's structures and keeps
	// them to the rules as they fall, rebuilding inhibitors and regenerating the Prime Well (ADR-011 §9).
	TEST_CLASS(Structures, "Veyra.World")
	{
		static constexpr double Lethal = 1000000.0;
		static constexpr double Tolerance = 1e-3;

		FActorTestSpawner Spawner;
		UVeyraBattlegroundSubsystem* Battleground = nullptr;
		UAbilitySystemComponent* Attacker = nullptr;
		TArray<FVeyraStructureDestroyedEvent> Destroyed;

		BEFORE_EACH()
		{
			Battleground = Spawner.GetWorld().GetSubsystem<UVeyraBattlegroundSubsystem>();
			ASSERT_THAT(IsNotNull(Battleground));
			SpawnCompactGround(Spawner.GetWorld());
			Battleground->SpawnStructures(CompactBattleground());
			Battleground->OnStructureDestroyed.AddLambda([this](const FVeyraStructureDestroyedEvent& Event) { Destroyed.Add(Event); });
			Attacker = &VeyraCombatTests::SpawnCombatant(Spawner);
			CastChecked<AVeyraPlayerState>(Attacker->GetOwner())->SetVeyraTeam(EVeyraTeam::A);
			VeyraCombat::InitializeStats(*Attacker, VeyraCombatTests::ExampleStats());
		}

		AVeyraStructure* Find(EVeyraStructureKind Kind, TOptional<EVeyraLane> Lane, int32 Order) const
		{
			return Battleground->FindStructure(EVeyraTeam::B, Kind, Lane, Order);
		}

		bool Destroy(AVeyraStructure& Structure) const
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Lethal });
			Damage.Delivery = EVeyraDamageDelivery::Developer;
			VeyraCombat::DealDamage(*Attacker, *Structure.GetAbilitySystemComponent(), Damage);
			return Structure.IsDestroyed();
		}

		TEST_METHOD(EveryStructureSpawnsWithItsStats)
		{
			// Per team: three Spires and an inhibitor, two base towers and the Prime Well.
			ASSERT_THAT(AreEqual(14, Battleground->GetStructures().Num()));
			const AVeyraStructure* Well = Find(EVeyraStructureKind::PrimeWell, {}, 0);
			ASSERT_THAT(IsNotNull(Well));
			const double MaxHealth = Well->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(MaxHealth, UVeyraWorldTuningSubsystem::Get().Structures.PrimeWell.MaxHealth, Tolerance)));
			ASSERT_THAT(IsTrue(Well->IsInvulnerable() && !Find(EVeyraStructureKind::LaneSpire, EVeyraLane::Mid, 0)->IsInvulnerable()));
		}

		TEST_METHOD(DestroyingALanesStructuresInOrderOpensTheBase)
		{
			AVeyraStructure* Outer = Find(EVeyraStructureKind::LaneSpire, EVeyraLane::Mid, 0);
			AVeyraStructure* Middle = Find(EVeyraStructureKind::LaneSpire, EVeyraLane::Mid, 1);
			ASSERT_THAT(IsFalse(Destroy(*Middle), TEXT("the middle Spire cannot fall before the outer one")));
			ASSERT_THAT(IsTrue(Destroy(*Outer) && Destroyed.Num() == 1 && Destroyed[0].Team == EVeyraTeam::B && Destroyed[0].Kind == EVeyraStructureKind::LaneSpire));
			ASSERT_THAT(IsTrue(Destroy(*Middle)));
			ASSERT_THAT(IsTrue(Destroy(*Find(EVeyraStructureKind::LaneSpire, EVeyraLane::Mid, 2))));
			AVeyraStructure* Tower = Find(EVeyraStructureKind::BaseTower, {}, 0);
			ASSERT_THAT(IsTrue(Tower->IsInvulnerable()));
			ASSERT_THAT(IsTrue(Destroy(*Find(EVeyraStructureKind::Inhibitor, EVeyraLane::Mid, 3))));
			ASSERT_THAT(IsFalse(Tower->IsInvulnerable(), TEXT("an inhibitor down opens the base towers")));
		}

		TEST_METHOD(AnInhibitorRebuildsAfterItsTimeAndClosesTheBase)
		{
			for (int32 Order = 0; Order < 3; ++Order)
			{
				ASSERT_THAT(IsTrue(Destroy(*Find(EVeyraStructureKind::LaneSpire, EVeyraLane::Mid, Order))));
			}
			AVeyraStructure* Inhibitor = Find(EVeyraStructureKind::Inhibitor, EVeyraLane::Mid, 3);
			ASSERT_THAT(IsTrue(Destroy(*Inhibitor)));
			const double RebuildSeconds = UVeyraWorldTuningSubsystem::Get().Inhibitor.RebuildSeconds;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Inhibitor->GetRebuildsAt(), Spawner.GetWorld().GetTimeSeconds() + RebuildSeconds, Tolerance)));

			// The timer manager ticks at most once per engine frame, and a test runs inside one; its
			// first tick only activates the timers set before it, so two frames pass.
			FTimerManager& Timers = Spawner.GetWorld().GetTimerManager();
			++GFrameCounter;
			Timers.Tick(0.0f);
			++GFrameCounter;
			Timers.Tick(static_cast<float>(RebuildSeconds + 1.0));
			ASSERT_THAT(IsFalse(Inhibitor->IsDestroyed(), TEXT("it reconstructs")));
			ASSERT_THAT(IsTrue(VeyraCombat::GetMissingHealth(*Inhibitor->GetAbilitySystemComponent()) == 0.0, TEXT("at full Health")));
			ASSERT_THAT(IsTrue(Find(EVeyraStructureKind::BaseTower, {}, 0)->IsInvulnerable(), TEXT("and the base towers close again")));
			ASSERT_THAT(IsFalse(Inhibitor->IsInvulnerable(), TEXT("it stands behind fallen Spires, so it can be hit again")));
		}

		TEST_METHOD(BackdoorProtectionHoldsUntilAnAttackingFluxbornArrives)
		{
			const FVeyraBackdoorTuning& Backdoor = UVeyraWorldTuningSubsystem::Get().Backdoor;
			AVeyraStructure* Outer = Find(EVeyraStructureKind::LaneSpire, EVeyraLane::Mid, 0);
			Battleground->UpdateBackdoorProtection(Backdoor.RampSeconds);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Outer->GetBackdoorProtection(), Backdoor.MaxReduction, Tolerance)));
			const double Taken = Outer->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraDefenceSet::GetIncomingDamageMultiplierAttribute());
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Taken, 1.0 - Backdoor.MaxReduction, Tolerance), TEXT("it is Combat's damage reduction")));
			ASSERT_THAT(IsTrue(Find(EVeyraStructureKind::Inhibitor, EVeyraLane::Mid, 3)->GetBackdoorProtection() == 0.0, TEXT("inhibitors have none")));

			// Team A's Fluxborn, the attackers of team B's Spire, arrive beside it.
			AVeyraFluxborn* Minion = Battleground->SpawnFluxborn(FVeyraContentId::FromText(TEXT("strider")).GetValue(), EVeyraTeam::A, EVeyraLane::Mid);
			ASSERT_THAT(IsNotNull(Minion));
			Minion->SetActorLocation(Outer->GetActorLocation() - FVector(Outer->GetSimpleCollisionRadius() * 2.0, 0.0, 0.0));
			Battleground->UpdateBackdoorProtection(Backdoor.UpdateSeconds);
			ASSERT_THAT(IsTrue(Outer->GetBackdoorProtection() == 0.0, TEXT("it drops at once")));
		}

		TEST_METHOD(ThePrimeWellRegeneratesOnlyWhileEveryInhibitorStands)
		{
			AVeyraStructure* Well = Find(EVeyraStructureKind::PrimeWell, {}, 0);
			UAbilitySystemComponent& WellUnit = *Well->GetAbilitySystemComponent();
			// Invulnerable, so its Health is set as a wound would leave it.
			const double MaxHealth = WellUnit.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
			WellUnit.SetNumericAttributeBase(UVeyraVitalsSet::GetHealthAttribute(), static_cast<float>(MaxHealth / 2.0));
			constexpr double Seconds = 2.0;
			Battleground->RegeneratePrimeWells(Seconds);
			const double Restored = MaxHealth * UVeyraWorldTuningSubsystem::Get().PrimeWell.RegenerationFractionPerSecond * Seconds;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraCombat::GetMissingHealth(WellUnit), MaxHealth / 2.0 - Restored, Tolerance)));

			for (int32 Order = 0; Order < 3; ++Order)
			{
				Destroy(*Find(EVeyraStructureKind::LaneSpire, EVeyraLane::Mid, Order));
			}
			ASSERT_THAT(IsTrue(Destroy(*Find(EVeyraStructureKind::Inhibitor, EVeyraLane::Mid, 3))));
			const double Missing = VeyraCombat::GetMissingHealth(WellUnit);
			Battleground->RegeneratePrimeWells(Seconds);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraCombat::GetMissingHealth(WellUnit), Missing, Tolerance), TEXT("an inhibitor down stops it")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
