// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraVitalsSet.h"
#include "CQTest.h"
#include "Delivery/VeyraLingeringArea.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	/** Fixture values for rides that strike what they meet and leave a trail, independent of the committed Abilities.json. */
	namespace ContactFixture
	{
		constexpr double RideSpeed = 600.0;
		constexpr double TurnRate = 200.0;
		constexpr double RideSeconds = 4.0;
		constexpr double LongSeconds = 60.0;
		constexpr double Reach = 60.0;
		constexpr double Pulse = 0.1;
		constexpr double Impact = 40.0;
		constexpr double Heal = 30.0;
		constexpr double Knock = 200.0;
		constexpr double KnockSpeed = 1000.0;
		constexpr double Spacing = 300.0;
		constexpr double Touching = 100.0;
		constexpr double Wake = 150.0;
		constexpr double Lasting = 3.0;
		constexpr double LingerPulse = 0.5;
		constexpr double Injury = 100.0;
		constexpr double Tolerance = 1e-3;
		constexpr float Step = 0.1f;
	}

	// Veyra.Abilities.RideContacts.*: rides whose body strikes the enemies and helps the allies it meets, each once a
	// ride, and that lay an area along their path (ADR-035 §6).
	TEST_CLASS(RideContacts, "Veyra.Abilities")
	{
		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Rider = nullptr;

		BEFORE_EACH()
		{
			using namespace ContactFixture;
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_wake")), StatusOf(EVeyraStatusKind::MoveSpeed, 0.1, LingerPulse * 2.0));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_rip")), StatusOf(EVeyraStatusKind::Slow, 0.2, LingerPulse * 2.0));
			FVeyraAreaAbilityTuning Trail;
			Trail.Cast = InstantCast(0.0, LongSeconds, 0.0);
			Trail.Origin = EVeyraAreaOrigin::Caster;
			FVeyraAreaZoneTuning& Zone = Trail.Zones.AddDefaulted_GetRef();
			Zone.Shape = CircleOf(Wake);
			FVeyraLingerTuning& Linger = Trail.Linger.AddDefaulted_GetRef();
			Linger.DurationSeconds = Lasting;
			Linger.PulseSeconds = LingerPulse;
			Linger.AllyStatuses = { ArchetypeTestId(TEXT("test_wake")) };
			Linger.EnemyStatuses = { ArchetypeTestId(TEXT("test_rip")) };
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_wake_area")), Trail);

			FVeyraRideAbilityTuning Wave;
			Wave.Cast = InstantCast(0.0, LongSeconds, 0.0);
			Wave.SetSpeed = RideSpeed;
			Wave.TurnRateDegreesPerSecond = TurnRate;
			Wave.DurationSeconds = RideSeconds;
			FVeyraRideContactTuning& Contact = Wave.Contact.AddDefaulted_GetRef();
			Contact.Reach = Reach;
			Contact.PulseSeconds = Pulse;
			Contact.EnemyEffects.Damage.Add(FVeyraDamageTuning{ EVeyraDamageType::TrueDamage, { Impact }, 0.0, 0.0 });
			FVeyraDisplacementTuning& Aside = Contact.EnemyEffects.Displacement.AddDefaulted_GetRef();
			Aside.Direction = EVeyraDisplacementDirection::AsideFromPath;
			Aside.Distance = Knock;
			Aside.Speed = KnockSpeed;
			Contact.EnemyKinds = { EVeyraUnitKind::Vanguard };
			FVeyraZoneAllyEffectsTuning& Allies = Contact.AllyEffects.AddDefaulted_GetRef();
			Allies.HealByRank = { Heal };
			Wave.Trail.Add(FVeyraRideTrailTuning{ Spacing, Pulse, ArchetypeTestId(TEXT("test_wake_area")) });
			Tuning.Ride.Add(ArchetypeTestId(TEXT("test_tidal")), Wave);
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
				World.Tick(LEVELTICK_TimeOnly, ContactFixture::Step);
				++GFrameCounter;
				World.GetTimerManager().Tick(ContactFixture::Step);
			}
		}

		bool Ride()
		{
			FArchetypeTestWorld World{ Spawner };
			return World.Learn(*Rider, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_tidal")))
				&& FArchetypeTestWorld::CastAt(*Rider, EVeyraAbilitySlot::Q, FVector::ZeroVector) == EVeyraCastRejection::None
				&& Rider->GetVeyraMovement()->IsRiding();
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

		int32 Areas()
		{
			int32 Count = 0;
			for (TActorIterator<AVeyraLingeringArea> It(&Spawner.GetWorld()); It; ++It)
			{
				++Count;
			}
			return Count;
		}

		TEST_METHOD(ItStrikesAnEnemyVanguardItMeetsOnceAndKnocksItAside)
		{
			using namespace ContactFixture;
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Touching, 0.0, 0.0));
			ASSERT_THAT(IsTrue(Ride()));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Lost(Enemy), Impact, Tolerance), TEXT("it strikes as it sets off")));
			const TOptional<FVector> Lands = Enemy.GetVeyraMovement()->GetForcedMoveDestination();
			ASSERT_THAT(IsTrue(Lands.IsSet() && FMath::Abs(Lands->Y - Enemy.GetActorLocation().Y) > Knock / 2.0, TEXT("and knocks it off the ride's line")));
			Enemy.GetVeyraMovement()->AdvanceForcedMove(static_cast<float>(Knock / KnockSpeed));
			Enemy.SetActorLocation(FVector(Touching, 0.0, Enemy.GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
			Wait(Pulse * 3.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Lost(Enemy), Impact, Tolerance), TEXT("once a ride")));
		}

		TEST_METHOD(ItPassesAFluxbornByAndHelpsAnAllyOnce)
		{
			using namespace ContactFixture;
			FArchetypeTestWorld World{ Spawner };
			AVeyraTestFluxborn& Minion = World.SpawnFluxborn(EVeyraTeam::B, FVector(Touching, 0.0, 0.0));
			AVeyraVanguardCharacter& Ally = World.Spawn(EVeyraTeam::A, FVector(0.0, Touching, 0.0));
			ASSERT_THAT(IsTrue(Wound(Minion, Ally, Injury)));
			ASSERT_THAT(IsTrue(Ride()));
			Wait(Pulse * 3.0);
			ASSERT_THAT(IsTrue(Lost(Minion) == 0.0, TEXT("it strikes enemy Vanguards only")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Lost(Ally), Injury - Heal, Tolerance), TEXT("and heals an ally once")));
		}

		TEST_METHOD(ItLaysItsTrailAsItGoes)
		{
			using namespace ContactFixture;
			ASSERT_THAT(IsTrue(Ride()));
			ASSERT_THAT(AreEqual(1, Areas(), TEXT("the first as it sets off")));
			const double Z = Rider->GetActorLocation().Z;
			Rider->SetActorLocation(FVector(Spacing + Touching, 0.0, Z), false, nullptr, ETeleportType::TeleportPhysics);
			Wait(Pulse * 3.0);
			ASSERT_THAT(AreEqual(2, Areas(), *FString::Printf(TEXT("another once it has come its spacing: %d area(s), riding %d, at %s"), Areas(),
				Rider->GetVeyraMovement()->IsRiding() ? 1 : 0, *Rider->GetActorLocation().ToCompactString())));
			Rider->SetActorLocation(FVector(Spacing + Touching * 2.0, 0.0, Z), false, nullptr, ETeleportType::TeleportPhysics);
			Wait(Pulse * 3.0);
			ASSERT_THAT(AreEqual(2, Areas(), TEXT("and none before")));
		}

		TEST_METHOD(ValidationKeepsTheTrailInPaceAndLingering)
		{
			const int32 Ranks[] = { 5, 3 };
			FVeyraAbilitiesTuning Broken = Tuning;
			Broken.Ride[ArchetypeTestId(TEXT("test_tidal"))].Trail[0].PulseSeconds = ContactFixture::Spacing;
			ASSERT_THAT(IsFalse(VeyraAbilityRules::Validate(Broken, Ranks).IsEmpty(), TEXT("a trail that looks less than once a spacing")));
			Broken = Tuning;
			Broken.Area[ArchetypeTestId(TEXT("test_wake_area"))].Linger.Reset();
			ASSERT_THAT(IsFalse(VeyraAbilityRules::Validate(Broken, Ranks).IsEmpty(), TEXT("a trail of an area that does not linger")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
