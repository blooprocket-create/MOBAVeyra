// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "CQTest.h"
#include "Targeting/VeyraVisibility.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraVisionSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	/** Fixture values for areas that lay fog and build shields (ADR-036 §3, §4). */
	namespace AreaFogFixture
	{
		constexpr double Range = 900.0;
		constexpr double Radius = 300.0;
		constexpr double Point = 600.0;
		constexpr double Lasts = 4.0;
		constexpr double Length = 1200.0;
		constexpr double Width = 400.0;
		constexpr double Pulse = 1.0;
		constexpr double Amount = 20.0;
		constexpr double CapRatio = 0.1;
		constexpr double Delay = 2.0;
		constexpr double ShieldLasts = 3.0;
		constexpr double Blow = 100.0;
		constexpr double Tolerance = 1e-3;
		constexpr float Step = 0.1f;
	}

	/** An area at Origin whose one zone is a circle of Radius that does nothing. */
	inline FVeyraAreaAbilityTuning QuietAreaOf(EVeyraAreaOrigin Origin, double ZoneRadius)
	{
		FVeyraAreaAbilityTuning Area;
		Area.Cast = InstantCast(AreaFogFixture::Range, 0.0, 0.0);
		Area.Origin = Origin;
		FVeyraAreaZoneTuning& Zone = Area.Zones.AddDefaulted_GetRef();
		Zone.Shape = CircleOf(ZoneRadius);
		return Area;
	}

	// Veyra.Abilities.AreaFog.*: areas that lay Dense Fog as they commit (ADR-036 §3).
	TEST_CLASS(AreaFog, "Veyra.Abilities")
	{
		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;

		BEFORE_EACH()
		{
			using namespace AreaFogFixture;
			FVeyraAreaAbilityTuning Mist = QuietAreaOf(EVeyraAreaOrigin::TargetPoint, Radius);
			FVeyraAreaFogTuning& Circle = Mist.Fog.AddDefaulted_GetRef();
			Circle.Shape = EVeyraAreaFogShape::Circle;
			Circle.Radius = Radius;
			Circle.DurationSeconds = Lasts;
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_mist")), Mist);

			FVeyraAreaAbilityTuning White = QuietAreaOf(EVeyraAreaOrigin::Caster, Width / 2.0);
			FVeyraAreaFogTuning& Corridor = White.Fog.AddDefaulted_GetRef();
			Corridor.Shape = EVeyraAreaFogShape::Corridor;
			Corridor.Length = Length;
			Corridor.Width = Width;
			Corridor.DurationSeconds = Lasts;
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_white")), White);

			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);
			const int32 Ranks[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, Ranks).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, Ranks), TEXT(" | "))));
			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Spawner.GetWorld().GetSubsystem<UVeyraVisionSubsystem>()->Start();
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		int32 VolumeAt(double X, double Y)
		{
			return VeyraVisibility::FogVolumeAt(&Spawner.GetWorld(), FVector(X, Y, 0.0));
		}

		TEST_METHOD(ACircleLaysFogWhereItLands)
		{
			using namespace AreaFogFixture;
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Caster, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_mist")))));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::W, FVector(Point, 0.0, 0.0)) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(VolumeAt(Point + Radius / 2.0, 0.0) != INDEX_NONE, TEXT("fog where it landed")));
			ASSERT_THAT(IsTrue(VolumeAt(0.0, 0.0) == INDEX_NONE, TEXT("none where its caster stands")));
		}

		TEST_METHOD(ACorridorRunsFromItsCasterAlongItsAim)
		{
			using namespace AreaFogFixture;
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Caster, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_white")))));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::Q, FVector(0.0, Point, 0.0)) == EVeyraCastRejection::None));
			const int32 Near = VolumeAt(0.0, Width / 2.0);
			ASSERT_THAT(IsTrue(Near != INDEX_NONE && Near == VolumeAt(0.0, Length - Width / 2.0), TEXT("one volume, along its aim, to its end")));
			ASSERT_THAT(IsTrue(VolumeAt(Length / 2.0, 0.0) == INDEX_NONE, TEXT("nothing off its line")));
			ASSERT_THAT(IsTrue(VolumeAt(0.0, Length + Width) == INDEX_NONE, TEXT("nothing past its end")));
		}

		TEST_METHOD(ValidationKeepsFogInShape)
		{
			using namespace AreaFogFixture;
			const int32 Ranks[] = { 5, 3 };
			FVeyraAbilitiesTuning Bad = Tuning;
			Bad.Area.FindChecked(ArchetypeTestId(TEXT("test_mist"))).Fog[0].Length = Length;
			Bad.Area.FindChecked(ArchetypeTestId(TEXT("test_white"))).Fog[0].DurationSeconds = 0.0;
			ASSERT_THAT(AreEqual(2, VeyraAbilityRules::Validate(Bad, Ranks).Num()));
			FVeyraAbilitiesTuning Twice = Tuning;
			FVeyraAreaAbilityTuning& Mist = Twice.Area.FindChecked(ArchetypeTestId(TEXT("test_mist")));
			const FVeyraAreaFogTuning Again = Mist.Fog[0];
			Mist.Fog.Add(Again);
			ASSERT_THAT(AreEqual(1, VeyraAbilityRules::Validate(Twice, Ranks).Num()));
		}
	};

	// Veyra.Abilities.ShieldTopUps.*: a lingering area that builds a shield on allies who stay (ADR-036 §4).
	TEST_CLASS(ShieldTopUps, "Veyra.Abilities")
	{
		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;
		AVeyraVanguardCharacter* Ally = nullptr;

		BEFORE_EACH()
		{
			using namespace AreaFogFixture;
			FVeyraAreaAbilityTuning Waymark = QuietAreaOf(EVeyraAreaOrigin::TargetPoint, Radius);
			FVeyraLingerTuning& Linger = Waymark.Linger.AddDefaulted_GetRef();
			Linger.DurationSeconds = Lasts * 3.0;
			Linger.PulseSeconds = Pulse;
			FVeyraShieldTopUpTuning& TopUp = Linger.ShieldTopUp.AddDefaulted_GetRef();
			TopUp.Shield.Id = ArchetypeTestId(TEXT("test_harbor_shield"));
			TopUp.Shield.AmountByRank = { Amount };
			TopUp.Shield.DurationSeconds = ShieldLasts;
			TopUp.Shield.Reapply = EVeyraShieldReapply::Merge;
			TopUp.Shield.MaxAmountMaxHealthRatio = CapRatio;
			TopUp.DelayAfterDamageSeconds = Delay;
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_waymark")), Waymark);

			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);
			const int32 Ranks[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, Ranks).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, Ranks), TEXT(" | "))));
			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Ally = &World.Spawn(EVeyraTeam::A, FVector(Point, 0.0, 0.0));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Caster, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_waymark")))));
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		static double ShieldOf(const AVeyraVanguardCharacter& Unit)
		{
			const UVeyraDamageAbsorptionComponent* Absorption = Unit.GetPlayerState()->FindComponentByClass<UVeyraDamageAbsorptionComponent>();
			double Sum = 0.0;
			for (const FVeyraShieldEntry& Shield : Absorption ? Absorption->GetLedger().Shields : TArray<FVeyraShieldEntry>())
			{
				Sum += Shield.Remaining;
			}
			return Sum;
		}

		/** The cap: a share of the caster's Max Health. */
		double Cap() const
		{
			return AreaFogFixture::CapRatio * Caster->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
		}

		/** World time passes, and the area's pulses come. */
		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, AreaFogFixture::Step);
				++GFrameCounter;
				World.GetTimerManager().Tick(AreaFogFixture::Step);
			}
		}

		TEST_METHOD(ItBuildsOnThoseWhoStayUpToItsCap)
		{
			using namespace AreaFogFixture;
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::E, Ally->GetActorLocation()) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ShieldOf(*Ally), Amount, Tolerance), TEXT("one grant as it lands")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyZero(ShieldOf(*Caster)), TEXT("its caster stands outside it")));
			Wait(Pulse + Step * 3.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ShieldOf(*Ally), FMath::Min(Amount * 2.0, Cap()), Tolerance), TEXT("it builds pulse by pulse")));
			Wait(Pulse * FMath::CeilToDouble(Cap() / Amount) + Step * 3.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ShieldOf(*Ally), Cap(), Tolerance), TEXT("never past its cap")));
		}

		TEST_METHOD(DamageHoldsItBackForItsDelay)
		{
			using namespace AreaFogFixture;
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::E, Ally->GetActorLocation()) == EVeyraCastRejection::None));
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Point * 3.0, 0.0, 0.0));
			FVeyraRawDamageEvent Hit;
			Hit.Components.Add({ EVeyraDamageType::TrueDamage, Blow });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Enemy.GetAbilitySystemComponent(), *Ally->GetAbilitySystemComponent(), Hit)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyZero(ShieldOf(*Ally)), TEXT("broken")));
			Wait(Pulse + Step * 3.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyZero(ShieldOf(*Ally)), TEXT("held back while the hit is fresh")));
			Wait(Delay + Step * 3.0);
			ASSERT_THAT(IsTrue(ShieldOf(*Ally) > 0.0, TEXT("then it builds again")));
		}

		TEST_METHOD(ValidationKeepsItsShieldMerging)
		{
			const int32 Ranks[] = { 5, 3 };
			FVeyraAbilitiesTuning Bad = Tuning;
			FVeyraShieldTopUpTuning& TopUp = Bad.Area.FindChecked(ArchetypeTestId(TEXT("test_waymark"))).Linger[0].ShieldTopUp[0];
			TopUp.Shield.Reapply = EVeyraShieldReapply::Replace;
			ASSERT_THAT(AreEqual(1, VeyraAbilityRules::Validate(Bad, Ranks).Num()));
			TopUp.Shield.Reapply = EVeyraShieldReapply::Merge;
			TopUp.DelayAfterDamageSeconds = -1.0;
			ASSERT_THAT(AreEqual(1, VeyraAbilityRules::Validate(Bad, Ranks).Num()));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
