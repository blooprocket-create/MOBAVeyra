// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Delivery/VeyraLingeringArea.h"
#include "EngineUtils.h"
#include "Movement/VeyraMovementFields.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	/** Fixture values for a field beside a dash's path. */
	namespace FieldFixture
	{
		constexpr double Radius = 250.0;
		constexpr double Pull = 150.0;
		constexpr double Beside = 200.0;
		constexpr double Midway = 300.0;
		constexpr double Far = 2000.0;
		constexpr double DashDistance = 600.0;
		constexpr double DashSpeed = 1200.0;
		constexpr double Lasting = 4.0;
		constexpr double Pulse = 1.0;
		constexpr double CastRange = 800.0;
		constexpr float Step = 0.1f;
	}

	// Veyra.Abilities.MovementFields.*: fields that bend enemy dashes and displacements toward their centre,
	// and a lingering area that holds one while it stands (ADR-033 §5).
	TEST_CLASS(MovementFields, "Veyra.Abilities")
	{
		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Mover = nullptr;

		BEFORE_EACH()
		{
			using namespace FieldFixture;
			FVeyraAreaAbilityTuning Field;
			Field.Cast = InstantCast(CastRange, 0.0, 0.0);
			Field.Origin = EVeyraAreaOrigin::TargetPoint;
			FVeyraAreaZoneTuning& Zone = Field.Zones.AddDefaulted_GetRef();
			Zone.Shape = CircleOf(Radius);
			FVeyraLingerTuning& Linger = Field.Linger.AddDefaulted_GetRef();
			Linger.DurationSeconds = Lasting;
			Linger.PulseSeconds = Pulse;
			Linger.MovementField.Add(FVeyraMovementFieldTuning{ Pull });
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_field")), Field);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);
			const int32 Ranks[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, Ranks).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, Ranks), TEXT(" | "))));

			FArchetypeTestWorld World{ Spawner };
			Mover = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		UVeyraMovementFieldSubsystem& Fields()
		{
			return *Spawner.GetWorld().GetSubsystem<UVeyraMovementFieldSubsystem>();
		}

		/** Where a dash ahead would end, from where the mover stands; it then goes back. */
		FVector DashAhead(double Distance = FieldFixture::DashDistance) const
		{
			FVeyraDash Dash;
			Dash.Direction = FVector::ForwardVector;
			Dash.Distance = Distance;
			Dash.Speed = FieldFixture::DashSpeed;
			const FVector Stood = Mover->GetActorLocation();
			if (!VeyraCombat::Dash(*Mover->GetAbilitySystemComponent(), Dash))
			{
				return Stood;
			}
			const FVector Lands = Mover->GetVeyraMovement()->GetForcedMoveDestination().Get(Stood);
			VeyraCombat::Blink(*Mover->GetAbilitySystemComponent(), Stood);
			return Lands;
		}

		FVector BesidePath() const
		{
			return Mover->GetActorLocation() + FVector(FieldFixture::Midway, FieldFixture::Beside, 0.0);
		}

		TEST_METHOD(AnEnemyFieldBendsADashThatCrossesItTowardItsCentre)
		{
			const int32 Handle = Fields().Add(FVeyraMovementField{ BesidePath(), FieldFixture::Radius, EVeyraTeam::B, FieldFixture::Pull });
			const FVector Lands = DashAhead();
			ASSERT_THAT(IsTrue(Lands.Y - Mover->GetActorLocation().Y > FieldFixture::Pull / 4.0, *FString::Printf(TEXT("it bends toward the centre: %s"), *Lands.ToString())));
			Fields().Remove(Handle);
			const FVector Straight = DashAhead();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Straight.Y, Mover->GetActorLocation().Y, 1.0), TEXT("and with the field gone it goes straight")));
		}

		TEST_METHOD(AMoveBentOntoItsOwnStartGoesNowhere)
		{
			// Centred where the mover stands, the field pulls the end of a dash shorter than its pull all the way back.
			const FVector Stood = Mover->GetActorLocation();
			Fields().Add(FVeyraMovementField{ Stood, FieldFixture::Radius, EVeyraTeam::B, FieldFixture::Pull });
			const FVector Lands = DashAhead(FieldFixture::Pull / 2.0);
			ASSERT_THAT(IsTrue(FVector::Dist2D(Lands, Stood) < 1.0, *FString::Printf(TEXT("it lands where it began: %s"), *Lands.ToString())));
		}

		TEST_METHOD(AnAlliedOrFarFieldLeavesItBe)
		{
			Fields().Add(FVeyraMovementField{ BesidePath(), FieldFixture::Radius, EVeyraTeam::A, FieldFixture::Pull });
			Fields().Add(FVeyraMovementField{ Mover->GetActorLocation() + FVector(0.0, FieldFixture::Far, 0.0), FieldFixture::Radius, EVeyraTeam::B, FieldFixture::Pull });
			const FVector Lands = DashAhead();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Lands.Y, Mover->GetActorLocation().Y, 1.0), TEXT("its own side's field, and one it never nears, bend nothing")));
		}

		TEST_METHOD(ADisplacementBendsToo)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, Mover->GetActorLocation() - FVector(FieldFixture::Midway, 0.0, 0.0));
			Fields().Add(FVeyraMovementField{ BesidePath(), FieldFixture::Radius, EVeyraTeam::B, FieldFixture::Pull });
			FVeyraDisplacement Push;
			Push.Direction = FVector::ForwardVector;
			Push.Distance = FieldFixture::DashDistance;
			Push.Speed = FieldFixture::DashSpeed;
			ASSERT_THAT(IsTrue(VeyraCombat::Displace(*Enemy.GetAbilitySystemComponent(), *Mover->GetAbilitySystemComponent(), Push)));
			const TOptional<FVector> Lands = Mover->GetVeyraMovement()->GetForcedMoveDestination();
			ASSERT_THAT(IsTrue(Lands.IsSet() && Lands->Y - Mover->GetActorLocation().Y > FieldFixture::Pull / 4.0));
		}

		TEST_METHOD(ALingeringAreaHoldsItsFieldWhileItStands)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Caster = World.Spawn(EVeyraTeam::B, Mover->GetActorLocation() + FVector(0.0, -FieldFixture::Midway, 0.0));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(Caster, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_field")))));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(Caster, EVeyraAbilitySlot::E, BesidePath()) == EVeyraCastRejection::None));
			ASSERT_THAT(AreEqual(1, Fields().GetFieldCount(), TEXT("it stands its field")));
			ASSERT_THAT(IsTrue(DashAhead().Y - Mover->GetActorLocation().Y > FieldFixture::Pull / 4.0, TEXT("which bends its caster's enemy")));
			UWorld& WorldRef = Spawner.GetWorld();
			const double Until = WorldRef.GetTimeSeconds() + FieldFixture::Lasting + FieldFixture::Step;
			while (WorldRef.GetTimeSeconds() < Until)
			{
				WorldRef.Tick(LEVELTICK_TimeOnly, FieldFixture::Step);
				++GFrameCounter;
				WorldRef.GetTimerManager().Tick(FieldFixture::Step);
			}
			ASSERT_THAT(AreEqual(0, Fields().GetFieldCount(), TEXT("and its field goes with it")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
