// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Terrain/VeyraRuntimeTerrain.h"
#include "Terrain/VeyraTerrainSubsystem.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraWorldTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	// Veyra.World.RuntimeTerrain.*: walls the battleground raises for abilities (Battleground Bible §2,
	// "Ability-created terrain"; ADR-032 §4).
	TEST_CLASS(RuntimeTerrain, "Veyra.World")
	{
		// Fixture values: a wall's size, where it stands, and a dash toward it.
		static constexpr double Length = 400.0;
		static constexpr double Thickness = 80.0;
		static constexpr double HalfHeight = 88.0;
		static constexpr double Ahead = 300.0;
		static constexpr double DashDistance = 600.0;
		static constexpr double DashSpeed = 1200.0;
		static constexpr double Tolerance = 1.0;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Unit = nullptr;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Unit = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			ASSERT_THAT(IsNotNull(Spawner.GetWorld().GetSubsystem<UVeyraTerrainSubsystem>(), TEXT("the battleground's terrain governs every world")));
		}

		FVeyraWallRequest WallAt(const FVector& Centre) const
		{
			FVeyraWallRequest Wall;
			Wall.Centre = Centre;
			Wall.Facing = FVector::ForwardVector;
			Wall.Length = Length;
			Wall.Thickness = Thickness;
			Wall.HalfHeight = HalfHeight;
			return Wall;
		}

		TOptional<FVector> DashForward() const
		{
			FVeyraDash Dash;
			Dash.Direction = FVector::ForwardVector;
			Dash.Distance = DashDistance;
			Dash.Speed = DashSpeed;
			if (!VeyraCombat::Dash(*Unit->GetAbilitySystemComponent(), Dash))
			{
				return {};
			}
			const TOptional<FVector> Lands = Unit->GetVeyraMovement()->GetForcedMoveDestination();
			// Back where it was, for the next dash.
			VeyraCombat::Blink(*Unit->GetAbilitySystemComponent(), FVector::ZeroVector);
			return Lands;
		}

		TEST_METHOD(AWallMovesWhatStandsWhereItFormsOutUnharmedAndUnannounced)
		{
			int32 Moves = 0;
			Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnUnitMoved.AddLambda([&Moves](const FVeyraUnitMovedEvent&) { ++Moves; });
			const FVector Stood = Unit->GetActorLocation();
			ASSERT_THAT(IsTrue(VeyraRuntimeTerrain::RaiseWall(Spawner.GetWorld(), WallAt(Stood + FVector(Thickness / 4.0, 0.0, 0.0))) != 0));
			float Radius = 0.0f;
			float BodyHalfHeight = 0.0f;
			Unit->GetSimpleCollisionCylinder(Radius, BodyHalfHeight);
			const double Gap = Stood.X + Thickness / 4.0 - Unit->GetActorLocation().X;
			ASSERT_THAT(IsTrue(Gap >= Thickness / 2.0 + Radius - Tolerance, *FString::Printf(TEXT("out the side its centre was on: %g"), Gap)));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(*Unit) == 0.0, TEXT("no hit")));
			ASSERT_THAT(AreEqual(0, Moves, TEXT("and not its own move")));
		}

		TEST_METHOD(AWallStopsADashUntilItIsLowered)
		{
			const int32 Handle = VeyraRuntimeTerrain::RaiseWall(Spawner.GetWorld(), WallAt(Unit->GetActorLocation() + FVector(Ahead, 0.0, 0.0)));
			ASSERT_THAT(IsTrue(Handle != 0));
			const TOptional<FVector> Stopped = DashForward();
			ASSERT_THAT(IsTrue(Stopped.IsSet() && Stopped->X < Ahead - Thickness / 2.0, TEXT("terrain stops it")));
			VeyraRuntimeTerrain::LowerWall(Spawner.GetWorld(), Handle);
			ASSERT_THAT(AreEqual(0, Spawner.GetWorld().GetSubsystem<UVeyraTerrainSubsystem>()->GetWallCount()));
			const TOptional<FVector> Free = DashForward();
			ASSERT_THAT(IsTrue(Free.IsSet() && FMath::IsNearlyEqual(Free->X, DashDistance, Tolerance), TEXT("and once lowered it is gone")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
