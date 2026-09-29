// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Delivery/VeyraProjectile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	// Veyra.Abilities.RideArchetype.*: an ability that rides (Combat Bible §56), as Raska's Kickstart and
	// NO BRAKES: its mounted actions and statuses while it lasts, and its vehicle going on without its
	// rider at every exit.
	TEST_CLASS(RideArchetype, "Veyra.Abilities")
	{
		// Fixture values, independent of the committed Abilities.json.
		static constexpr double RideSpeed = 600.0;
		static constexpr double TurnRate = 200.0;
		static constexpr double RideSeconds = 2.0;
		static constexpr double LongSeconds = 60.0;
		static constexpr double Range = 1000.0;
		static constexpr double Leap = 400.0;
		static constexpr float WorldStep = 0.1f;

		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Rider = nullptr;
		UVeyraAbilityLoadoutComponent* Loadout = nullptr;

		BEFORE_EACH()
		{
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_big")), StatusOf(EVeyraStatusKind::BodyScale, 1.5, LongSeconds));

			FVeyraSkillshotAbilityTuning Hound;
			Hound.Cast = InstantCast(Range, LongSeconds, 0.0);
			Hound.Projectile.Speed = Range;
			Hound.Projectile.Radius = 50.0;
			Hound.Projectile.Range = Range;
			Hound.Collision = EVeyraSkillshotCollision::Pierce;
			Tuning.Skillshot.Add(ArchetypeTestId(TEXT("test_hound")), Hound);

			FVeyraDashAbilityTuning Bail;
			Bail.Cast = InstantCast(Range, LongSeconds, 0.0);
			Bail.Direction = EVeyraDashDirection::TowardPoint;
			Bail.Distance = Leap;
			Bail.Speed = Leap * 4.0;
			Bail.RideExit = EVeyraRideExit::Leave;
			Tuning.Dash.Add(ArchetypeTestId(TEXT("test_bail")), Bail);

			FVeyraRideAbilityTuning Kick;
			Kick.Cast = InstantCast(0.0, LongSeconds, 0.0);
			Kick.SetSpeed = RideSpeed;
			Kick.TurnRateDegreesPerSecond = TurnRate;
			Kick.DurationSeconds = RideSeconds;
			Kick.RiderStatuses.Add(ArchetypeTestId(TEXT("test_big")));
			Kick.Mounted.Add(FVeyraRideSlotTuning{ EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_bail")) });
			Kick.Vehicle.Add(ArchetypeTestId(TEXT("test_hound")));
			Tuning.Ride.Add(ArchetypeTestId(TEXT("test_kick")), Kick);

			// A ride whose recast fires by itself as it runs out, as NO BRAKES' Last Exit.
			FVeyraRideAbilityTuning Brakes = Kick;
			Brakes.Mounted.Reset();
			FVeyraRecastTuning& Exit = Brakes.Cast.RecastWindow.AddDefaulted_GetRef();
			Exit.Ability = ArchetypeTestId(TEXT("test_bail"));
			Exit.WindowSeconds = RideSeconds;
			Exit.OnExpiry = EVeyraRecastExpiry::Cast;
			Tuning.Ride.Add(ArchetypeTestId(TEXT("test_brakes")), Brakes);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);

			FArchetypeTestWorld World{ Spawner };
			Rider = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Loadout = Rider->GetPlayerState()->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

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

		bool Holds(EVeyraAbilitySlot Slot, const TCHAR* Ability) const
		{
			const FVeyraLoadoutEntry* Entry = Loadout->FindSlot(Slot);
			return Entry && Entry->Ability == ArchetypeTestId(Ability);
		}

		int32 Vehicles()
		{
			int32 Count = 0;
			for (TActorIterator<AVeyraProjectile> It(&Spawner.GetWorld()); It; ++It)
			{
				++Count;
			}
			return Count;
		}

		bool Ride(const TCHAR* Ability)
		{
			FArchetypeTestWorld World{ Spawner };
			return World.Learn(*Rider, EVeyraAbilitySlot::Q, ArchetypeTestId(Ability))
				&& FArchetypeTestWorld::CastAt(*Rider, EVeyraAbilitySlot::Q, FVector::ZeroVector) == EVeyraCastRejection::None;
		}

		TEST_METHOD(MountedItHoldsItsActionsAndItsStatuses)
		{
			ASSERT_THAT(IsTrue(Ride(TEXT("test_kick"))));
			ASSERT_THAT(IsTrue(Rider->GetVeyraMovement()->IsRiding() && Holds(EVeyraAbilitySlot::Q, TEXT("test_bail")), TEXT("its own slot holds its mounted action")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(*Rider, TEXT("test_big"))));
		}

		TEST_METHOD(BailingOutLeapsAndLeavesTheRideAsItLands)
		{
			ASSERT_THAT(IsTrue(Ride(TEXT("test_kick"))));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Rider, EVeyraAbilitySlot::Q, FVector(Range, 0.0, 0.0)) == EVeyraCastRejection::None));
			// It leaps free and leaves the ride where it lands, after its landing's effects (Roster Bible §1);
			// the landing itself runs over real frames, in Veyra.Net.Vanguards.Raska.
			ASSERT_THAT(IsTrue(Rider->GetVeyraMovement()->IsDashing() && Rider->GetVeyraMovement()->IsRiding() && Vehicles() == 0));
		}

		TEST_METHOD(ItEndsWithItsTimeAndTheVehicleGoesOn)
		{
			ASSERT_THAT(IsTrue(Ride(TEXT("test_kick"))));
			AdvanceWorld(RideSeconds + WorldStep * 2.0);
			ASSERT_THAT(IsFalse(Rider->GetVeyraMovement()->IsRiding()));
			ASSERT_THAT(IsTrue(Vehicles() == 1));
		}

		TEST_METHOD(ItEndsWithItsRidersDeath)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Range, 0.0, 0.0));
			ASSERT_THAT(IsTrue(Ride(TEXT("test_kick"))));
			FVeyraRawDamageEvent Lethal;
			Lethal.Components.Add({ EVeyraDamageType::TrueDamage, VeyraCombatTests::ExampleStats().MaxHealth * 2.0 });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Enemy.GetAbilitySystemComponent(), *Rider->GetAbilitySystemComponent(), Lethal)));
			ASSERT_THAT(IsFalse(Rider->GetVeyraMovement()->IsRiding()));
			ASSERT_THAT(IsTrue(Vehicles() == 1, TEXT("its vehicle goes on along its heading")));
		}

		TEST_METHOD(ARecastThatFiresAtExpiryFiresAsItRunsOut)
		{
			ASSERT_THAT(IsTrue(Ride(TEXT("test_brakes"))));
			ASSERT_THAT(IsTrue(Holds(EVeyraAbilitySlot::Q, TEXT("test_bail"))));
			AdvanceWorld(RideSeconds + WorldStep * 2.0);
			ASSERT_THAT(IsTrue(Rider->GetVeyraMovement()->IsDashing(), TEXT("its payoff comes whether or not it was cast")));
			ASSERT_THAT(IsTrue(Rider->GetVeyraMovement()->IsRiding() && Vehicles() == 0, TEXT("the payoff ends the ride as it lands, not the clock")));
		}

		TEST_METHOD(ValidationKeepsARideInShape)
		{
			constexpr int32 RankCounts[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, RankCounts).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, RankCounts), TEXT(" | "))));
			FVeyraAbilitiesTuning Broken = Tuning;
			FVeyraRideAbilityTuning& Kick = Broken.Ride.FindChecked(ArchetypeTestId(TEXT("test_kick")));
			Kick.TurnRateDegreesPerSecond = 0.0;
			Kick.Mounted.Add(FVeyraRideSlotTuning{ EVeyraAbilitySlot::R, ArchetypeTestId(TEXT("test_bail")) });
			Kick.Vehicle = { ArchetypeTestId(TEXT("test_bail")) };
			Broken.Dash.FindChecked(ArchetypeTestId(TEXT("test_bail"))).RideExit = EVeyraRideExit::Stay;
			const FString All = FString::Join(VeyraAbilityRules::Validate(Broken, RankCounts), TEXT(" | "));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/ride/test_kick:")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/ride/test_kick/mounted/1/slot:")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/ride/test_kick/vehicle:")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/ride/test_brakes/cast/recastWindow:")), All));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
