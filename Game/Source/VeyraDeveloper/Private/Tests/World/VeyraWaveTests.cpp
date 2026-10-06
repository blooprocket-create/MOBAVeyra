// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Components/ActorTestSpawner.h"
#include "CQTest.h"
#include "Fluxborn/VeyraFluxborn.h"
#include "Rules/VeyraWaveRules.h"
#include "Structures/VeyraStructure.h"
#include "Tests/World/VeyraBattlegroundTestLayout.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "VeyraBattlegroundSubsystem.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraWorldTests
{
	namespace
	{
		FVeyraContentId WaveId(const TCHAR* Text)
		{
			return FVeyraContentId::FromText(Text).GetValue();
		}

		/**
		 * A schedule of the bible's shape (Battleground Bible §17): first at 30 s, every 30 s, every
		 * 25 s from 840 s and every 20 s from 1800 s; siege units every 3rd wave, every 2nd from
		 * 1800 s. Fixture values, independent of the committed World.json.
		 */
		FVeyraWavePhaseTuning Phase(double FromSeconds, double IntervalSeconds, int32 SiegeEveryWaves)
		{
			FVeyraWavePhaseTuning Phase;
			Phase.FromSeconds = FromSeconds;
			Phase.IntervalSeconds = IntervalSeconds;
			Phase.SiegeEveryWaves = SiegeEveryWaves;
			return Phase;
		}

		FVeyraWaveUnitTuning Units(const TCHAR* Unit, int32 Count)
		{
			FVeyraWaveUnitTuning Units;
			Units.Unit = WaveId(Unit);
			Units.Count = Count;
			return Units;
		}

		FVeyraWavesTuning ExampleWaves()
		{
			FVeyraWavesTuning Waves;
			Waves.FirstWaveSeconds = 30.0;
			Waves.Phases = { Phase(0.0, 30.0, 3), Phase(840.0, 25.0, 3), Phase(1800.0, 20.0, 2) };
			Waves.Units = { Units(TEXT("strider"), 3), Units(TEXT("spark"), 3) };
			Waves.SiegeUnits = { Units(TEXT("breaker"), 1) };
			Waves.InhibitorDownUnits = { Units(TEXT("breaker"), 1) };
			return Waves;
		}
	}

	// Veyra.World.WaveSchedule.*: when waves spawn and what they bring (Battleground Bible §17, §18).
	TEST_CLASS(WaveSchedule, "Veyra.World")
	{
		static constexpr double Tolerance = 1e-6;

		// Fixture value: 45 minutes of match clock.
		static constexpr double MatchSeconds = 45.0 * 60.0;

		TEST_METHOD(CrossingAPhaseNeitherDuplicatesNorSkipsAWave)
		{
			const FVeyraWavesTuning Waves = ExampleWaves();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraWaveRules::WaveTime(Waves, 0), 30.0, Tolerance)));
			// 30 + 27 × 30 = 840: that wave spawns in the second phase, so the next follows 25 s later.
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraWaveRules::WaveTime(Waves, 26), 810.0, Tolerance)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraWaveRules::WaveTime(Waves, 27), 840.0, Tolerance)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraWaveRules::WaveTime(Waves, 28), 865.0, Tolerance)));
			// 840 + 38 × 25 = 1790 spawns before 1800, so 1815 follows it; then every 20 s.
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraWaveRules::WaveTime(Waves, 65), 1790.0, Tolerance)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraWaveRules::WaveTime(Waves, 66), 1815.0, Tolerance)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraWaveRules::WaveTime(Waves, 67), 1835.0, Tolerance)));
		}

		TEST_METHOD(EveryGapIsItsPhasesInterval)
		{
			const FVeyraWavesTuning Waves = ExampleWaves();
			double Previous = VeyraWaveRules::WaveTime(Waves, 0);
			for (int32 Index = 1; Previous < MatchSeconds; ++Index)
			{
				const double Next = VeyraWaveRules::WaveTime(Waves, Index);
				ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Next - Previous, VeyraWaveRules::IntervalAt(Waves, Previous), Tolerance)));
				Previous = Next;
			}
		}

		TEST_METHOD(SiegeUnitsComeEveryThirdWaveThenEverySecond)
		{
			const FVeyraWavesTuning Waves = ExampleWaves();
			const auto Siege = [&Waves](int32 Index) { return VeyraWaveRules::HasSiege(Waves, Index, VeyraWaveRules::WaveTime(Waves, Index)); };
			ASSERT_THAT(IsTrue(!Siege(0) && !Siege(1) && Siege(2) && !Siege(3) && Siege(5)));
			// From wave 67 (index 66), at 1815 s, the last phase brings them every second wave.
			ASSERT_THAT(IsTrue(!Siege(66) && Siege(67) && !Siege(68) && Siege(69)));
		}

		TEST_METHOD(AWaveWalksOutFrontLineFirst)
		{
			const FVeyraWavesTuning Waves = ExampleWaves();
			const FVeyraContentId Strider = WaveId(TEXT("strider"));
			const FVeyraContentId Spark = WaveId(TEXT("spark"));
			const FVeyraContentId Breaker = WaveId(TEXT("breaker"));
			ASSERT_THAT(IsTrue(VeyraWaveRules::Composition(Waves, false, false) == TArray<FVeyraContentId>({ Strider, Strider, Strider, Spark, Spark, Spark })));
			ASSERT_THAT(IsTrue(VeyraWaveRules::Composition(Waves, /*bSiege*/ true, /*bInhibitorDown*/ true)
				== TArray<FVeyraContentId>({ Breaker, Strider, Strider, Strider, Breaker, Spark, Spark, Spark })));
		}
	};

	// Veyra.World.Waves.*: the battleground spawns each wave in every lane for both teams, and a lane
	// whose enemy inhibitor is down adds to its waves (Battleground Bible §17, §18).
	TEST_CLASS(Waves, "Veyra.World")
	{
		FActorTestSpawner Spawner;
		TUniquePtr<FScopedWorldTuning> Tuning;
		UVeyraBattlegroundSubsystem* Battleground = nullptr;

		BEFORE_EACH()
		{
			Tuning = MakeUnique<FScopedWorldTuning>();
			Tuning->Tuning.Waves = ExampleWaves();
			// Every unit at once, so the test needs no timers.
			Tuning->Tuning.Waves.UnitIntervalSeconds = 0.0;
			Battleground = Spawner.GetWorld().GetSubsystem<UVeyraBattlegroundSubsystem>();
			ASSERT_THAT(IsNotNull(Battleground));
			SpawnCompactGround(Spawner.GetWorld());
			Battleground->SpawnStructures(CompactBattleground());
		}

		AFTER_EACH()
		{
			Tuning.Reset();
		}

		int32 CountOf(EVeyraTeam Team) const
		{
			return Battleground->GetFluxborn().FilterByPredicate([Team](const AVeyraFluxborn* Unit) { return Unit->GetVeyraTeam() == Team; }).Num();
		}

		TEST_METHOD(AWaveSpawnsInEveryLaneForBothTeams)
		{
			Battleground->SpawnWave(0);
			// The compact battleground has one lane: three Striders and three Sparks a side.
			ASSERT_THAT(AreEqual(6, CountOf(EVeyraTeam::A)));
			ASSERT_THAT(AreEqual(6, CountOf(EVeyraTeam::B)));
			ASSERT_THAT(AreEqual(1, Battleground->GetWavesSpawned()));
			Battleground->SpawnWave(2);
			ASSERT_THAT(AreEqual(6 + 7, CountOf(EVeyraTeam::A), TEXT("the third wave brings a Breaker")));
		}

		TEST_METHOD(ADownedInhibitorAddsToTheOtherSidesWaves)
		{
			// Team A's lane falls in order down to its inhibitor.
			UAbilitySystemComponent& Source = *Battleground->FindStructure(EVeyraTeam::B, EVeyraStructureKind::PrimeWell, {}, 0)->GetAbilitySystemComponent();
			for (int32 Order = 0; Order <= 3; ++Order)
			{
				const EVeyraStructureKind Kind = Order < 3 ? EVeyraStructureKind::LaneSpire : EVeyraStructureKind::Inhibitor;
				UAbilitySystemComponent& Structure = *Battleground->FindStructure(EVeyraTeam::A, Kind, EVeyraLane::Mid, Order)->GetAbilitySystemComponent();
				FVeyraRawDamageEvent Lethal;
				Lethal.Components.Add({ EVeyraDamageType::TrueDamage, Structure.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) });
				Lethal.Delivery = EVeyraDamageDelivery::Developer;
				VeyraCombat::DealDamage(Source, Structure, Lethal);
			}
			ASSERT_THAT(IsTrue(Battleground->IsInhibitorDown(EVeyraTeam::A, EVeyraLane::Mid)));
			Battleground->SpawnWave(0);
			ASSERT_THAT(AreEqual(7, CountOf(EVeyraTeam::B), TEXT("team B's wave gains its extra Breaker")));
			ASSERT_THAT(AreEqual(6, CountOf(EVeyraTeam::A)));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
