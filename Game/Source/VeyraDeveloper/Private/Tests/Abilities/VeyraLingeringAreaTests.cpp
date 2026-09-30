// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Delivery/VeyraAreaDelivery.h"
#include "Delivery/VeyraDelayedArea.h"
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
	// side, and lighting its shape (ADR-018 §5); hitting the enemies inside at its pulses and its end,
	// and hastening its caster's delayed areas inside it (ADR-026 §4).
	TEST_CLASS(LingeringArea, "Veyra.Abilities")
	{
		// Fixture values, independent of the committed Abilities.json.
		static constexpr double LingerSeconds = 2.0;
		static constexpr double PulseSeconds = 0.5;
		/** Longer than a pulse, so what it gives lasts while one stays inside. */
		static constexpr double GivenSeconds = 0.75;
		static constexpr float WorldStep = 0.1f;
		/** Not a whole number of pulses, so its last pulse and its end never coincide. */
		static constexpr double FieldSeconds = 1.75;
		static constexpr double WarningSeconds = 0.5;
		static constexpr double PulseDamage = 10.0;
		static constexpr double EndDamage = 100.0;
		static constexpr double SlowDelay = 1.0;
		static constexpr double QuickDelay = 0.25;

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

			// A field on its caster whose pulses and end hurt the enemies inside, its end warned of.
			FVeyraAreaAbilityTuning Field;
			Field.Cast = InstantCast(0.0, 0.0, 0.0);
			Field.Origin = EVeyraAreaOrigin::Caster;
			Field.Zones.AddDefaulted_GetRef().Shape = CircleOf(Width);
			FVeyraLingerTuning& Lasting = Field.Linger.AddDefaulted_GetRef();
			Lasting.DurationSeconds = FieldSeconds;
			Lasting.PulseSeconds = PulseSeconds;
			Lasting.PulseEffects.AddDefaulted_GetRef().Damage.Add(FVeyraDamageTuning{ EVeyraDamageType::TrueDamage, { PulseDamage }, 0.0, 0.0 });
			Lasting.EndEffects.AddDefaulted_GetRef().Damage.Add(FVeyraDamageTuning{ EVeyraDamageType::TrueDamage, { EndDamage }, 0.0, 0.0 });
			Lasting.EndWarningSeconds = WarningSeconds;
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_field")), Field);

			// A delayed area at a point, sooner inside its caster's field.
			FVeyraAreaAbilityTuning Cure;
			Cure.Cast = InstantCast(Length, 0.0, 0.0);
			Cure.Origin = EVeyraAreaOrigin::TargetPoint;
			Cure.DelaySeconds = SlowDelay;
			Cure.Zones.AddDefaulted_GetRef().Shape = CircleOf(Width / 2.0);
			Cure.DelayWithin.Add(FVeyraAreaDelayWithinTuning{ ArchetypeTestId(TEXT("test_field")), QuickDelay });
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_cure")), Cure);
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

		/** A lingering area of Ability's, Owner's, at At, as a cast of it would leave. */
		void SpawnField(UAbilitySystemComponent& Owner, const FVector& At, const TCHAR* Ability)
		{
			AVeyraLingeringArea& Field = Spawner.SpawnActorAt<AVeyraLingeringArea>(At, FRotator::ZeroRotator);
			FVeyraEffectFrame Placement;
			Placement.Origin = At;
			Field.Arm(Owner, Placement, CircleOf(Width), FVeyraLingerStatuses(), FVeyraLingerEffects(), FieldSeconds, PulseSeconds, ArchetypeTestId(Ability));
		}

		TEST_METHOD(ItsPulsesAndItsEndHitTheEnemiesInside)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Warden = World.Spawn(EVeyraTeam::A, FVector(0.0, Length, 0.0));
			ASSERT_THAT(IsTrue(World.Learn(Warden, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_field")))));
			AVeyraVanguardCharacter& Inside = World.Spawn(EVeyraTeam::B, FVector(Width / 2.0, Length, 0.0));
			AVeyraVanguardCharacter& Outside = World.Spawn(EVeyraTeam::B, FVector(Width * 3.0, Length, 0.0));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(Warden, EVeyraAbilitySlot::W, FVector(Length, Length, 0.0)) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(World.HealthLost(Inside) == 0.0, TEXT("as it lands, its zones do what it does")));
			TActorIterator<AVeyraLingeringArea> Field(&Spawner.GetWorld());
			ASSERT_THAT(IsTrue(static_cast<bool>(Field)));
			ASSERT_THAT(IsTrue(Field->GetEndWarningSeconds() == WarningSeconds, TEXT("its end is warned of")));
			ASSERT_THAT(IsTrue(Field->IsEndNear(Field->GetEndsAt() - WarningSeconds) && !Field->IsEndNear(Field->GetEndsAt() - WarningSeconds - WorldStep)));

			AdvanceWorld(PulseSeconds + WorldStep);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Inside), PulseDamage, 1e-3), FString::SanitizeFloat(World.HealthLost(Inside))));
			AdvanceWorld(FieldSeconds);
			ASSERT_THAT(IsFalse(Lasts()));
			const double Pulses = FMath::FloorToDouble(FieldSeconds / PulseSeconds);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Inside), Pulses * PulseDamage + EndDamage, 1e-3), FString::SanitizeFloat(World.HealthLost(Inside))));
			ASSERT_THAT(IsTrue(World.HealthLost(Outside) == 0.0));
		}

		TEST_METHOD(ItsPulsesPassASpellShieldAndItsEndDoesNot)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Warden = World.Spawn(EVeyraTeam::A, FVector(0.0, Length, 0.0));
			ASSERT_THAT(IsTrue(World.Learn(Warden, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_field")))));
			AVeyraVanguardCharacter& Inside = World.Spawn(EVeyraTeam::B, FVector(Width / 2.0, Length, 0.0));
			FVeyraStatusSpec Ward;
			Ward.Id = ArchetypeTestId(TEXT("test_ward"));
			Ward.Kind = EVeyraStatusKind::SpellShield;
			Ward.DurationSeconds = FieldSeconds * 4.0;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Inside.GetAbilitySystemComponent(), *Inside.GetAbilitySystemComponent(), Ward)));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(Warden, EVeyraAbilitySlot::W, FVector(Length, Length, 0.0)) == EVeyraCastRejection::None));
			AdvanceWorld(PulseSeconds + WorldStep);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Inside), PulseDamage, 1e-3) && World.Has(Inside, TEXT("test_ward")),
				TEXT("a pulse is a tick: it lands, and the shield stays (ADR-025 §4)")));
			AdvanceWorld(FieldSeconds);
			const double Pulses = FMath::FloorToDouble(FieldSeconds / PulseSeconds);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Inside), Pulses * PulseDamage, 1e-3) && !World.Has(Inside, TEXT("test_ward")),
				TEXT("its end is a hit the shield blocks, and spends it")));
		}

		TEST_METHOD(ADelayedAreaInsideItsCastersFieldLandsSooner)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Healer = World.Spawn(EVeyraTeam::A, FVector(0.0, -Length, 0.0));
			AVeyraVanguardCharacter& Other = World.Spawn(EVeyraTeam::A, FVector(0.0, Length, 0.0));
			ASSERT_THAT(IsTrue(World.Learn(Healer, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_cure")))));
			UAbilitySystemComponent& Own = *Healer.GetAbilitySystemComponent();
			const FVeyraAreaAbilityTuning& Cure = Tuning.Area.FindChecked(ArchetypeTestId(TEXT("test_cure")));
			const FVector InOwn(Width, -Length, 0.0);
			const FVector InOthers(Width, Length, 0.0);
			const FVector InSightline(Width, -Length / 2.0, 0.0);
			SpawnField(Own, InOwn, TEXT("test_field"));
			SpawnField(*Other.GetAbilitySystemComponent(), InOthers, TEXT("test_field"));
			SpawnField(Own, InSightline, TEXT("test_sightline"));
			const UWorld& Here = Spawner.GetWorld();
			ASSERT_THAT(IsTrue(VeyraAreaDelivery::DelayAt(Here, Own, Cure, InOwn) == QuickDelay));
			ASSERT_THAT(IsTrue(VeyraAreaDelivery::DelayAt(Here, Own, Cure, InOthers) == SlowDelay, TEXT("only its caster's own field")));
			ASSERT_THAT(IsTrue(VeyraAreaDelivery::DelayAt(Here, Own, Cure, InSightline) == SlowDelay, TEXT("only the named ability's")));
			ASSERT_THAT(IsTrue(VeyraAreaDelivery::DelayAt(Here, Own, Cure, FVector(Width * 3.0, -Length, 0.0)) == SlowDelay, TEXT("only inside it")));

			const double Now = Here.GetTimeSeconds();
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(Healer, EVeyraAbilitySlot::W, InOwn) == EVeyraCastRejection::None));
			TActorIterator<AVeyraDelayedArea> Delayed(&Spawner.GetWorld());
			ASSERT_THAT(IsTrue(static_cast<bool>(Delayed)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Delayed->GetResolvesAt() - Now, QuickDelay), TEXT("a cast there waits the shorter delay")));
		}

		TEST_METHOD(ValidationKeepsPulsesEndsAndDelaysInShape)
		{
			constexpr int32 RankCounts[] = { 5, 3 };
			FVeyraAbilitiesTuning Broken = Tuning;
			FVeyraLingerTuning& Lasting = Broken.Area.FindChecked(ArchetypeTestId(TEXT("test_field"))).Linger[0];
			const FVeyraEffectBundleTuning Pulse = Lasting.PulseEffects[0];
			Lasting.PulseEffects.Add(Pulse);
			Lasting.EndWarningSeconds = FieldSeconds * 2.0;
			FVeyraLingerTuning& Quiet = Broken.Area.FindChecked(ArchetypeTestId(TEXT("test_sightline"))).Linger[0];
			Quiet.EndWarningSeconds = WarningSeconds;
			FVeyraAreaAbilityTuning& Cure = Broken.Area.FindChecked(ArchetypeTestId(TEXT("test_cure")));
			Cure.DelayWithin.Add(FVeyraAreaDelayWithinTuning{ ArchetypeTestId(TEXT("test_cure")), QuickDelay });
			const FString All = FString::Join(VeyraAbilityRules::Validate(Broken, RankCounts), TEXT(" | "));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/area/test_field/linger/0: pulseEffects")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/area/test_field/linger/0/endWarningSeconds")), TEXT("no longer than it lasts")));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/area/test_sightline/linger/0/endWarningSeconds")), TEXT("a warning needs an end that hits")));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/area/test_cure/delayWithin/1/ability")), TEXT("the named ability must linger")));
			ASSERT_THAT(IsFalse(All.Contains(TEXT("/area/test_cure/delayWithin/0")), All));
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
