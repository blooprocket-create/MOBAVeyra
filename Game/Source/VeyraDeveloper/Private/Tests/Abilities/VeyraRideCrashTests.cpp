// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraVitalsSet.h"
#include "CQTest.h"
#include "Engine/World.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	/** Fixture values for crashing rides and zones that reach allies, independent of the committed Abilities.json. */
	namespace CrashFixture
	{
		constexpr double RideSpeed = 600.0;
		constexpr double TurnRate = 200.0;
		constexpr double RideSeconds = 2.0;
		constexpr double LongSeconds = 60.0;
		constexpr double Reach = 400.0;
		constexpr double Near = 200.0;
		constexpr double Far = 3000.0;
		constexpr double Crash = 50.0;
		constexpr double Heal = 30.0;
		constexpr double Injury = 100.0;
		constexpr double Tolerance = 1e-3;
		constexpr float Step = 0.1f;
	}

	// Veyra.Abilities.RideCrashes.*: rides that crash where they end, a dismount that ends them, and zones that
	// heal and empower their caster's allies (ADR-035 §3, §4).
	TEST_CLASS(RideCrashes, "Veyra.Abilities")
	{
		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Rider = nullptr;

		/** A zone of Reach that strikes enemies for Crash and heals allies for Heal with a status. */
		static FVeyraAreaZoneTuning CrashZone(EVeyraAllyReach AllyReach)
		{
			using namespace CrashFixture;
			FVeyraAreaZoneTuning Zone;
			Zone.Shape = CircleOf(Reach);
			Zone.Effects.Damage.Add(FVeyraDamageTuning{ EVeyraDamageType::TrueDamage, { Crash }, 0.0, 0.0 });
			FVeyraZoneAllyEffectsTuning& Allies = Zone.AllyEffects.AddDefaulted_GetRef();
			Allies.HealByRank = { Heal };
			Allies.Statuses = { ArchetypeTestId(TEXT("test_wake")) };
			Allies.Reach = AllyReach;
			return Zone;
		}

		BEFORE_EACH()
		{
			using namespace CrashFixture;
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_wake")), StatusOf(EVeyraStatusKind::MoveSpeed, 0.1, LongSeconds));
			FVeyraDismountAbilityTuning Leave;
			Leave.Cast = InstantCast(0.0, 0.0, 0.0);
			Tuning.Dismount.Add(ArchetypeTestId(TEXT("test_crash")), Leave);
			FVeyraRideAbilityTuning Wave;
			Wave.Cast = InstantCast(0.0, LongSeconds, 0.0);
			Wave.SetSpeed = RideSpeed;
			Wave.TurnRateDegreesPerSecond = TurnRate;
			Wave.DurationSeconds = RideSeconds;
			Wave.Mounted.Add(FVeyraRideSlotTuning{ EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_crash")) });
			Wave.CrashZones.Add(CrashZone(EVeyraAllyReach::OthersOnly));
			Tuning.Ride.Add(ArchetypeTestId(TEXT("test_wave")), Wave);
			FVeyraAreaAbilityTuning Tide;
			Tide.Cast = InstantCast(Far, LongSeconds, 0.0);
			Tide.Origin = EVeyraAreaOrigin::TargetPoint;
			Tide.Zones.Add(CrashZone(EVeyraAllyReach::CasterToo));
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_tide")), Tide);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);
			const int32 Ranks[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, Ranks).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, Ranks), TEXT(" | "))));

			FArchetypeTestWorld World{ Spawner };
			Rider = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, CrashFixture::Step);
				++GFrameCounter;
				World.GetTimerManager().Tick(CrashFixture::Step);
			}
		}

		static double Lost(const AActor& Unit)
		{
			const UAbilitySystemComponent& Abilities = *UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
			return Abilities.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) - Abilities.GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
		}

		static bool Wound(AActor& Source, AActor& Target, double Amount)
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Amount });
			return VeyraCombat::DealDamage(*UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Source),
				*UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Target), Damage);
		}

		bool Ride()
		{
			FArchetypeTestWorld World{ Spawner };
			return World.Learn(*Rider, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_wave")))
				&& FArchetypeTestWorld::CastAt(*Rider, EVeyraAbilitySlot::Q, FVector::ZeroVector) == EVeyraCastRejection::None
				&& Rider->GetVeyraMovement()->IsRiding();
		}

		TEST_METHOD(ItsRecastCrashesItWhereItIs)
		{
			using namespace CrashFixture;
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			AVeyraVanguardCharacter& Ally = World.Spawn(EVeyraTeam::A, FVector(0.0, Near, 0.0));
			ASSERT_THAT(IsTrue(Wound(Enemy, Ally, Injury) && Wound(Enemy, *Rider, Injury)));
			ASSERT_THAT(IsTrue(Ride()));
			ASSERT_THAT(IsTrue(Lost(Enemy) == 0.0, TEXT("nothing before the crash")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Rider, EVeyraAbilitySlot::Q, FVector::ZeroVector) == EVeyraCastRejection::None, TEXT("its mounted Q dismounts")));
			ASSERT_THAT(IsFalse(Rider->GetVeyraMovement()->IsRiding()));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Lost(Enemy), Crash, Tolerance), TEXT("the crash strikes the enemy")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Lost(Ally), Injury - Heal, Tolerance) && FArchetypeTestWorld::Has(Ally, TEXT("test_wake")),
				TEXT("and heals and empowers the ally")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Lost(*Rider), Injury, Tolerance) && !FArchetypeTestWorld::Has(*Rider, TEXT("test_wake")),
				TEXT("but not its rider, when it reaches others only")));
		}

		TEST_METHOD(ItCrashesAsItsTimeRunsOut)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(CrashFixture::Near, 0.0, 0.0));
			ASSERT_THAT(IsTrue(Ride()));
			Wait(CrashFixture::RideSeconds + CrashFixture::Step * 2.0);
			ASSERT_THAT(IsFalse(Rider->GetVeyraMovement()->IsRiding()));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Lost(Enemy), CrashFixture::Crash, CrashFixture::Tolerance)));
		}

		TEST_METHOD(ItDoesNotCrashAsItsRiderDies)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(CrashFixture::Near, 0.0, 0.0));
			ASSERT_THAT(IsTrue(Ride()));
			ASSERT_THAT(IsTrue(Wound(Enemy, *Rider, VeyraCombatTests::ExampleStats().MaxHealth * 2.0)));
			ASSERT_THAT(IsFalse(Rider->GetVeyraMovement()->IsRiding()));
			ASSERT_THAT(IsTrue(Lost(Enemy) == 0.0, TEXT("a dead rider's wave breaks on nothing")));
		}

		TEST_METHOD(OffItsRideTheDismountIsRefused)
		{
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(World.Learn(*Rider, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_crash")))));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Rider, EVeyraAbilitySlot::W, FVector::ZeroVector) == EVeyraCastRejection::InvalidTarget));
		}

		TEST_METHOD(AZoneThatReachesItsCasterHealsItToo)
		{
			using namespace CrashFixture;
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Far, 0.0, 0.0));
			ASSERT_THAT(IsTrue(Wound(Enemy, *Rider, Injury)));
			ASSERT_THAT(IsTrue(World.Learn(*Rider, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_tide")))));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Rider, EVeyraAbilitySlot::E, FVector::ZeroVector) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Lost(*Rider), Injury - Heal, Tolerance) && FArchetypeTestWorld::Has(*Rider, TEXT("test_wake"))));
		}

		TEST_METHOD(ValidationWantsAZoneToDoSomethingForItsAllies)
		{
			const int32 Ranks[] = { 5, 3 };
			FVeyraAbilitiesTuning Broken = Tuning;
			FVeyraZoneAllyEffectsTuning& Nothing = Broken.Ride[ArchetypeTestId(TEXT("test_wave"))].CrashZones[0].AllyEffects[0];
			Nothing.HealByRank = { 0.0 };
			Nothing.Statuses.Reset();
			ASSERT_THAT(IsFalse(VeyraAbilityRules::Validate(Broken, Ranks).IsEmpty(), TEXT("an ally effect that heals nothing and gives nothing")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
