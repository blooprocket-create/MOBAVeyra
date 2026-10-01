// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Algo/AllOf.h"
#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "EngineUtils.h"
#include "Fog/VeyraDenseFogBank.h"
#include "Rules/VeyraVisionRules.h"
#include "Targeting/VeyraTargeting.h"
#include "Targeting/VeyraVisibility.h"
#include "TimerManager.h"
#include "Tuning/VeyraVisionTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardCharacter.h"
#include "VeyraVisionSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVisionTests
{
	/** Fixture values for fog an ability lays: a bush's size, how long it lasts, and a test world's step. */
	namespace RuntimeFogFixture
	{
		constexpr double StartingMaxHealth = 100.0;
		constexpr double BushRadius = 300.0;
		constexpr double Step = 0.1;
		constexpr double Lasts = 1.0;
		constexpr double Inside = 100.0;
	}

	// Veyra.Vision.RuntimeFog.*: Dense Fog an ability lays, the same construct as the map's (ADR-036 §1).
	TEST_CLASS(RuntimeFog, "Veyra.Vision")
	{
		FActorTestSpawner Spawner;

		AVeyraVanguardCharacter& SpawnVanguard(EVeyraTeam Team, const FVector& Location)
		{
			AVeyraPlayerState& PlayerState = Spawner.SpawnActor<AVeyraPlayerState>();
			PlayerState.SetVeyraTeam(Team);
			VeyraCombat::InitializeVitals(*PlayerState.GetAbilitySystemComponent(), RuntimeFogFixture::StartingMaxHealth);
			AVeyraVanguardCharacter& Vanguard = Spawner.SpawnActorAt<AVeyraVanguardCharacter>(Location, FRotator::ZeroRotator);
			Vanguard.SetPlayerState(&PlayerState);
			return Vanguard;
		}

		UVeyraVisionSubsystem& Vision()
		{
			return *Spawner.GetWorld().GetSubsystem<UVeyraVisionSubsystem>();
		}

		static double SightRadius()
		{
			return UVeyraVisionTuningSubsystem::Get().Sight.Vanguard;
		}

		static FVeyraFogShape CircleAt(const FVector2D& Where, double Radius)
		{
			FVeyraFogShape Shape;
			Shape.Origin = FVector(Where, 0.0);
			Shape.Radius = Radius;
			return Shape;
		}

		int32 CountBanks()
		{
			int32 Count = 0;
			for (TActorIterator<AVeyraDenseFogBank> It(&Spawner.GetWorld()); It; ++It)
			{
				Count += IsValid(*It) ? 1 : 0;
			}
			return Count;
		}

		/** World time passes, and the timers due in it fire (ue-test-world-timers). */
		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, static_cast<float>(RuntimeFogFixture::Step));
				++GFrameCounter;
				World.GetTimerManager().Tick(static_cast<float>(RuntimeFogFixture::Step));
			}
		}

		TEST_METHOD(ACorridorIsOverlappingCirclesWithinItsEnds)
		{
			FVeyraFogShape Corridor;
			Corridor.Kind = EVeyraFogShapeKind::Corridor;
			Corridor.Origin = FVector(100.0, 0.0, 0.0);
			Corridor.Direction = FVector(0.0, 1.0, 0.0);
			Corridor.Length = 1000.0;
			Corridor.Width = 200.0;
			const TArray<FVeyraFogCircle> Circles = VeyraVisionRules::CirclesOf(Corridor);
			const double Radius = Corridor.Width / 2.0;
			ASSERT_THAT(IsTrue(Circles.Num() > 2));
			ASSERT_THAT(IsTrue(Circles[0].Center.Equals(FVector2D(100.0, Radius)) && Circles.Last().Center.Equals(FVector2D(100.0, Corridor.Length - Radius)),
				TEXT("the first and last within its ends")));
			for (int32 Index = 0; Index < Circles.Num(); ++Index)
			{
				ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Circles[Index].Radius, Radius)));
				ASSERT_THAT(IsTrue(Index == 0 || FVector2D::Distance(Circles[Index - 1].Center, Circles[Index].Center) <= Radius + UE_KINDA_SMALL_NUMBER,
					TEXT("each overlaps the next")));
			}
			const TArray<int32> Volumes = VeyraVisionRules::ConnectVolumes(Circles);
			ASSERT_THAT(IsTrue(Algo::AllOf(Volumes, [&Volumes](int32 Volume) { return Volume == Volumes[0]; }), TEXT("one volume")));

			// No longer than it is wide, it is one circle at its middle.
			Corridor.Length = Corridor.Width / 2.0;
			const TArray<FVeyraFogCircle> Short = VeyraVisionRules::CirclesOf(Corridor);
			ASSERT_THAT(IsTrue(Short.Num() == 1 && Short[0].Center.Equals(FVector2D(100.0, Corridor.Length / 2.0))));
		}

		TEST_METHOD(LaidFogHidesAsTheMapsFogDoes)
		{
			using namespace RuntimeFogFixture;
			const FVector2D Bush(SightRadius() * 2.0, 0.0);
			AVeyraVanguardCharacter& InFog = SpawnVanguard(EVeyraTeam::A, FVector(Bush.X - Inside, 0.0, 0.0));
			AVeyraVanguardCharacter& OutOfFog = SpawnVanguard(EVeyraTeam::A, FVector(Bush.X - BushRadius - Inside * 2.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Enemy = SpawnVanguard(EVeyraTeam::B, FVector(Bush.X + Inside, 0.0, 0.0));
			Vision().Start();
			ASSERT_THAT(IsTrue(VeyraTargeting::CanAcquire(&OutOfFog, Enemy), TEXT("no fog yet")));

			VeyraVisibility::AddDenseFog(Spawner.GetWorld(), CircleAt(Bush, BushRadius), Lasts);
			ASSERT_THAT(AreEqual(1, CountBanks(), TEXT("its bank, which every player receives")));
			ASSERT_THAT(IsTrue(VeyraTargeting::CanAcquire(&InFog, Enemy)));
			ASSERT_THAT(IsFalse(VeyraTargeting::CanAcquire(&OutOfFog, Enemy), TEXT("its teammate, outside the fog")));
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, Enemy), TEXT("team vision does not carry it")));
			ASSERT_THAT(IsTrue(VeyraVisibility::FogVolumeAt(&Spawner.GetWorld(), Enemy.GetActorLocation()) != INDEX_NONE));
			ASSERT_THAT(IsTrue(VeyraVisibility::FogVolumeAt(&Spawner.GetWorld(), OutOfFog.GetActorLocation()) == INDEX_NONE));

			// It ends at once: no lingering concealment, and its bank goes.
			Wait(Lasts + Step * 3.0);
			ASSERT_THAT(IsTrue(VeyraTargeting::CanAcquire(&OutOfFog, Enemy)));
			ASSERT_THAT(IsTrue(VeyraVisibility::FogVolumeAt(&Spawner.GetWorld(), Enemy.GetActorLocation()) == INDEX_NONE));
			ASSERT_THAT(AreEqual(0, CountBanks()));
		}

		TEST_METHOD(LaidFogJoinsTheMapsWhileItLasts)
		{
			// Overlapping fog is one volume while connected, and splits again at once as the laid fog ends (Vision Bible §2).
			using namespace RuntimeFogFixture;
			const FVector2D Bush(SightRadius() * 2.0, 0.0);
			const FVector2D Laid = Bush + FVector2D(BushRadius * 1.5, 0.0);
			AVeyraVanguardCharacter& Watcher = SpawnVanguard(EVeyraTeam::A, FVector(Laid + FVector2D(Inside, 0.0), 0.0));
			AVeyraVanguardCharacter& Enemy = SpawnVanguard(EVeyraTeam::B, FVector(Bush - FVector2D(Inside, 0.0), 0.0));
			Vision().Start();
			Vision().SetDenseFog({ FVeyraFogCircle{ Bush, BushRadius } });
			ASSERT_THAT(IsFalse(VeyraTargeting::CanAcquire(&Watcher, Enemy), TEXT("outside the map's fog")));

			VeyraVisibility::AddDenseFog(Spawner.GetWorld(), CircleAt(Laid, BushRadius), Lasts);
			ASSERT_THAT(IsTrue(VeyraVisibility::FogVolumeAt(&Spawner.GetWorld(), Watcher.GetActorLocation())
				== VeyraVisibility::FogVolumeAt(&Spawner.GetWorld(), Enemy.GetActorLocation()), TEXT("one volume")));
			ASSERT_THAT(IsTrue(VeyraTargeting::CanAcquire(&Watcher, Enemy), TEXT("inside the same fog")));
			ASSERT_THAT(AreEqual(Vision().GetDenseFog().Num(), 2));

			Wait(Lasts + Step * 3.0);
			ASSERT_THAT(IsFalse(VeyraTargeting::CanAcquire(&Watcher, Enemy), TEXT("split again")));
			ASSERT_THAT(AreEqual(Vision().GetDenseFog().Num(), 1, TEXT("the map's fog stays")));
		}

		TEST_METHOD(AWorldWithoutVisionLaysNoFog)
		{
			using namespace RuntimeFogFixture;
			VeyraVisibility::AddDenseFog(Spawner.GetWorld(), CircleAt(FVector2D::ZeroVector, BushRadius), Lasts);
			ASSERT_THAT(AreEqual(0, CountBanks()));
			ASSERT_THAT(IsTrue(VeyraVisibility::FogVolumeAt(&Spawner.GetWorld(), FVector::ZeroVector) == INDEX_NONE));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
