// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Delivery/VeyraProjectile.h"
#include "Delivery/VeyraVolleySubsystem.h"
#include "EngineUtils.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	// Veyra.Abilities.Volley.*: a lane its caster fires into shot by shot (ADR-018 §6), as Kade's Kill
	// Corridor.
	TEST_CLASS(Volley, "Veyra.Abilities")
	{
		// Fixture values, independent of the committed Abilities.json.
		static constexpr double LaneRange = 2000.0;
		static constexpr double ShotInterval = 1.0;
		static constexpr double LaneSeconds = 10.0;
		static constexpr double HalfAngle = 20.0;
		static constexpr int32 Shots = 3;
		static constexpr int32 BonusShots = 2;
		static constexpr double LongSeconds = 60.0;
		static constexpr double Push = 100.0;
		static constexpr float WorldStep = 0.1f;

		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;
		UVeyraAbilityLoadoutComponent* Loadout = nullptr;
		UVeyraVolleySubsystem* Volleys = nullptr;

		BEFORE_EACH()
		{
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_stance")), StatusOf(EVeyraStatusKind::Planted, 0.0, LongSeconds));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_mark")), StatusOf(EVeyraStatusKind::SourceAttackRange, Push, LongSeconds));

			FVeyraSkillshotAbilityTuning Shot;
			Shot.Cast = InstantCast(LaneRange, ShotInterval, 0.0);
			Shot.Projectile.Speed = LaneRange;
			Shot.Projectile.Radius = Push / 2.0;
			Shot.Projectile.Range = LaneRange;
			Shot.Collision = EVeyraSkillshotCollision::Pierce;
			Tuning.Skillshot.Add(ArchetypeTestId(TEXT("test_shot")), Shot);

			FVeyraVolleyAbilityTuning Corridor;
			Corridor.Cast = InstantCast(LaneRange, LongSeconds, 0.0);
			Corridor.Shot = ArchetypeTestId(TEXT("test_shot"));
			Corridor.Shots = Shots;
			Corridor.LaneHalfAngleDegrees = HalfAngle;
			Corridor.DurationSeconds = LaneSeconds;
			Corridor.CasterStatuses.Add(ArchetypeTestId(TEXT("test_stance")));
			FVeyraVolleyBonusTuning& Bonus = Corridor.Bonus.AddDefaulted_GetRef();
			Bonus.Status = ArchetypeTestId(TEXT("test_mark"));
			Bonus.MaxShots = BonusShots;
			Tuning.Volley.Add(ArchetypeTestId(TEXT("test_corridor")), Corridor);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_corridor")))));
			Loadout = Caster->GetPlayerState()->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
			Volleys = Spawner.GetWorld().GetSubsystem<UVeyraVolleySubsystem>();
			ASSERT_THAT(IsNotNull(Volleys));
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		/** Moves world time on by Seconds in small steps, timers included. */
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

		EVeyraCastRejection FireToward(const FVector& Direction) const
		{
			return FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::Q, Direction * (LaneRange / 2.0));
		}

		bool Holds(const TCHAR* Ability) const
		{
			const FVeyraLoadoutEntry* Entry = Loadout->FindSlot(EVeyraAbilitySlot::Q);
			return Entry && Entry->Ability == ArchetypeTestId(Ability);
		}

		int32 ShotsLeft() const
		{
			return Volleys->GetShotsLeft(*Caster->GetAbilitySystemComponent());
		}

		bool Planted() const
		{
			return FArchetypeTestWorld::Has(*Caster, TEXT("test_stance"));
		}

		static bool PushBy(AActor& From, AActor& Whom)
		{
			UAbilitySystemComponent* Source = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&From);
			UAbilitySystemComponent* Target = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Whom);
			return Source && Target && VeyraCombat::Displace(*Source, *Target, FVeyraDisplacement{ FVector::RightVector, Push, Push * 10.0 });
		}

		TEST_METHOD(ALaneHoldsItsShotPlantsItsCasterAndClosesOnceItsShotsAreSpent)
		{
			ASSERT_THAT(IsTrue(FireToward(FVector::ForwardVector) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Holds(TEXT("test_shot")) && Planted() && ShotsLeft() == Shots));
			ASSERT_THAT(IsTrue(EnumHasAnyFlags(VeyraCombat::GetActionBlocks(*Caster->GetAbilitySystemComponent()), EVeyraActionBlocks::Move), TEXT("the stance plants it")));
			for (int32 Shot = 0; Shot < Shots; ++Shot)
			{
				ASSERT_THAT(IsTrue(FireToward(FVector::ForwardVector) == EVeyraCastRejection::None, FString::Printf(TEXT("shot %d"), Shot + 1)));
				if (Shot + 1 < Shots)
				{
					ASSERT_THAT(IsTrue(FireToward(FVector::ForwardVector) == EVeyraCastRejection::OnCooldown, TEXT("each waits out the one before")));
					AdvanceWorld(ShotInterval + WorldStep);
				}
			}
			ASSERT_THAT(IsTrue(Holds(TEXT("test_corridor")) && !Planted() && ShotsLeft() == 0, TEXT("spent, the lane closes")));
		}

		TEST_METHOD(AShotAimedOutsideTheLaneFliesAlongItsEdge)
		{
			ASSERT_THAT(IsTrue(FireToward(FVector::ForwardVector) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FireToward(FVector::RightVector) == EVeyraCastRejection::None));
			TActorIterator<AVeyraProjectile> Shot(&Spawner.GetWorld());
			ASSERT_THAT(IsTrue(Shot && FMath::IsNearlyEqual(Shot->GetActorRotation().Yaw, HalfAngle, 0.5), FString::Printf(TEXT("yaw %g"), Shot ? Shot->GetActorRotation().Yaw : 0.0)));
		}

		TEST_METHOD(AnAllysDisplacementOfAMarkedEnemyEarnsShotsUpToTheBonus)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Ally = World.Spawn(EVeyraTeam::A, FVector(0.0, LaneRange / 4.0, 0.0));
			AVeyraVanguardCharacter& Marked = World.Spawn(EVeyraTeam::B, FVector(LaneRange / 4.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Unmarked = World.Spawn(EVeyraTeam::B, FVector(LaneRange / 4.0, LaneRange / 4.0, 0.0));
			ASSERT_THAT(IsTrue(FireToward(FVector::ForwardVector) == EVeyraCastRejection::None));
			const TOptional<FVeyraStatusSpec> Mark = UVeyraAbilitiesTuningSubsystem::FindStatus(ArchetypeTestId(TEXT("test_mark")));
			ASSERT_THAT(IsTrue(Mark.IsSet() && VeyraCombat::ApplyStatus(*Caster->GetAbilitySystemComponent(), *Marked.GetAbilitySystemComponent(), Mark.GetValue())));

			ASSERT_THAT(IsTrue(PushBy(*Caster, Marked) && ShotsLeft() == Shots, TEXT("its caster's own displacement earns none")));
			ASSERT_THAT(IsTrue(PushBy(Ally, Unmarked) && ShotsLeft() == Shots, TEXT("an enemy it did not mark earns none")));
			for (int32 Pushes = 0; Pushes <= BonusShots; ++Pushes)
			{
				ASSERT_THAT(IsTrue(PushBy(Ally, Marked)));
			}
			ASSERT_THAT(AreEqual(Shots + BonusShots, ShotsLeft()));
		}

		TEST_METHOD(TheLaneClosesWhenItsTimeRunsOut)
		{
			ASSERT_THAT(IsTrue(FireToward(FVector::ForwardVector) == EVeyraCastRejection::None));
			AdvanceWorld(LaneSeconds + WorldStep);
			ASSERT_THAT(IsTrue(Holds(TEXT("test_corridor")) && !Planted() && ShotsLeft() == 0));
		}

		TEST_METHOD(ValidationChecksTheShotAndTheBonus)
		{
			constexpr int32 RankCounts[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, RankCounts).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, RankCounts), TEXT(" | "))));
			FVeyraAbilitiesTuning Broken = Tuning;
			FVeyraVolleyAbilityTuning& Corridor = Broken.Volley.FindChecked(ArchetypeTestId(TEXT("test_corridor")));
			Corridor.Shot = ArchetypeTestId(TEXT("test_corridor"));
			Corridor.Shots = 0;
			Corridor.Bonus[0].Status = ArchetypeTestId(TEXT("no_such_status"));
			const FString All = FString::Join(VeyraAbilityRules::Validate(Broken, RankCounts), TEXT(" | "));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/volley/test_corridor/shot:")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/volley/test_corridor/shots:")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("no_such_status")), All));
		}
	};

	// Veyra.Abilities.SkillshotRecoil.*: a skillshot whose caster recoils away from its aim (ADR-018 §6),
	// as Kade's Reposition.
	TEST_CLASS(SkillshotRecoil, "Veyra.Abilities")
	{
		// Fixture values.
		static constexpr double Range = 750.0;
		static constexpr double Recoil = 400.0;
		static constexpr double RecoilSpeed = 1000.0;

		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;

		BEFORE_EACH()
		{
			FVeyraSkillshotAbilityTuning Reposition;
			Reposition.Cast = InstantCast(Range, Range, 0.0);
			Reposition.Projectile.Speed = Range;
			Reposition.Projectile.Radius = Recoil / 8.0;
			Reposition.Projectile.Range = Range;
			FVeyraCasterDashTuning& Dash = Reposition.CasterDash.AddDefaulted_GetRef();
			Dash.Distance = Recoil;
			Dash.Speed = RecoilSpeed;
			Tuning.Skillshot.Add(ArchetypeTestId(TEXT("test_reposition")), Reposition);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_reposition")))));
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		TEST_METHOD(TheCasterRecoilsAwayFromItsAim)
		{
			const FVector Start = Caster->GetActorLocation();
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::E, FVector(Range, 0.0, 0.0)) == EVeyraCastRejection::None));
			UVeyraMovementComponent* Movement = Caster->GetVeyraMovement();
			ASSERT_THAT(IsTrue(Movement->IsDashing()));
			const TOptional<FVector> End = Movement->GetForcedMoveDestination();
			ASSERT_THAT(IsTrue(End.IsSet() && End->X < Start.X && FMath::IsNearlyEqual(End->Y, Start.Y, 1.0), TEXT("back along the aim")));
			ASSERT_THAT(IsTrue(TActorIterator<AVeyraProjectile>(&Spawner.GetWorld()) ? true : false, TEXT("the shot still leaves")));
		}

		TEST_METHOD(ARootedOrGroundedCasterCannotRecoil)
		{
			// The recoil moves the caster by its own ability, which both refuse (ADR-026 §3; ADR-028 §2).
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(0.0, Range * 2.0, 0.0));
			for (const EVeyraStatusKind Kind : { EVeyraStatusKind::Root, EVeyraStatusKind::Grounded })
			{
				FVeyraStatusSpec Held;
				Held.Id = ArchetypeTestId(Kind == EVeyraStatusKind::Root ? TEXT("test_rooted") : TEXT("test_grounded"));
				Held.Kind = Kind;
				Held.DurationSeconds = Range;
				ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Enemy.GetAbilitySystemComponent(), *Caster->GetAbilitySystemComponent(), Held)));
				ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::E, FVector(Range, 0.0, 0.0)) == EVeyraCastRejection::CrowdControlled,
					*UEnum::GetValueAsString(Kind)));
				VeyraCombat::RemoveStatus(*Caster->GetAbilitySystemComponent(), Held.Id);
			}
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::E, FVector(Range, 0.0, 0.0)) == EVeyraCastRejection::None, TEXT("free again")));
		}

		TEST_METHOD(ValidationKeepsItToOneWithADistanceAndASpeed)
		{
			constexpr int32 RankCounts[] = { 5, 3 };
			FVeyraAbilitiesTuning Broken = Tuning;
			FVeyraSkillshotAbilityTuning& Reposition = Broken.Skillshot.FindChecked(ArchetypeTestId(TEXT("test_reposition")));
			Reposition.CasterDash[0].Speed = 0.0;
			Reposition.CasterDash.AddDefaulted();
			const FString All = FString::Join(VeyraAbilityRules::Validate(Broken, RankCounts), TEXT(" | "));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/skillshot/test_reposition/casterDash:")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/skillshot/test_reposition/casterDash/0:")), All));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
