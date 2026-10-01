// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Delivery/VeyraProjectile.h"
#include "Entities/VeyraPlacedMarker.h"
#include "EngineUtils.h"
#include "Loadout/VeyraFollowUpSubsystem.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	/** Fixture values for a wave that leaves a wall, and the follow-up that detonates it. */
	namespace WallFixture
	{
		constexpr double Range = 600.0;
		constexpr double ShotSpeed = 1000.0;
		constexpr double ShotRadius = 50.0;
		constexpr double Length = 400.0;
		constexpr double Thickness = 80.0;
		constexpr double Lifetime = 4.0;
		constexpr double Arming = 1.0;
		constexpr double Window = 5.0;
		constexpr double Blast = 300.0;
		constexpr double Blow = 50.0;
		constexpr double Beyond = 300.0;
		constexpr double DashSpeed = 1200.0;
		constexpr double DashDistance = 600.0;
		constexpr double Tolerance = 1.0;
		constexpr float Step = 0.1f;

		inline FVeyraEffectBundleTuning TrueDamage(double Amount)
		{
			FVeyraEffectBundleTuning Effects;
			FVeyraDamageTuning& Damage = Effects.Damage.AddDefaulted_GetRef();
			Damage.Type = EVeyraDamageType::TrueDamage;
			Damage.AmountByRank = { Amount };
			return Effects;
		}
	}

	// Veyra.Abilities.Walls.*: a skillshot that leaves a wall where its flight ends, which blocks as terrain
	// does; a follow-up that arms and ends with the wall; an area at the caster's marker (ADR-032 §4–§6).
	TEST_CLASS(Walls, "Veyra.Abilities")
	{
		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;

		BEFORE_EACH()
		{
			using namespace WallFixture;
			FVeyraSkillshotAbilityTuning Divide;
			Divide.Cast = InstantCast(0.0, 0.0, 0.0);
			FVeyraRecastTuning& Recast = Divide.Cast.RecastWindow.AddDefaulted_GetRef();
			Recast.Ability = ArchetypeTestId(TEXT("test_shatter"));
			Recast.WindowSeconds = Window;
			Recast.ArmingSeconds = Arming;
			Divide.Projectile.Speed = ShotSpeed;
			Divide.Projectile.Radius = ShotRadius;
			Divide.Projectile.Range = Range;
			Divide.Collision = EVeyraSkillshotCollision::Pierce;
			Divide.EndWall.Add(FVeyraWallTuning{ Length, Thickness, Lifetime });
			Tuning.Skillshot.Add(ArchetypeTestId(TEXT("test_divide")), Divide);
			FVeyraAreaAbilityTuning Shatter;
			Shatter.Cast = InstantCast(0.0, 0.0, 0.0);
			Shatter.Origin = EVeyraAreaOrigin::CastersMarker;
			Shatter.OriginAbility = { ArchetypeTestId(TEXT("test_divide")) };
			FVeyraAreaZoneTuning& Zone = Shatter.Zones.AddDefaulted_GetRef();
			Zone.Shape = CircleOf(Blast);
			Zone.Effects = TrueDamage(Blow);
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_shatter")), Shatter);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);
			const int32 Ranks[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, Ranks).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, Ranks), TEXT(" | "))));

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Caster, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_divide")))));
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		FVector Ahead(double Distance) const
		{
			return Caster->GetActorLocation() + FVector(Distance, 0.0, 0.0);
		}

		/** Casts the wave ahead and flies it to the end of its range. */
		void Raise()
		{
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::Q, Ahead(WallFixture::Range)) == EVeyraCastRejection::None));
			for (TActorIterator<AVeyraProjectile> It(&Spawner.GetWorld()); It; ++It)
			{
				if (!It->IsActorBeingDestroyed())
				{
					It->AdvanceBy(WallFixture::Range / WallFixture::ShotSpeed + WallFixture::Step);
				}
			}
		}

		AVeyraPlacedMarker* Wall() const
		{
			return AVeyraPlacedMarker::FindStanding(*Caster->GetAbilitySystemComponent(), ArchetypeTestId(TEXT("test_divide")));
		}

		FVeyraContentId InQ() const
		{
			const FVeyraLoadoutEntry* Entry = Caster->GetPlayerState()->FindComponentByClass<UVeyraAbilityLoadoutComponent>()->FindSlot(EVeyraAbilitySlot::Q);
			return Entry ? Entry->Ability : FVeyraContentId();
		}

		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, WallFixture::Step);
				++GFrameCounter;
				World.GetTimerManager().Tick(WallFixture::Step);
			}
		}

		TEST_METHOD(AShotLeavesItsWallAcrossItsPathWhereItsFlightEnds)
		{
			Raise();
			const AVeyraPlacedMarker* Standing = Wall();
			ASSERT_THAT(IsNotNull(Standing, TEXT("a wall stands")));
			ASSERT_THAT(IsTrue(Standing->IsWall()));
			ASSERT_THAT(IsTrue(FVector::Dist2D(Standing->GetActorLocation(), Ahead(WallFixture::Range)) < WallFixture::Tolerance, TEXT("where the flight ended")));
			ASSERT_THAT(IsTrue(Standing->GetActorForwardVector().Equals(FVector::ForwardVector, 1e-3), TEXT("facing along the path")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Standing->GetWallSize().X, static_cast<float>(WallFixture::Length))
				&& FMath::IsNearlyEqual(Standing->GetWallSize().Y, static_cast<float>(WallFixture::Thickness))));
		}

		TEST_METHOD(TheWallStopsADashAsTerrainDoes)
		{
			Raise();
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, Ahead(WallFixture::Range + WallFixture::Beyond));
			FVeyraDash Dash;
			Dash.Direction = FVector::BackwardVector;
			Dash.Distance = WallFixture::DashDistance;
			Dash.Speed = WallFixture::DashSpeed;
			ASSERT_THAT(IsTrue(VeyraCombat::Dash(*Enemy.GetAbilitySystemComponent(), Dash)));
			const TOptional<FVector> Lands = Enemy.GetVeyraMovement()->GetForcedMoveDestination();
			ASSERT_THAT(IsTrue(Lands.IsSet()));
			ASSERT_THAT(IsTrue(Lands->X > Ahead(WallFixture::Range).X + WallFixture::Thickness / 2.0,
				*FString::Printf(TEXT("its path ends on its own side of the wall, at %g"), Lands->X)));
		}

		TEST_METHOD(TheFollowUpArmsThenDetonatesTheWall)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, Ahead(WallFixture::Range + WallFixture::Blast / 2.0) + FVector(0.0, WallFixture::Length, 0.0));
			Raise();
			ASSERT_THAT(IsTrue(InQ() == ArchetypeTestId(TEXT("test_divide")), TEXT("not yet armed")));
			Wait(WallFixture::Arming + WallFixture::Step);
			ASSERT_THAT(IsTrue(InQ() == ArchetypeTestId(TEXT("test_shatter")), TEXT("armed")));
			const double Before = FArchetypeTestWorld::HealthLost(Enemy);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::Q, Caster->GetActorLocation()) == EVeyraCastRejection::None));
			ASSERT_THAT(IsNull(Wall(), TEXT("the wall goes")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(Enemy) == Before, TEXT("an enemy out of the blast is spared")));
		}

		TEST_METHOD(TheBlastLandsAroundTheWall)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, Ahead(WallFixture::Range + WallFixture::Blast / 2.0));
			Raise();
			Wait(WallFixture::Arming + WallFixture::Step);
			const double Before = FArchetypeTestWorld::HealthLost(Enemy);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::Q, Caster->GetActorLocation()) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FArchetypeTestWorld::HealthLost(Enemy) - Before, WallFixture::Blow), TEXT("the blast hits beside the wall, far from the caster")));
			ASSERT_THAT(IsTrue(InQ() == ArchetypeTestId(TEXT("test_divide")), TEXT("the follow-up is spent")));
		}

		TEST_METHOD(TheFollowUpEndsWithItsWall)
		{
			Raise();
			Wait(WallFixture::Arming + WallFixture::Step);
			ASSERT_THAT(IsTrue(InQ() == ArchetypeTestId(TEXT("test_shatter"))));
			Wait(WallFixture::Lifetime);
			ASSERT_THAT(IsNull(Wall(), TEXT("its time ran out")));
			ASSERT_THAT(IsTrue(InQ() == ArchetypeTestId(TEXT("test_divide")), TEXT("and the follow-up went with it")));
		}

		TEST_METHOD(AnAreaAtTheCastersMarkerIsRefusedWithoutOne)
		{
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Caster, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_shatter")))));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::E, Caster->GetActorLocation()) == EVeyraCastRejection::InvalidLocation));
		}

		TEST_METHOD(ValidationKeepsWallsArmingAndMarkerOriginsInShape)
		{
			FVeyraAbilitiesTuning Broken = Tuning;
			FVeyraSkillshotAbilityTuning& Divide = Broken.Skillshot.FindChecked(ArchetypeTestId(TEXT("test_divide")));
			Divide.EndWall[0].Length = 0.0;
			Divide.Cast.RecastWindow[0].OpensWhen = EVeyraRecastCondition::TargetHeld;
			Divide.Cast.RecastWindow[0].HeldStatus = {};
			Broken.Area.FindChecked(ArchetypeTestId(TEXT("test_shatter"))).OriginAbility = { ArchetypeTestId(TEXT("test_shatter")) };
			const int32 Ranks[] = { 5, 3 };
			const FString Problems = FString::Join(VeyraAbilityRules::Validate(Broken, Ranks), TEXT(" | "));
			ASSERT_THAT(IsTrue(Problems.Contains(TEXT("/endWall")), *Problems));
			ASSERT_THAT(IsTrue(Problems.Contains(TEXT("/armingSeconds")), *Problems));
			ASSERT_THAT(IsTrue(Problems.Contains(TEXT("/originAbility")), *Problems));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
