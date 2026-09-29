// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "Life/VeyraLifeComponent.h"
#include "Rules/VeyraVisionRules.h"
#include "Targeting/VeyraTargeting.h"
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
