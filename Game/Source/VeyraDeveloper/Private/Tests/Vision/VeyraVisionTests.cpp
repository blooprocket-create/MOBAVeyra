// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "Life/VeyraLifeComponent.h"
#include "Rules/VeyraVisionRules.h"
#include "Targeting/VeyraTargeting.h"
#include "Tethers/VeyraTetherSubsystem.h"
#include "Tests/Abilities/VeyraTestFluxborn.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "Tuning/VeyraVisionTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardCharacter.h"
#include "VeyraVisionSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVisionTests
{
	// Veyra.Vision.VisionRules.*: sight as plain rules of positions (ADR-016 §2).
	TEST_CLASS(VisionRules, "Veyra.Vision")
	{
		TEST_METHOD(ASourceSeesWithinItsRadiusForItsSideOnly)
		{
			const FVeyraSightSource Sources[] = {
				{ EVeyraTeam::A, FVector2D(0.0, 0.0), 1000.0 },
				{ EVeyraTeam::B, FVector2D(5000.0, 0.0), 500.0 },
			};
			ASSERT_THAT(IsTrue(VeyraVisionRules::IsSeenBy(EVeyraTeam::A, Sources, FVector2D(999.0, 0.0))));
			ASSERT_THAT(IsTrue(VeyraVisionRules::IsSeenBy(EVeyraTeam::A, Sources, FVector2D(0.0, 1000.0)), TEXT("the edge counts")));
			ASSERT_THAT(IsFalse(VeyraVisionRules::IsSeenBy(EVeyraTeam::A, Sources, FVector2D(1001.0, 0.0))));
			ASSERT_THAT(IsTrue(VeyraVisionRules::IsSeenBy(EVeyraTeam::B, Sources, FVector2D(5400.0, 0.0))));
			ASSERT_THAT(IsFalse(VeyraVisionRules::IsSeenBy(EVeyraTeam::A, Sources, FVector2D(5000.0, 0.0)), TEXT("the other side's source")));
			ASSERT_THAT(IsFalse(VeyraVisionRules::IsSeenBy(EVeyraTeam::None, Sources, FVector2D(0.0, 0.0))));
		}

		TEST_METHOD(AWallBetweenASourceAndAPointHidesIt)
		{
			// Fixture: a wall facing +X at 500 along X, 400 long across it (ADR-042 §3).
			const FVeyraTerrainBox Walls[] = { { FVector2D(500.0, 0.0), FVector2D(1.0, 0.0), 400.0, 100.0 } };
			FVeyraSightSource Sources[] = { { EVeyraTeam::A, FVector2D(0.0, 0.0), 1000.0, /*bDetects*/ true } };
			ASSERT_THAT(IsTrue(VeyraVisionRules::IsBlocked(Walls, FVector2D(0.0, 0.0), FVector2D(900.0, 0.0))));
			ASSERT_THAT(IsFalse(VeyraVisionRules::IsSeenBy(EVeyraTeam::A, Sources, FVector2D(900.0, 0.0), Walls), TEXT("behind the wall")));
			ASSERT_THAT(IsTrue(VeyraVisionRules::IsSeenBy(EVeyraTeam::A, Sources, FVector2D(900.0, 0.0)), TEXT("with no walls given")));
			ASSERT_THAT(IsTrue(VeyraVisionRules::IsSeenBy(EVeyraTeam::A, Sources, FVector2D(300.0, 0.0), Walls), TEXT("before the wall")));
			ASSERT_THAT(IsTrue(VeyraVisionRules::IsSeenBy(EVeyraTeam::A, Sources, FVector2D(700.0, 600.0), Walls), TEXT("past its end")));
			ASSERT_THAT(IsFalse(VeyraVisionRules::IsDetectedBy(EVeyraTeam::A, Sources, FVector2D(900.0, 0.0), 1000.0, Walls), TEXT("no detection through it")));
			// A lit area lights what lies inside it, walls or none.
			Sources[0].bThroughWalls = true;
			ASSERT_THAT(IsTrue(VeyraVisionRules::IsSeenBy(EVeyraTeam::A, Sources, FVector2D(900.0, 0.0), Walls)));
		}

		TEST_METHOD(FogCirclesThatTouchAreOneVolume)
		{
			// Two touching circles, one apart, and a third touching the second (Vision Bible §2).
			const FVeyraFogCircle Circles[] = {
				{ FVector2D(0.0, 0.0), 100.0 },
				{ FVector2D(1000.0, 0.0), 100.0 },
				{ FVector2D(150.0, 0.0), 50.0 },
				{ FVector2D(1150.0, 0.0), 50.0 },
				{ FVector2D(1250.0, 0.0), 50.0 },
			};
			const TArray<int32> Volumes = VeyraVisionRules::ConnectVolumes(Circles);
			ASSERT_THAT(IsTrue(Volumes.Num() == 5 && Volumes[0] == Volumes[2] && Volumes[1] == Volumes[3] && Volumes[3] == Volumes[4]));
			ASSERT_THAT(IsTrue(Volumes[0] != Volumes[1], TEXT("apart, two volumes")));
			ASSERT_THAT(IsTrue(VeyraVisionRules::VolumeAt(Circles, Volumes, FVector2D(190.0, 0.0)) == Volumes[0]));
			ASSERT_THAT(IsTrue(VeyraVisionRules::VolumeAt(Circles, Volumes, FVector2D(1290.0, 0.0)) == Volumes[1]));
			ASSERT_THAT(IsTrue(VeyraVisionRules::VolumeAt(Circles, Volumes, FVector2D(500.0, 0.0)) == INDEX_NONE, TEXT("between them, no fog")));
		}
	};

	// Veyra.Vision.Sight.*: a match's vision, worked out from its units (ADR-016 §2).
	TEST_CLASS(Sight, "Veyra.Vision")
	{
		static constexpr double StartingMaxHealth = 100.0;

		FActorTestSpawner Spawner;

		AVeyraVanguardCharacter& SpawnVanguard(EVeyraTeam Team, const FVector& Location)
		{
			AVeyraPlayerState& PlayerState = Spawner.SpawnActor<AVeyraPlayerState>();
			PlayerState.SetVeyraTeam(Team);
			VeyraCombat::InitializeVitals(*PlayerState.GetAbilitySystemComponent(), StartingMaxHealth);
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

		TEST_METHOD(EachSideSeesWhatItsUnitsSee)
		{
			AVeyraVanguardCharacter& Caster = SpawnVanguard(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Near = SpawnVanguard(EVeyraTeam::B, FVector(SightRadius() / 2.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Far = SpawnVanguard(EVeyraTeam::B, FVector(SightRadius() * 3.0, 0.0, 0.0));
			ASSERT_THAT(IsNotNull(Spawner.GetWorld().GetSubsystem<UVeyraVisionSubsystem>()));
			Vision().Start();
			ASSERT_THAT(IsTrue(Vision().IsVisibleToTeam(EVeyraTeam::A, Near)));
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, Far), TEXT("beyond every one of A's sources")));
			ASSERT_THAT(IsTrue(Vision().IsVisibleToTeam(EVeyraTeam::B, Caster), TEXT("the near enemy sees back")));
			ASSERT_THAT(IsTrue(Vision().IsVisibleToTeam(EVeyraTeam::B, Far), TEXT("a side always sees its own")));
			// Targeting reads the same record (Vision Bible §1).
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(Caster, &Far, SightRadius() * 4.0) == EVeyraTargetValidity::NotVisible));
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(Caster, &Near, SightRadius()) == EVeyraTargetValidity::Valid));

			// It moves into sight: the next pass sees it.
			Far.SetActorLocation(FVector(0.0, SightRadius() / 2.0, 0.0));
			Vision().UpdateNow();
			ASSERT_THAT(IsTrue(Vision().IsVisibleToTeam(EVeyraTeam::A, Far)));
		}

		TEST_METHOD(TheBattlegroundsWallsHideWhatStandsBehindThem)
		{
			// Fixture: an enemy within sight, a wall between them, and a reveal that lights it anyway (ADR-042 §3).
			AVeyraVanguardCharacter& Watcher = SpawnVanguard(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Behind = SpawnVanguard(EVeyraTeam::B, FVector(SightRadius() / 2.0, 0.0, 0.0));
			Vision().Start();
			ASSERT_THAT(IsTrue(Vision().IsVisibleToTeam(EVeyraTeam::A, Behind)));
			const FVeyraTerrainBox Wall{ FVector2D(SightRadius() / 4.0, 0.0), FVector2D(1.0, 0.0), SightRadius(), SightRadius() / 20.0 };
			Vision().SetSightWalls({ Wall });
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, Behind), TEXT("the wall hides it")));
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::B, Watcher), TEXT("and hides it both ways")));
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(Watcher, &Behind, SightRadius()) == EVeyraTargetValidity::NotVisible));
			Vision().RevealArea(EVeyraTeam::A, Behind.GetActorLocation(), SightRadius() / 10.0, /*DurationSeconds*/ 5.0);
			Vision().UpdateNow();
			ASSERT_THAT(IsTrue(Vision().IsVisibleToTeam(EVeyraTeam::A, Behind), TEXT("a lit area lights it through the wall")));
		}

		TEST_METHOD(TheDeadGiveNoVision)
		{
			AVeyraVanguardCharacter& Scout = SpawnVanguard(EVeyraTeam::A, FVector(SightRadius() * 3.0, 0.0, 0.0));
			SpawnVanguard(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Enemy = SpawnVanguard(EVeyraTeam::B, FVector(SightRadius() * 3.0, SightRadius() / 2.0, 0.0));
			Vision().Start();
			ASSERT_THAT(IsTrue(Vision().IsVisibleToTeam(EVeyraTeam::A, Enemy)));
			Scout.GetPlayerState()->FindComponentByClass<UVeyraLifeComponent>()->SetState(EVeyraLifeState::Dead);
			Vision().UpdateNow();
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, Enemy)));
		}

		TEST_METHOD(AParticipantSeesFromItsVanguardNotItsPlayerState)
		{
			// Every PlayerState sits at the world's origin, where the Vanguards are not: combat counts it as
			// a unit, but it has no body on the battleground.
			SpawnVanguard(EVeyraTeam::A, FVector(SightRadius() * 3.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Enemy = SpawnVanguard(EVeyraTeam::B, FVector(SightRadius() / 4.0, 0.0, 0.0));
			Vision().Start();
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, Enemy)));
			// A participant is seen where its Vanguard is.
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, *Enemy.GetPlayerState())));
		}

		TEST_METHOD(AUnitThatJustSpawnedIsJudgedAtOnce)
		{
			// A tower acquiring a Fluxborn the moment it appears must not wait for the next pass.
			AVeyraVanguardCharacter& Caster = SpawnVanguard(EVeyraTeam::A, FVector::ZeroVector);
			Vision().Start();
			const AVeyraVanguardCharacter& Near = SpawnVanguard(EVeyraTeam::B, FVector(SightRadius() / 2.0, 0.0, 0.0));
			const AVeyraVanguardCharacter& Far = SpawnVanguard(EVeyraTeam::B, FVector(SightRadius() * 3.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(Vision().IsVisibleToTeam(EVeyraTeam::A, Near)));
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, Far)));
			ASSERT_THAT(IsTrue(VeyraTargeting::CanAcquire(Caster.GetPlayerState(), Near), TEXT("a participant looks from its side")));
		}

		TEST_METHOD(OnlyAVanguardInsideTheSameFogSeesAnEnemyInIt)
		{
			// The bush: an enemy inside Dense Fog is hidden from all but those inside the same volume, and a
			// teammate's sighting there is not shared (Vision Bible §2).
			const FVector2D Bush(SightRadius() * 2.0, 0.0);
			AVeyraVanguardCharacter& Inside = SpawnVanguard(EVeyraTeam::A, FVector(Bush.X - 100.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Outside = SpawnVanguard(EVeyraTeam::A, FVector(Bush.X - 500.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Enemy = SpawnVanguard(EVeyraTeam::B, FVector(Bush.X + 100.0, 0.0, 0.0));
			Vision().Start();
			Vision().SetDenseFog({ FVeyraFogCircle{ Bush, 300.0 } });
			ASSERT_THAT(IsTrue(VeyraTargeting::CanAcquire(&Inside, Enemy)));
			ASSERT_THAT(IsFalse(VeyraTargeting::CanAcquire(&Outside, Enemy), TEXT("its teammate, a few steps out of the fog")));
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, Enemy), TEXT("team vision does not carry it")));
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(Outside, &Enemy, SightRadius()) == EVeyraTargetValidity::NotVisible));
			// The enemy sees both: the one in its fog with it, and the one outside by ordinary vision.
			ASSERT_THAT(IsTrue(VeyraTargeting::CanAcquire(&Enemy, Inside) && VeyraTargeting::CanAcquire(&Enemy, Outside)));

			// Out of the fog, the enemy is seen by the whole team again.
			Enemy.SetActorLocation(FVector(Bush.X + 500.0, 0.0, 0.0));
			Vision().UpdateNow();
			ASSERT_THAT(IsTrue(VeyraTargeting::CanAcquire(&Outside, Enemy) && Vision().IsVisibleToTeam(EVeyraTeam::A, Enemy)));
		}

		/** A Camouflage's detection radius: well inside a Vanguard's sight. */
		static double DetectionRadius()
		{
			return SightRadius() / 4.0;
		}

		template <typename UnitType>
		UnitType& SpawnUnit(EVeyraTeam Team, const FVector& Location)
		{
			UnitType& Unit = Spawner.SpawnActorAt<UnitType>(Location, FRotator::ZeroRotator);
			Unit.SetVeyraTeam(Team);
			VeyraCombat::InitializeVitals(*Unit.GetAbilitySystemComponent(), StartingMaxHealth);
			return Unit;
		}

		TEST_METHOD(ACamouflagedEnemyIsSeenOnlyWithinItsDetectionRadius)
		{
			AVeyraVanguardCharacter& Caster = SpawnVanguard(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Enemy = SpawnVanguard(EVeyraTeam::B, FVector(SightRadius() / 2.0, 0.0, 0.0));
			Vision().Start();
			ASSERT_THAT(IsTrue(Vision().IsVisibleToTeam(EVeyraTeam::A, Enemy)));
			ASSERT_THAT(IsTrue(VeyraCombatTests::Camouflage(Enemy, DetectionRadius())));
			Vision().UpdateNow();
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, Enemy), TEXT("in sight, beyond its detection radius")));
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(Caster, &Enemy, SightRadius()) == EVeyraTargetValidity::NotVisible));
			ASSERT_THAT(IsTrue(Vision().IsVisibleToTeam(EVeyraTeam::B, Enemy), TEXT("its own side still sees it")));

			Enemy.SetActorLocation(FVector(DetectionRadius() / 2.0, 0.0, 0.0));
			Vision().UpdateNow();
			ASSERT_THAT(IsTrue(Vision().IsVisibleToTeam(EVeyraTeam::A, Enemy), TEXT("within its radius of an enemy Vanguard")));
		}

		TEST_METHOD(OnlyVanguardsAndStandingStructuresDetectCamouflage)
		{
			const FVector Hideout(SightRadius() * 3.0, 0.0, 0.0);
			SpawnVanguard(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Enemy = SpawnVanguard(EVeyraTeam::B, Hideout);
			ASSERT_THAT(IsTrue(VeyraCombatTests::Camouflage(Enemy, DetectionRadius())));
			SpawnUnit<AVeyraTestFluxborn>(EVeyraTeam::A, Hideout - FVector(DetectionRadius() / 2.0, 0.0, 0.0));
			Vision().Start();
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, Enemy), TEXT("a Fluxborn beside it does not detect it")));
			SpawnUnit<AVeyraTestStructure>(EVeyraTeam::A, Hideout + FVector(0.0, DetectionRadius() / 2.0, 0.0));
			Vision().UpdateNow();
			ASSERT_THAT(IsTrue(Vision().IsVisibleToTeam(EVeyraTeam::A, Enemy), TEXT("a standing structure does")));
		}

		TEST_METHOD(AnInvisibleEnemyIsHiddenEvenBesideAVanguardAndTrueSightShowsIt)
		{
			AVeyraVanguardCharacter& Caster = SpawnVanguard(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Enemy = SpawnVanguard(EVeyraTeam::B, FVector(DetectionRadius() / 4.0, 0.0, 0.0));
			FVeyraStatusSpec Invisible;
			Invisible.Id = FVeyraContentId::FromText(TEXT("test_invisible")).GetValue();
			Invisible.Kind = EVeyraStatusKind::Invisible;
			// Fixture value: longer than the test.
			Invisible.DurationSeconds = 60.0;
			UAbilitySystemComponent& Hidden = *Enemy.GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Hidden, Hidden, Invisible)));
			Vision().Start();
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, Enemy), TEXT("no nearness shows it (Combat Bible §11)")));
			ASSERT_THAT(IsTrue(Vision().IsVisibleToTeam(EVeyraTeam::B, Enemy), TEXT("its own side sees it")));
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(Caster, &Enemy, SightRadius()) == EVeyraTargetValidity::NotVisible));
			constexpr double TrueSightSeconds = 60.0;
			Vision().AddTrueSight(EVeyraTeam::A, Caster, SightRadius(), TrueSightSeconds);
			Vision().UpdateNow();
			ASSERT_THAT(IsTrue(Vision().IsVisibleToTeam(EVeyraTeam::A, Enemy), TEXT("True Sight does")));
		}

		TEST_METHOD(AnUntargetableEnemyCannotBeTargetedButAnUntargetableAllyCan)
		{
			AVeyraVanguardCharacter& Caster = SpawnVanguard(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Ally = SpawnVanguard(EVeyraTeam::A, FVector(0.0, SightRadius() / 4.0, 0.0));
			AVeyraVanguardCharacter& Enemy = SpawnVanguard(EVeyraTeam::B, FVector(SightRadius() / 4.0, 0.0, 0.0));
			FVeyraStatusSpec Vanish;
			Vanish.Id = FVeyraContentId::FromText(TEXT("test_untargetable")).GetValue();
			Vanish.Kind = EVeyraStatusKind::Untargetable;
			Vanish.DurationSeconds = 60.0;
			for (AVeyraVanguardCharacter* Held : { &Ally, &Enemy })
			{
				ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Held->GetAbilitySystemComponent(), *Held->GetAbilitySystemComponent(), Vanish)));
			}
			Vision().Start();
			ASSERT_THAT(IsTrue(Vision().IsVisibleToTeam(EVeyraTeam::A, Enemy), TEXT("it is still seen (Combat Bible §10)")));
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(Caster, &Enemy, SightRadius()) != EVeyraTargetValidity::Valid));
			ASSERT_THAT(IsFalse(VeyraTargeting::CanHitEnemy(&Caster, Enemy), TEXT("nor hit by areas or skillshots")));
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckAllyTarget(Caster, &Ally, SightRadius()) == EVeyraTargetValidity::Valid, TEXT("allies' effects still reach it")));
			VeyraCombat::RemoveStatus(*Enemy.GetAbilitySystemComponent(), Vanish.Id);
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(Caster, &Enemy, SightRadius()) == EVeyraTargetValidity::Valid));
		}

		TEST_METHOD(TrueSightShowsACamouflagedEnemy)
		{
			AVeyraVanguardCharacter& Caster = SpawnVanguard(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Enemy = SpawnVanguard(EVeyraTeam::B, FVector(SightRadius() / 2.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(VeyraCombatTests::Camouflage(Enemy, DetectionRadius())));
			Vision().Start();
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, Enemy)));
			// Fixture value: longer than the test.
			constexpr double TrueSightSeconds = 60.0;
			Vision().AddTrueSight(EVeyraTeam::A, Caster, SightRadius(), TrueSightSeconds);
			Vision().UpdateNow();
			ASSERT_THAT(IsTrue(Vision().IsVisibleToTeam(EVeyraTeam::A, Enemy)));
		}

		TEST_METHOD(DenseFogStillComesFirstForTheCamouflaged)
		{
			// A bush of half a Vanguard's sight, the enemy just inside its edge, and a teammate just outside,
			// within its detection radius but out of the fog.
			const double S = SightRadius();
			const FVector2D Bush(S * 2.0, 0.0);
			AVeyraVanguardCharacter& Enemy = SpawnVanguard(EVeyraTeam::B, FVector(Bush.X + S * 7.0 / 16.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Outside = SpawnVanguard(EVeyraTeam::A, FVector(Bush.X + S * 9.0 / 16.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Inside = SpawnVanguard(EVeyraTeam::A, FVector(Bush.X - S / 4.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(VeyraCombatTests::Camouflage(Enemy, DetectionRadius())));
			Vision().Start();
			Vision().SetDenseFog({ FVeyraFogCircle{ Bush, S / 2.0 } });
			ASSERT_THAT(IsFalse(VeyraTargeting::CanAcquire(&Outside, Enemy), TEXT("within its radius, but the fog comes first")));
			ASSERT_THAT(IsFalse(VeyraTargeting::CanAcquire(&Inside, Enemy), TEXT("in the same fog, beyond its radius")));
			Inside.SetActorLocation(FVector(Bush.X + S * 5.0 / 16.0, 0.0, 0.0));
			Vision().UpdateNow();
			ASSERT_THAT(IsTrue(VeyraTargeting::CanAcquire(&Inside, Enemy), TEXT("in the same fog, within its radius")));
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, Enemy), TEXT("a sighting in fog is still not shared")));
		}

		TEST_METHOD(ATetherShowsItsTargetThroughFogAndCamouflageButNotInDenseFog)
		{
			const double S = SightRadius();
			AVeyraVanguardCharacter& Source = SpawnVanguard(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Enemy = SpawnVanguard(EVeyraTeam::B, FVector(S * 2.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(VeyraCombatTests::Camouflage(Enemy, DetectionRadius())));
			Vision().Start();
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, Enemy)));
			// Fixture values: a tether long enough to hold for the test.
			FVeyraTetherSpec Thread;
			Thread.Id = FVeyraContentId::FromText(TEXT("test_thread")).GetValue();
			Thread.MaxRange = S * 4.0;
			Thread.DurationSeconds = 60.0;
			UVeyraTetherSubsystem& Tethers = *Spawner.GetWorld().GetSubsystem<UVeyraTetherSubsystem>();
			ASSERT_THAT(IsTrue(Tethers.Tether(*Source.GetAbilitySystemComponent(), *Enemy.GetAbilitySystemComponent(), Thread)));
			Vision().UpdateNow();
			ASSERT_THAT(IsTrue(Vision().IsVisibleToTeam(EVeyraTeam::A, Enemy), TEXT("out of sight and Camouflaged, but tethered (Combat Bible §43)")));
			Vision().SetDenseFog({ FVeyraFogCircle{ FVector2D(Enemy.GetActorLocation()), S / 4.0 } });
			Vision().UpdateNow();
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, Enemy), TEXT("Dense Fog overrides a tether's vision")));
		}

		TEST_METHOD(TrueSightShowsACamouflagedEnemyToALookoutInTheSameFog)
		{
			// A bush of half a Vanguard's sight, the enemy just inside its edge, and a teammate inside it
			// too, within sight but beyond the detection radius.
			const double S = SightRadius();
			const FVector2D Bush(S * 2.0, 0.0);
			AVeyraVanguardCharacter& Enemy = SpawnVanguard(EVeyraTeam::B, FVector(Bush.X + S * 7.0 / 16.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Inside = SpawnVanguard(EVeyraTeam::A, FVector(Bush.X - S / 4.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(VeyraCombatTests::Camouflage(Enemy, DetectionRadius())));
			Vision().Start();
			Vision().SetDenseFog({ FVeyraFogCircle{ Bush, S / 2.0 } });
			ASSERT_THAT(IsFalse(VeyraTargeting::CanAcquire(&Inside, Enemy), TEXT("in the same fog, beyond its radius")));
			// Fixture value: longer than the test.
			constexpr double TrueSightSeconds = 60.0;
			Vision().AddTrueSight(EVeyraTeam::A, Inside, SightRadius(), TrueSightSeconds);
			Vision().UpdateNow();
			ASSERT_THAT(IsTrue(VeyraTargeting::CanAcquire(&Inside, Enemy), TEXT("True Sight shows it once the fog's volume is shared")));
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, Enemy), TEXT("a sighting in fog is still not shared")));
		}

		TEST_METHOD(AShapedRevealIsOrdinaryVisionInsideItsShape)
		{
			// A corridor lit far beyond the side's sight, along +X (ADR-018 §5).
			const double S = SightRadius();
			SpawnVanguard(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Inside = SpawnVanguard(EVeyraTeam::B, FVector(S * 3.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Beside = SpawnVanguard(EVeyraTeam::B, FVector(S * 3.0, S, 0.0));
			AVeyraVanguardCharacter& Hidden = SpawnVanguard(EVeyraTeam::B, FVector(S * 3.5, 0.0, 0.0));
			ASSERT_THAT(IsTrue(VeyraCombatTests::Camouflage(Hidden, DetectionRadius())));
			Vision().Start();
			FVeyraShape Corridor;
			Corridor.Kind = EVeyraShapeKind::Rectangle;
			Corridor.Length = S * 2.0;
			Corridor.Width = S / 4.0;
			// Fixture value: longer than the test.
			constexpr double RevealSeconds = 60.0;
			Vision().RevealShape(EVeyraTeam::A, FVeyraPlacedShape{ Corridor, FVector(S * 2.0, 0.0, 0.0), FVector::ForwardVector }, RevealSeconds);
			Vision().UpdateNow();
			ASSERT_THAT(IsTrue(Vision().IsVisibleToTeam(EVeyraTeam::A, Inside)));
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, Beside), TEXT("beside the shape, though within its reach")));
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, Hidden), TEXT("ordinary vision detects no Camouflage")));
			// Over Dense Fog it shows nothing.
			Vision().SetDenseFog({ FVeyraFogCircle{ FVector2D(S * 3.0, 0.0), S / 8.0 } });
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, Inside)));
		}

		TEST_METHOD(AWorldWithoutAMatchSeesEverything)
		{
			// Match starts Vision; unit tests and development worlds without a match never do.
			AVeyraVanguardCharacter& Caster = SpawnVanguard(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Far = SpawnVanguard(EVeyraTeam::B, FVector(SightRadius() * 3.0, 0.0, 0.0));
			ASSERT_THAT(IsFalse(Vision().IsStarted()));
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(Caster, &Far, SightRadius() * 4.0) == EVeyraTargetValidity::Valid));
			Vision().Start();
			Vision().Stop();
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(Caster, &Far, SightRadius() * 4.0) == EVeyraTargetValidity::Valid, TEXT("stopped, it governs nothing")));
		}
	};
}

#endif
