// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Delivery/VeyraLingeringArea.h"
#include "EngineUtils.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVisionTuningSubsystem.h"
#include "VeyraVisionSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	// Veyra.Abilities.LingeringArea.*: a delivered area that lasts, giving those inside it statuses by
	// side, and lighting its shape (ADR-018 §5).
	TEST_CLASS(LingeringArea, "Veyra.Abilities")
	{
		// Fixture values, independent of the committed Abilities.json.
		static constexpr double LingerSeconds = 2.0;
		static constexpr double PulseSeconds = 0.5;
		/** Longer than a pulse, so what it gives lasts while one stays inside. */
		static constexpr double GivenSeconds = 0.75;
		static constexpr float WorldStep = 0.1f;

		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;
		/** The corridor runs twice a Vanguard's sight along the cast's direction. */
		double Length = 0.0;
		double Width = 0.0;

		BEFORE_EACH()
		{
			Length = UVeyraVisionTuningSubsystem::Get().Sight.Vanguard * 2.0;
			Width = Length / 8.0;
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_fervor")), StatusOf(EVeyraStatusKind::AttackSpeed, 0.35, GivenSeconds));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_stride")), StatusOf(EVeyraStatusKind::MoveSpeed, 0.2, GivenSeconds));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_mire")), StatusOf(EVeyraStatusKind::Slow, 0.2, GivenSeconds));

			// Sightline: a corridor that lasts, and lights itself for its side.
			FVeyraAreaAbilityTuning Sightline;
			Sightline.Cast = InstantCast(0.0, 0.0, 0.0);
			Sightline.Origin = EVeyraAreaOrigin::Caster;
			FVeyraAreaZoneTuning& Zone = Sightline.Zones.AddDefaulted_GetRef();
			Zone.Shape.Kind = EVeyraShapeKind::Rectangle;
			Zone.Shape.Length = Length;
			Zone.Shape.Width = Width;
			FVeyraLingerTuning& Linger = Sightline.Linger.AddDefaulted_GetRef();
			Linger.DurationSeconds = LingerSeconds;
			Linger.PulseSeconds = PulseSeconds;
			Linger.CasterStatuses.Add(ArchetypeTestId(TEXT("test_fervor")));
			Linger.AllyStatuses.Add(ArchetypeTestId(TEXT("test_stride")));
			Linger.EnemyStatuses.Add(ArchetypeTestId(TEXT("test_mire")));
			Linger.Sight = EVeyraLingerSight::Ordinary;
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_sightline")), Sightline);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_sightline")))));
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
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

		EVeyraCastRejection CastAlongX() const
		{
			return FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::W, FVector(Length, 0.0, 0.0));
		}

		bool Lasts()
		{
			return TActorIterator<AVeyraLingeringArea>(&Spawner.GetWorld()) ? true : false;
		}

		TEST_METHOD(ItGivesEachSideInsideItsStatusesUntilItEnds)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Ally = World.Spawn(EVeyraTeam::A, FVector(Length / 2.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Outsider = World.Spawn(EVeyraTeam::A, FVector(Length / 2.0, Width * 2.0, 0.0));
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Length * 3.0 / 4.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(CastAlongX() == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Lasts()));
			ASSERT_THAT(IsTrue(World.Has(*Caster, TEXT("test_fervor")) && !World.Has(*Caster, TEXT("test_stride")), TEXT("its caster's own, as it lands")));
			ASSERT_THAT(IsTrue(World.Has(Ally, TEXT("test_stride")) && !World.Has(Ally, TEXT("test_fervor"))));
			ASSERT_THAT(IsTrue(World.Has(Enemy, TEXT("test_mire")) && !World.Has(Enemy, TEXT("test_stride"))));
			ASSERT_THAT(IsFalse(World.Has(Outsider, TEXT("test_stride"))));

			// One who walks in takes it at the next pulse, and what it gives lasts between pulses.
			Outsider.SetActorLocation(FVector(Length / 4.0, 0.0, 0.0));
			AdvanceWorld(PulseSeconds + WorldStep);
			ASSERT_THAT(IsTrue(World.Has(Outsider, TEXT("test_stride"))));
			AdvanceWorld(PulseSeconds);
			ASSERT_THAT(IsTrue(World.Has(Ally, TEXT("test_stride"))));

			// It ends, and what it gave runs out after.
			AdvanceWorld(LingerSeconds);
			ASSERT_THAT(IsFalse(Lasts()));
			AdvanceWorld(GivenSeconds + WorldStep);
			ASSERT_THAT(IsFalse(World.Has(Ally, TEXT("test_stride")) || World.Has(*Caster, TEXT("test_fervor"))));
		}

		TEST_METHOD(ItsShapeIsOrdinaryVisionForItsSide)
		{
			FArchetypeTestWorld World{ Spawner };
			// Beyond the caster's sight, in the corridor.
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Length * 3.0 / 4.0, 0.0, 0.0));
			UVeyraVisionSubsystem& Vision = *Spawner.GetWorld().GetSubsystem<UVeyraVisionSubsystem>();
			Vision.Start();
			ASSERT_THAT(IsFalse(Vision.IsVisibleToTeam(EVeyraTeam::A, Enemy)));
			ASSERT_THAT(IsTrue(CastAlongX() == EVeyraCastRejection::None));
			Vision.UpdateNow();
			ASSERT_THAT(IsTrue(Vision.IsVisibleToTeam(EVeyraTeam::A, Enemy)));
			AdvanceWorld(LingerSeconds + WorldStep);
			Vision.UpdateNow();
			ASSERT_THAT(IsFalse(Vision.IsVisibleToTeam(EVeyraTeam::A, Enemy), TEXT("it lights only while it lasts")));
		}

		TEST_METHOD(ValidationKeepsItToOneAndItsTimesInOrder)
		{
			constexpr int32 RankCounts[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, RankCounts).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, RankCounts), TEXT(" | "))));
			FVeyraAbilitiesTuning Broken = Tuning;
			FVeyraAreaAbilityTuning& Sightline = Broken.Area.FindChecked(ArchetypeTestId(TEXT("test_sightline")));
			Sightline.Linger[0].PulseSeconds = LingerSeconds * 2.0;
			Sightline.Linger[0].EnemyStatuses.Add(ArchetypeTestId(TEXT("no_such_status")));
			const FVeyraLingerTuning Second = Sightline.Linger[0];
			Sightline.Linger.Add(Second);
			const FString All = FString::Join(VeyraAbilityRules::Validate(Broken, RankCounts), TEXT(" | "));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/area/test_sightline/linger:")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/area/test_sightline/linger/0:")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("no_such_status")), All));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
