// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "EngineUtils.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Rules/VeyraVisionRules.h"
#include "State/VeyraVisionTeamState.h"
#include "Targeting/VeyraVisibility.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tools/VeyraVisionToolComponent.h"
#include "Tuning/VeyraVisionTuningSubsystem.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardCharacter.h"
#include "VeyraVisionSubsystem.h"
#include "Wards/VeyraWard.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVisionTests
{
	// Veyra.Vision.Tools.*: Sweeper's True Sight and outlines, Quick Sight's lit area, presence pings
	// from wards and lit areas in Dense Fog, and cooldowns kept across swaps (Vision Bible §4-§7;
	// ADR-016 §5, §6).
	TEST_CLASS(Tools, "Veyra.Vision")
	{
		FActorTestSpawner Spawner;
		/** On Team B: places the wards, and hides in the fog. */
		AVeyraVanguardCharacter* Enemy = nullptr;
		/** On Team A: uses the tools. */
		AVeyraVanguardCharacter* Scout = nullptr;
		AVeyraVanguardCharacter* ScoutsAlly = nullptr;

		BEFORE_EACH()
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			Enemy = &Spawn(World, EVeyraTeam::B, FVector::ZeroVector);
			Scout = &Spawn(World, EVeyraTeam::A, FVector(0.0, Far(), 0.0));
			ScoutsAlly = &Spawn(World, EVeyraTeam::A, FVector(0.0, -Far(), 0.0));
			Vision().Start();
		}

		static AVeyraVanguardCharacter& Spawn(VeyraAbilitiesTests::FArchetypeTestWorld& World, EVeyraTeam Team, const FVector& Location)
		{
			AVeyraVanguardCharacter& Vanguard = World.Spawn(Team, Location);
			Vanguard.GetPlayerState()->FindComponentByClass<UVeyraProgressionComponent>()->Initialize(FVeyraStatGrowth(), 0.0);
			return Vanguard;
		}

		/** Further than anything in this world sees or reaches. */
		static double Far()
		{
			const FVeyraVisionTuning& Tuning = UVeyraVisionTuningSubsystem::Get();
			return 10.0 * FMath::Max3(Tuning.Sight.Vanguard, Tuning.Sight.Ward, Tuning.QuickSight.Range);
		}

		static UVeyraVisionToolComponent& ToolOf(const AVeyraVanguardCharacter& Vanguard)
		{
			return *Vanguard.GetPlayerState()->FindComponentByClass<UVeyraVisionToolComponent>();
		}

		UVeyraVisionSubsystem& Vision()
		{
			return *Spawner.GetWorld().GetSubsystem<UVeyraVisionSubsystem>();
		}

		AVeyraWard* PlaceEnemyWard()
		{
			if (ToolOf(*Enemy).Use(Enemy->GetActorLocation()) != EVeyraVisionToolRejection::None)
			{
				return nullptr;
			}
			TActorIterator<AVeyraWard> It(&Spawner.GetWorld());
			return It ? *It : nullptr;
		}

		/** Moves world time on by at least Seconds, then works vision out: Vision's areas run on world time. */
		void Pass(double Seconds)
		{
			// Fixture value: a step below the longest frame the world accepts in one tick.
			constexpr float StepSeconds = 0.1f;
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, StepSeconds);
			}
			Vision().UpdateNow();
		}

		/** Just past Seconds: what lasts Seconds is over by then. */
		static double Past(double Seconds)
		{
			constexpr double JustPast = 0.1;
			return Seconds + JustPast;
		}

		TEST_METHOD(SweeperShowsItsWholeTeamAnEnemyWardForItsDuration)
		{
			const FVeyraSweeperTuning& Sweeper = UVeyraVisionTuningSubsystem::Get().Sweeper;
			AVeyraWard* Ward = PlaceEnemyWard();
			ASSERT_THAT(IsNotNull(Ward));
			Scout->SetActorLocation(Ward->GetActorLocation() + FVector(Sweeper.Radius / 2.0, 0.0, 0.0));
			Vision().UpdateNow();
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, *Ward), TEXT("ordinary sight never shows it")));

			ToolOf(*Scout).Equip(EVeyraVisionTool::Sweeper);
			ASSERT_THAT(IsTrue(ToolOf(*Scout).Use(Scout->GetActorLocation()) == EVeyraVisionToolRejection::None));
			Vision().UpdateNow();
			ASSERT_THAT(IsTrue(Vision().IsVisibleToTeam(EVeyraTeam::A, *Ward)));
			ASSERT_THAT(IsTrue(Vision().CanSee(*ScoutsAlly, *Ward), TEXT("its whole team, near or far, may target it")));
			ASSERT_THAT(IsTrue(ToolOf(*Scout).Use(Scout->GetActorLocation()) == EVeyraVisionToolRejection::CoolingDown));

			Pass(Past(Sweeper.DurationSeconds));
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, *Ward), TEXT("invisible again once it ends")));
		}

		TEST_METHOD(CooldownsSurviveSwapsAndPersistentWardComesBackFull)
		{
			const FVeyraVisionTuning& Tuning = UVeyraVisionTuningSubsystem::Get();
			UVeyraVisionToolComponent& Tool = ToolOf(*Scout);
			ASSERT_THAT(IsTrue(Tool.Use(Scout->GetActorLocation()) == EVeyraVisionToolRejection::None, TEXT("a ward, spending a charge")));
			Tool.Equip(EVeyraVisionTool::Sweeper);
			ASSERT_THAT(IsTrue(Tool.Use(Scout->GetActorLocation()) == EVeyraVisionToolRejection::None));
			Tool.Equip(EVeyraVisionTool::PersistentWard);
			ASSERT_THAT(IsTrue(Tool.GetWardCharges() == Tuning.WardCharges.Max, TEXT("equipped again, it comes with every charge")));
			Tool.Equip(EVeyraVisionTool::Sweeper);
			ASSERT_THAT(IsTrue(Tool.Use(Scout->GetActorLocation()) == EVeyraVisionToolRejection::CoolingDown, TEXT("a swap never refreshes a cooldown")));
			Pass(Past(Tuning.Sweeper.CooldownSeconds));
			ASSERT_THAT(IsTrue(Tool.Use(Scout->GetActorLocation()) == EVeyraVisionToolRejection::None));
		}

		TEST_METHOD(QuickSightLightsAPointWithinItsRangeForAMoment)
		{
			const FVeyraQuickSightTuning& QuickSight = UVeyraVisionTuningSubsystem::Get().QuickSight;
			// The enemy stands just within the tool's reach, far beyond the scout's own sight.
			Enemy->SetActorLocation(Scout->GetActorLocation() + FVector(QuickSight.Range, 0.0, 0.0));
			Vision().UpdateNow();
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, *Enemy)));

			UVeyraVisionToolComponent& Tool = ToolOf(*Scout);
			Tool.Equip(EVeyraVisionTool::QuickSight);
			// Aimed past its reach: brought back within it, onto the enemy.
			ASSERT_THAT(IsTrue(Tool.Use(Scout->GetActorLocation() + FVector(Far(), 0.0, 0.0)) == EVeyraVisionToolRejection::None));
			Vision().UpdateNow();
			ASSERT_THAT(IsTrue(Vision().IsVisibleToTeam(EVeyraTeam::A, *Enemy)));
			Pass(Past(QuickSight.DurationSeconds));
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, *Enemy), TEXT("only for a moment")));
		}

		TEST_METHOD(AWardInDenseFogPingsItsCircleWhenAnEnemyComesWithinItsSensor)
		{
			// The scout wards a patch of fog, then walks away; the enemy steps into the fog.
			const FVeyraVisionTuning& Tuning = UVeyraVisionTuningSubsystem::Get();
			const double FogRadius = Tuning.PersistentWard.SensorRadius * 3.0;
			const FVector2D Bush(Scout->GetActorLocation());
			Vision().SetDenseFog({ FVeyraFogCircle{ Bush, FogRadius } });
			ASSERT_THAT(IsTrue(ToolOf(*Scout).Use(Scout->GetActorLocation()) == EVeyraVisionToolRejection::None));
			Scout->SetActorLocation(FVector(Far(), Far(), 0.0));
			const AVeyraVisionTeamState* State = AVeyraVisionTeamState::Find(&Spawner.GetWorld(), EVeyraTeam::A);
			ASSERT_THAT(IsNotNull(State));

			// In the fog but beyond the ward's own coverage: no ping.
			Enemy->SetActorLocation(FVector(Bush.X + Tuning.PersistentWard.SensorRadius * 2.0, Bush.Y, 0.0));
			Vision().UpdateNow();
			ASSERT_THAT(IsTrue(State->GetPings().IsEmpty(), TEXT("its own coverage, not the whole fog")));

			Enemy->SetActorLocation(FVector(Bush.X + Tuning.PersistentWard.SensorRadius / 2.0, Bush.Y, 0.0));
			Vision().UpdateNow();
			ASSERT_THAT(AreEqual(State->GetPings().Num(), 1));
			ASSERT_THAT(IsTrue(State->GetPings()[0].Centre.Equals(Bush) && State->GetPings()[0].Radius == FogRadius, TEXT("the circle, not where it stands")));
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, *Enemy), TEXT("a sensor, not a camera")));
			const double FirstAt = State->GetPings()[0].At;
			Vision().UpdateNow();
			ASSERT_THAT(AreEqual(State->GetPings().Num(), 1, TEXT("at its cadence, not every pass")));
			Pass(Past(Tuning.Presence.PingEverySeconds));
			ASSERT_THAT(IsTrue(!State->GetPings().IsEmpty() && State->GetPings().Last().At > FirstAt, TEXT("again while it stays")));
		}

		TEST_METHOD(AnEnemyEnteringAWardsEmptiedCoveragePingsAtOnce)
		{
			// The enemy is sensed, steps out of the ward's coverage, and steps back in before the cadence.
			const FVeyraVisionTuning& Tuning = UVeyraVisionTuningSubsystem::Get();
			const FVector2D Bush(Scout->GetActorLocation());
			Vision().SetDenseFog({ FVeyraFogCircle{ Bush, Tuning.PersistentWard.SensorRadius * 3.0 } });
			ASSERT_THAT(IsTrue(ToolOf(*Scout).Use(Scout->GetActorLocation()) == EVeyraVisionToolRejection::None));
			Scout->SetActorLocation(FVector(Far(), Far(), 0.0));
			const AVeyraVisionTeamState* State = AVeyraVisionTeamState::Find(&Spawner.GetWorld(), EVeyraTeam::A);
			ASSERT_THAT(IsNotNull(State));
			const FVector Within(Bush.X + Tuning.PersistentWard.SensorRadius / 2.0, Bush.Y, 0.0);
			Enemy->SetActorLocation(Within);
			Vision().UpdateNow();
			ASSERT_THAT(AreEqual(State->GetPings().Num(), 1));

			Enemy->SetActorLocation(FVector(Bush.X + Tuning.PersistentWard.SensorRadius * 2.0, Bush.Y, 0.0));
			Vision().UpdateNow();
			ASSERT_THAT(AreEqual(State->GetPings().Num(), 1, TEXT("nothing while its coverage is empty")));
			Enemy->SetActorLocation(Within);
			Vision().UpdateNow();
			ASSERT_THAT(AreEqual(State->GetPings().Num(), 2, TEXT("a new entry pings at once, whatever the cadence")));
		}

		TEST_METHOD(SweeperOutlinesAnEnemyInFogWithoutRevealingIt)
		{
			const FVeyraVisionTuning& Tuning = UVeyraVisionTuningSubsystem::Get();
			const FVector2D Bush(Enemy->GetActorLocation());
			Vision().SetDenseFog({ FVeyraFogCircle{ Bush, Tuning.Sweeper.Radius / 2.0 } });
			// The scout stands outside the fog, the enemy within its Sweeper's reach.
			Scout->SetActorLocation(Enemy->GetActorLocation() + FVector(Tuning.Sweeper.Radius * 0.9, 0.0, 0.0));
			UVeyraVisionToolComponent& Tool = ToolOf(*Scout);
			Tool.Equip(EVeyraVisionTool::Sweeper);
			ASSERT_THAT(IsTrue(Tool.Use(Scout->GetActorLocation()) == EVeyraVisionToolRejection::None));
			Vision().UpdateNow();
			const AVeyraVisionTeamState* State = AVeyraVisionTeamState::Find(&Spawner.GetWorld(), EVeyraTeam::A);
			ASSERT_THAT(IsNotNull(State));
			ASSERT_THAT(AreEqual(State->GetOutlines().Num(), 1));
			ASSERT_THAT(IsTrue(FVector::Dist2D(State->GetOutlines()[0].Location, Enemy->GetActorLocation()) < 1.0));
			ASSERT_THAT(IsFalse(Vision().CanSee(*Scout, *Enemy), TEXT("an outline grants no targeting")));

			// Out of its reach, the outline stays where it was last covered, then fades.
			Enemy->SetActorLocation(Enemy->GetActorLocation() - FVector(Tuning.Sweeper.Radius * 0.2, 0.0, 0.0));
			const FVector LastCovered = State->GetOutlines()[0].Location;
			Scout->SetActorLocation(FVector(Far(), Far(), 0.0));
			Vision().UpdateNow();
			ASSERT_THAT(IsTrue(State->GetOutlines().Num() == 1 && State->GetOutlines()[0].Location.Equals(LastCovered), TEXT("it lingers, without tracking")));
			Pass(Past(Tuning.Sweeper.OutlineLingerSeconds));
			ASSERT_THAT(IsTrue(State->GetOutlines().IsEmpty()));
		}

		TEST_METHOD(ALitAreaIsVisionAndOverFogAPresenceSensor)
		{
			// As Bryn's Sounding Flare lights its point, through the contract every ability reads (ADR-016 §5).
			const FVeyraVisionTuning& Tuning = UVeyraVisionTuningSubsystem::Get();
			constexpr double LitRadius = 400.0;
			constexpr double LitSeconds = 3.0;
			const FVector Point = Enemy->GetActorLocation();
			VeyraVisibility::RevealArea(Spawner.GetWorld(), EVeyraTeam::A, Point, LitRadius, LitSeconds);
			Vision().UpdateNow();
			ASSERT_THAT(IsTrue(Vision().IsVisibleToTeam(EVeyraTeam::A, *Enemy), TEXT("outside fog, ordinary vision")));

			Vision().SetDenseFog({ FVeyraFogCircle{ FVector2D(Point), LitRadius } });
			Vision().UpdateNow();
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, *Enemy), TEXT("inside fog, never shown")));
			const AVeyraVisionTeamState* State = AVeyraVisionTeamState::Find(&Spawner.GetWorld(), EVeyraTeam::A);
			ASSERT_THAT(IsTrue(State && State->GetPings().Num() == 1, TEXT("but its presence is reported")));
			ASSERT_THAT(IsTrue(AVeyraVisionTeamState::Find(&Spawner.GetWorld(), EVeyraTeam::B)->GetPings().IsEmpty(), TEXT("to the side that lit it alone")));
			Pass(Past(LitSeconds + Tuning.Presence.PingEverySeconds));
			ASSERT_THAT(IsTrue(State->GetPings().Num() == 1, TEXT("no more once the light is gone")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
