// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Casting/VeyraCastStateComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CQTest.h"
#include "Movement/VeyraUnitCollision.h"
#include "Terrain/VeyraGround.h"
#include "VeyraVisionSubsystem.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Delivery/VeyraProjectile.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Life/VeyraLifeComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	// Veyra.Abilities.Projectile.*: skillshots and homing projectiles hit what they meet as they fly,
	// and dashes set off with their areas and hit the enemy they stop at (ADR-008 §3, §9; ADR-009 §4).
	// Flight is advanced by hand here; Veyra.Net.Skillshot runs it on the server's ticks.
	TEST_CLASS(Projectile, "Veyra.Abilities")
	{
		// Fixture values, independent of the committed Abilities.json.
		static constexpr double ShotSpeed = 1000.0;
		static constexpr double ShotRadius = 30.0;
		static constexpr double ShotRange = 1000.0;
		static constexpr double Damage = 40.0;
		static constexpr double PushDistance = 200.0;
		static constexpr double ForcedMoveSpeed = 1000.0;
		static constexpr double DashDistance = 300.0;
		static constexpr double ZoneRadius = 300.0;
		static constexpr double ZoneArc = 90.0;
		static constexpr double WallFaceX = 300.0;
		static constexpr double LongSeconds = 60.0;
		static constexpr double Tolerance = 2.0;

		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;

		static FVeyraEffectBundleTuning DamageOnly()
		{
			FVeyraEffectBundleTuning Effects;
			Effects.Damage.Add(FVeyraDamageTuning{ EVeyraDamageType::TrueDamage, { Damage }, 0.0, 0.0 });
			return Effects;
		}

		static FVeyraSkillshotAbilityTuning ShotWith(EVeyraSkillshotCollision Collision)
		{
			FVeyraSkillshotAbilityTuning Shot;
			Shot.Cast = InstantCast(ShotRange, LongSeconds, 0.0);
			Shot.Projectile = FVeyraProjectileTuning{ ShotSpeed, ShotRadius, ShotRange };
			Shot.Collision = Collision;
			Shot.Effects = DamageOnly();
			return Shot;
		}

		BEFORE_EACH()
		{
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_stun")), StatusOf(EVeyraStatusKind::Stun, 0.0, LongSeconds));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_haste")), StatusOf(EVeyraStatusKind::MoveSpeed, 0.2, LongSeconds));

			Tuning.Skillshot.Add(ArchetypeTestId(TEXT("test_spear")), ShotWith(EVeyraSkillshotCollision::FirstEnemy));
			Tuning.Skillshot.Add(ArchetypeTestId(TEXT("test_lance")), ShotWith(EVeyraSkillshotCollision::Pierce));
			FVeyraSkillshotAbilityTuning Hook = ShotWith(EVeyraSkillshotCollision::FirstEnemyVanguard);
			Hook.Effects.Displacement.Add(FVeyraDisplacementTuning{ EVeyraDisplacementDirection::TowardOrigin, ShotRange, ForcedMoveSpeed });
			Hook.PassThroughEffects.Displacement.Add(FVeyraDisplacementTuning{ EVeyraDisplacementDirection::AsideFromPath, PushDistance, ForcedMoveSpeed });
			Tuning.Skillshot.Add(ArchetypeTestId(TEXT("test_hook")), Hook);

			FVeyraDashAbilityTuning Charge;
			Charge.Cast = InstantCast(ShotRange, LongSeconds, 0.0);
			Charge.Direction = EVeyraDashDirection::TowardPoint;
			Charge.Distance = ShotRange;
			Charge.Speed = ForcedMoveSpeed;
			Charge.Contact = EVeyraDashContact::StopAtFirstEnemy;
			Charge.ContactEffects.Statuses.Add(ArchetypeTestId(TEXT("test_stun")));
			Charge.ContactSelfStatuses.Add(ArchetypeTestId(TEXT("test_haste")));
			Tuning.Dash.Add(ArchetypeTestId(TEXT("test_charge")), Charge);

			FVeyraDashAbilityTuning Recoil;
			Recoil.Cast = InstantCast(ShotRange, LongSeconds, 0.0);
			Recoil.Direction = EVeyraDashDirection::AwayFromPoint;
			Recoil.Distance = DashDistance;
			Recoil.Speed = ForcedMoveSpeed;
			FVeyraAreaZoneTuning& Blast = Recoil.StartZones.AddDefaulted_GetRef();
			Blast.Shape.Kind = EVeyraShapeKind::Sector;
			Blast.Shape.Radius = ZoneRadius;
			Blast.Shape.ArcDegrees = ZoneArc;
			Blast.Effects = DamageOnly();
			Tuning.Dash.Add(ArchetypeTestId(TEXT("test_recoil")), Recoil);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		/** Learns Ability in W and casts it along +X. */
		EVeyraCastRejection LearnAndCast(const TCHAR* Ability) const
		{
			if (!FArchetypeTestWorld::Learn(*Caster, EVeyraAbilitySlot::W, ArchetypeTestId(Ability)))
			{
				return EVeyraCastRejection::NotLearned;
			}
			return FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::W, FVector(ShotRange, 0.0, 0.0));
		}

		/** The projectile still flying, if any. */
		AVeyraProjectile* InFlight()
		{
			for (TActorIterator<AVeyraProjectile> It(&Spawner.GetWorld()); It; ++It)
			{
				if (!It->IsActorBeingDestroyed())
				{
					return *It;
				}
			}
			return nullptr;
		}

		/** Flies the projectile in flight for Seconds, in Steps equal steps. */
		void Fly(double Seconds, int32 Steps = 1)
		{
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				if (AVeyraProjectile* Shot = InFlight())
				{
					Shot->AdvanceBy(Seconds / Steps);
				}
			}
		}

		// Fixture values: a wall across the path, as thick and tall as needed to stop it.
		static constexpr double WallThickness = 50.0;
		static constexpr double WallWidth = 600.0;
		static constexpr double WallHeight = 400.0;

		/** A block of terrain of Size, centred on Center. */
		void SpawnTerrain(const FVector& Center, const FVector& Size)
		{
			UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			const FTransform Transform(FRotator::ZeroRotator, Center, Size / Cube->GetBoundingBox().GetSize());
			AStaticMeshActor* Wall = Spawner.GetWorld().SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
			Wall->SetMobility(EComponentMobility::Movable);
			Wall->GetStaticMeshComponent()->SetStaticMesh(Cube);
		}

		/** A wall across the path at WallFaceX, moved aside by OffsetY. */
		void SpawnWall(double OffsetY = 0.0)
		{
			SpawnTerrain(FVector(WallFaceX + WallThickness / 2.0, OffsetY, 0.0), FVector(WallThickness, WallWidth, WallHeight));
		}

		TEST_METHOD(ASkillshotFliesOverRisingGroundAtItsHeightAboveIt)
		{
			// Fixture values: the ground under the caster, and a terrace rising across the path (ADR-040 §4).
			constexpr double TerraceFromX = 400.0;
			constexpr double TerraceRise = 150.0;
			constexpr double SlabHalfThickness = 50.0;
			constexpr double SlabHalfWidth = 1000.0;
			constexpr double FlightShare = 0.7;
			const double Feet = Caster->GetActorLocation().Z - Caster->GetSimpleCollisionHalfHeight();
			const auto SpawnGround = [this](double FromX, double ToX, double TopZ) {
				AActor* Slab = Spawner.GetWorld().SpawnActor<AActor>();
				UBoxComponent* Box = NewObject<UBoxComponent>(Slab);
				Slab->SetRootComponent(Box);
				Box->SetBoxExtent(FVector((ToX - FromX) / 2.0, SlabHalfWidth, SlabHalfThickness));
				VeyraGround::MakeGround(*Box);
				VeyraUnitCollision::SetResponseToUnits(*Box, ECR_Ignore);
				Box->RegisterComponent();
				Slab->SetActorLocation(FVector((FromX + ToX) / 2.0, 0.0, TopZ - SlabHalfThickness));
			};
			SpawnGround(-ShotRange, TerraceFromX, Feet);
			SpawnGround(TerraceFromX, ShotRange * 2.0, Feet + TerraceRise);
			ASSERT_THAT(IsTrue(LearnAndCast(TEXT("test_spear")) == EVeyraCastRejection::None));
			const AVeyraProjectile* Shot = InFlight();
			ASSERT_THAT(IsTrue(Shot != nullptr));
			const double LaunchZ = Shot->GetActorLocation().Z;
			Fly(ShotRange * FlightShare / ShotSpeed);
			ASSERT_THAT(IsTrue(InFlight() == Shot, TEXT("rising ground does not stop it")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Shot->GetActorLocation().X, ShotRange * FlightShare, Tolerance)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Shot->GetActorLocation().Z, LaunchZ + TerraceRise, Tolerance),
				FString::Printf(TEXT("at Z %g, launched at %g"), Shot->GetActorLocation().Z, LaunchZ)));
		}

		TEST_METHOD(AFirstEnemySkillshotStopsAtTheNearestEnemy)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Near = World.Spawn(EVeyraTeam::B, FVector(300.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Far = World.Spawn(EVeyraTeam::B, FVector(600.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(LearnAndCast(TEXT("test_spear")) == EVeyraCastRejection::None));
			const AVeyraProjectile* Shot = InFlight();
			ASSERT_THAT(IsTrue(Shot && Shot->GetVeyraTeam() == EVeyraTeam::A && Shot->GetRadius() == ShotRadius));
			ASSERT_THAT(IsTrue(World.HealthLost(Near) == 0.0, TEXT("it hit before it flew")));
			Fly(ShotRange / ShotSpeed);
			ASSERT_THAT(IsTrue(World.HealthLost(Near) == Damage));
			ASSERT_THAT(IsTrue(World.HealthLost(Far) == 0.0));
			ASSERT_THAT(IsTrue(InFlight() == nullptr));
			// Its caster's cast state says where it ended, for clients' impacts: at the unit it struck (ADR-072 §4).
			ASSERT_THAT(IsTrue(LastEnd().Serial == 1 && LastEnd().Ability == ArchetypeTestId(TEXT("test_spear")) && LastEnd().CastId > 0));
			// Where its body met the unit's: short of the unit's centre by their two radii.
			const double Met = Near.GetActorLocation().X - Near.GetCapsuleComponent()->GetScaledCapsuleRadius() - ShotRadius;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(LastEnd().Location.X, Met, Tolerance), *FString::Printf(TEXT("ended at %.1f, met at %.1f"), LastEnd().Location.X, Met)));
			// Each end counts on, the latest one in its place.
			const int32 FirstCast = LastEnd().CastId;
			Near.SetActorLocation(FVector(ShotRange * 4.0, 0.0, 0.0));
			Far.SetActorLocation(FVector(ShotRange * 4.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Caster, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_lance")))));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::E, FVector(ShotRange, 0.0, 0.0)) == EVeyraCastRejection::None));
			Fly(ShotRange / ShotSpeed * 2.0);
			ASSERT_THAT(IsTrue(LastEnd().Serial == 2 && LastEnd().Ability == ArchetypeTestId(TEXT("test_lance")) && LastEnd().CastId != FirstCast));
		}

		TEST_METHOD(AHookPushesOtherUnitsAsideAndPullsTheFirstEnemyVanguard)
		{
			// Fixture value: the Fluxborn stands just to the left of the path, so it goes left.
			constexpr double OffPath = -10.0;
			FArchetypeTestWorld World{ Spawner };
			AVeyraTestFluxborn& Fluxborn = World.SpawnFluxborn(EVeyraTeam::B, FVector(300.0, OffPath, 0.0));
			AVeyraVanguardCharacter& Target = World.Spawn(EVeyraTeam::B, FVector(600.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(LearnAndCast(TEXT("test_hook")) == EVeyraCastRejection::None));
			Fly(ShotRange / ShotSpeed);

			const UVeyraMovementComponent* Pushed = Fluxborn.GetVeyraMovement();
			ASSERT_THAT(IsTrue(World.HealthLost(Fluxborn) == 0.0 && Pushed->IsDisplaced()));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Pushed->GetForcedMoveDestination()->Y, OffPath - PushDistance, Tolerance),
				FString::Printf(TEXT("pushed to Y %g"), Pushed->GetForcedMoveDestination()->Y)));

			const UVeyraMovementComponent* Pulled = Target.GetVeyraMovement();
			ASSERT_THAT(IsTrue(World.HealthLost(Target) == Damage && Pulled->IsDisplaced()));
			const double Touching = Caster->GetSimpleCollisionRadius() + Target.GetSimpleCollisionRadius();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Pulled->GetForcedMoveDestination()->X, Touching, Tolerance)));
			ASSERT_THAT(IsTrue(InFlight() == nullptr));
		}

		TEST_METHOD(APiercingSkillshotHitsEachEnemyOnce)
		{
			// Fixture value: small steps, so it meets each unit in more than one.
			constexpr int32 Steps = 20;
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& First = World.Spawn(EVeyraTeam::B, FVector(300.0, 0.0, 0.0));
			AVeyraTestFluxborn& Second = World.SpawnFluxborn(EVeyraTeam::B, FVector(600.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Friend = World.Spawn(EVeyraTeam::A, FVector(450.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(LearnAndCast(TEXT("test_lance")) == EVeyraCastRejection::None));
			Fly(ShotRange / ShotSpeed, Steps);
			ASSERT_THAT(IsTrue(World.HealthLost(First) == Damage && World.HealthLost(Second) == Damage));
			ASSERT_THAT(IsTrue(World.HealthLost(Friend) == 0.0));
			ASSERT_THAT(IsTrue(InFlight() == nullptr, TEXT("it should end at its range")));
		}

		TEST_METHOD(TerrainStopsASkillshot)
		{
			SpawnWall();
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Behind = World.Spawn(EVeyraTeam::B, FVector(WallFaceX * 2.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(LearnAndCast(TEXT("test_lance")) == EVeyraCastRejection::None));
			Fly(ShotRange / ShotSpeed);
			ASSERT_THAT(IsTrue(World.HealthLost(Behind) == 0.0));
			ASSERT_THAT(IsTrue(InFlight() == nullptr));
		}

		TEST_METHOD(TerrainStopsASkillshotWhoseBodyMeetsIt)
		{
			// The wall's edge is half the shot's radius beside the path: its centre line passes, its body does not.
			SpawnWall(-(WallWidth / 2.0 + ShotRadius / 2.0));
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Behind = World.Spawn(EVeyraTeam::B, FVector(WallFaceX * 2.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(LearnAndCast(TEXT("test_lance")) == EVeyraCastRejection::None));
			Fly(ShotRange / ShotSpeed);
			ASSERT_THAT(IsTrue(World.HealthLost(Behind) == 0.0));
			ASSERT_THAT(IsTrue(InFlight() == nullptr));
		}

		TEST_METHOD(ASkillshotCastAlongAWallItStartsAgainstFlies)
		{
			// A wall along the path up to WallFaceX, overlapping the shot's body from its launch but not its centre line.
			SpawnTerrain(FVector(WallFaceX - ShotRange / 2.0, -(ShotRadius / 2.0 + WallThickness / 2.0), 0.0), FVector(ShotRange, WallThickness, WallHeight));
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Ahead = World.Spawn(EVeyraTeam::B, FVector(WallFaceX * 2.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(LearnAndCast(TEXT("test_spear")) == EVeyraCastRejection::None));
			Fly(ShotRange / ShotSpeed);
			ASSERT_THAT(IsTrue(World.HealthLost(Ahead) == Damage));
		}

		TEST_METHOD(ASkillshotAlongAWallStillStopsAtTerrainItsBodyMeetsAhead)
		{
			// Fixture values: past the wall the shot starts against, a block whose edge is half the
			// shot's radius beside the path, which its body meets but its centre line passes.
			constexpr double BlockStartX = WallFaceX + 150.0;
			constexpr double BlockLength = 50.0;
			SpawnTerrain(FVector(WallFaceX - ShotRange / 2.0, -(ShotRadius / 2.0 + WallThickness / 2.0), 0.0), FVector(ShotRange, WallThickness, WallHeight));
			SpawnTerrain(FVector(BlockStartX + BlockLength / 2.0, -(ShotRadius / 2.0 + WallThickness / 2.0), 0.0), FVector(BlockLength, WallThickness, WallHeight));
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Behind = World.Spawn(EVeyraTeam::B, FVector(WallFaceX * 2.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(LearnAndCast(TEXT("test_spear")) == EVeyraCastRejection::None));
			Fly(ShotRange / ShotSpeed);
			ASSERT_THAT(IsTrue(World.HealthLost(Behind) == 0.0));
			ASSERT_THAT(IsTrue(InFlight() == nullptr));
		}

		TEST_METHOD(ASkillshotEndsAtItsRange)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Beyond = World.Spawn(EVeyraTeam::B, FVector(ShotRange * 2.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(LearnAndCast(TEXT("test_spear")) == EVeyraCastRejection::None));
			Fly(ShotRange / ShotSpeed / 2.0);
			const AVeyraProjectile* Shot = InFlight();
			ASSERT_THAT(IsTrue(Shot && FMath::IsNearlyEqual(Shot->GetActorLocation().X, ShotRange / 2.0, Tolerance)));
			// Clients draw it from its launch data and the server's clock, and it stops at its range.
			const double Halfway = Shot->GetLaunchedAt() + ShotRange / ShotSpeed / 2.0;
			ASSERT_THAT(IsTrue(FVector::Dist(Shot->GetLineLocationAt(Halfway), Shot->GetActorLocation()) <= Tolerance));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Shot->GetLineLocationAt(Halfway + LongSeconds).X, ShotRange, Tolerance)));
			Fly(ShotRange / ShotSpeed);
			ASSERT_THAT(IsTrue(InFlight() == nullptr));
			ASSERT_THAT(IsTrue(World.HealthLost(Beyond) == 0.0));
			ASSERT_THAT(IsTrue(LastEnd().Serial == 1 && FMath::IsNearlyEqual(LastEnd().Location.X, ShotRange, Tolerance), TEXT("it ended at its range")));
		}

		/** Where the caster's cast state says its latest cast projectile ended (ADR-072 §4). */
		const FVeyraProjectileEnd& LastEnd() const
		{
			const UVeyraCastStateComponent* Casts = Caster->GetAbilitySystemComponent()->GetOwner()->FindComponentByClass<UVeyraCastStateComponent>();
			check(Casts);
			return Casts->GetLastProjectileEnd();
		}

		/** A homing projectile from the caster after Target, dealing the fixture damage. */
		AVeyraProjectile& LaunchHomingAt(AActor& Target)
		{
			UAbilitySystemComponent& Source = *Caster->GetAbilitySystemComponent();
			AVeyraProjectile* Shot = Spawner.GetWorld().SpawnActor<AVeyraProjectile>(AVeyraProjectile::StaticClass(), FTransform(Caster->GetActorLocation()));
			Shot->LaunchHoming(Source, Target, ShotSpeed, ShotRadius, VeyraEffectDelivery::Prepare(Source, DamageOnly(), 1), ArchetypeTestId(TEXT("test_bolt")), 0);
			return *Shot;
		}

		TEST_METHOD(AHomingProjectileReachesItsTargetPastTerrain)
		{
			SpawnWall();
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Target = World.Spawn(EVeyraTeam::B, FVector(WallFaceX * 2.0, 0.0, 0.0));
			LaunchHomingAt(Target);
			ASSERT_THAT(IsTrue(InFlight() && InFlight()->GetHomingTarget() == &Target));
			Fly(ShotRange / ShotSpeed);
			ASSERT_THAT(IsTrue(World.HealthLost(Target) == Damage));
			ASSERT_THAT(IsTrue(InFlight() == nullptr));
			ASSERT_THAT(IsTrue(LastEnd().Serial == 0, TEXT("one launched by no cast, as a basic attack is, says nothing of its end")));
		}

		TEST_METHOD(AHomingProjectileKeepsGoingAfterItsTargetVanishes)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Target = World.Spawn(EVeyraTeam::B, FVector(ShotRange / 2.0, 0.0, 0.0));
			UVeyraVisionSubsystem& Vision = *Spawner.GetWorld().GetSubsystem<UVeyraVisionSubsystem>();
			Vision.Start();
			LaunchHomingAt(Target);
			ASSERT_THAT(IsTrue(VeyraCombatTests::Camouflage(Target, ShotRadius)));
			Vision.UpdateNow();
			ASSERT_THAT(IsFalse(Vision.IsVisibleToTeam(EVeyraTeam::A, Target)));
			// A Camouflaged unit is not Untargetable: what was already launched still arrives (ADR-018 §4).
			Fly(ShotRange / ShotSpeed);
			ASSERT_THAT(IsTrue(World.HealthLost(Target) == Damage));
		}

		TEST_METHOD(AHomingProjectileFizzlesIfItsTargetDies)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Target = World.Spawn(EVeyraTeam::B, FVector(ShotRange / 2.0, 0.0, 0.0));
			LaunchHomingAt(Target);
			Target.GetPlayerState()->FindComponentByClass<UVeyraLifeComponent>()->SetState(EVeyraLifeState::Dead);
			Fly(ShotRange / ShotSpeed);
			ASSERT_THAT(IsTrue(World.HealthLost(Target) == 0.0));
			ASSERT_THAT(IsTrue(InFlight() == nullptr));
		}

		TEST_METHOD(ADashHitsItsStartZonesAndSetsOff)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& InFront = World.Spawn(EVeyraTeam::B, FVector(ZoneRadius / 2.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Behind = World.Spawn(EVeyraTeam::B, FVector(-ZoneRadius / 2.0, 0.0, 0.0));
			const FVector Start = Caster->GetActorLocation();
			ASSERT_THAT(IsTrue(LearnAndCast(TEXT("test_recoil")) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(World.HealthLost(InFront) == Damage));
			ASSERT_THAT(IsTrue(World.HealthLost(Behind) == 0.0, TEXT("the blast faces the point")));
			const UVeyraMovementComponent* Movement = Caster->GetVeyraMovement();
			ASSERT_THAT(IsTrue(Movement->IsDashing()));
			ASSERT_THAT(IsTrue(FVector::Dist(Movement->GetForcedMoveDestination().GetValue(), Start - FVector(DashDistance, 0.0, 0.0)) <= Tolerance,
				TEXT("the recoil goes straight back from the point")));
		}

		TEST_METHOD(ARootedCasterCannotDash)
		{
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Caster, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_charge")))));
			FVeyraStatusSpec Root;
			Root.Id = ArchetypeTestId(TEXT("test_root"));
			Root.Kind = EVeyraStatusKind::Root;
			Root.DurationSeconds = 60.0;
			UAbilitySystemComponent& Self = *Caster->GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Self, Self, Root)));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::W, FVector(ShotRange, 0.0, 0.0)) == EVeyraCastRejection::CrowdControlled,
				TEXT("rooted, it cannot dash (ADR-026 §3)")));
			ASSERT_THAT(IsTrue(VeyraCombat::RemoveStatus(Self, Root.Id)));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::W, FVector(ShotRange, 0.0, 0.0)) == EVeyraCastRejection::None));
		}

		TEST_METHOD(AChargeThatStopsAtAnEnemyHitsIt)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(ShotRange / 2.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(LearnAndCast(TEXT("test_charge")) == EVeyraCastRejection::None));
			UVeyraMovementComponent* Movement = Caster->GetVeyraMovement();
			ASSERT_THAT(IsTrue(Movement->IsDashing()));
			ASSERT_THAT(IsFalse(World.Has(Enemy, TEXT("test_stun")), TEXT("nothing lands before the contact")));

			// The movement reports the contact as its dash reaches the enemy (Veyra.Net.Skillshot runs it for real).
			FVeyraDashEnd Contact;
			Contact.Reason = EVeyraDashEndReason::EnemyContact;
			Contact.Contact = &Enemy;
			Movement->OnDashEnded.Broadcast(Contact);
			ASSERT_THAT(IsTrue(World.Has(Enemy, TEXT("test_stun"))));
			ASSERT_THAT(IsTrue(World.Has(*Caster, TEXT("test_haste"))));
		}

		TEST_METHOD(OnlyAVanguardSeekingSkillshotPassesThroughUnits)
		{
			constexpr int32 RankCounts[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, RankCounts).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, RankCounts), TEXT(" | "))));
			FVeyraAbilitiesTuning Broken = Tuning;
			Broken.Skillshot.FindChecked(ArchetypeTestId(TEXT("test_spear"))).PassThroughEffects = DamageOnly();
			const TArray<FString> Problems = VeyraAbilityRules::Validate(Broken, RankCounts);
			ASSERT_THAT(IsTrue(Problems.ContainsByPredicate([](const FString& Problem) { return Problem.StartsWith(TEXT("/skillshot/test_spear/passThroughEffects:")); }),
				FString::Join(Problems, TEXT(" | "))));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
