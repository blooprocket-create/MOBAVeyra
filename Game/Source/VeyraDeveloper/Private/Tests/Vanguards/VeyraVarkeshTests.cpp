// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Delivery/VeyraProjectile.h"
#include "Entities/VeyraPlacedMarker.h"
#include "EngineUtils.h"
#include "Passives/VeyraStressTemperPassive.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraVanguards.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	// Veyra.Vanguards.Forgeheart.*: Varkesh's kit (Roster Bible §14; ADR-032), from the committed tuning.
	TEST_CLASS(Forgeheart, "Veyra.Vanguards")
	{
		// Fixture values: XP for many levels, steps of time, and how far an enemy dashes.
		static constexpr double ManyLevels = 50000.0;
		static constexpr float Step = 0.05f;
		static constexpr double DashDistance = 300.0;
		static constexpr double DashSpeed = 1500.0;
		static constexpr int32 MaxSteps = 200;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Varkesh = nullptr;
		UVeyraProgressionComponent* Progression = nullptr;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Varkesh = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraPlayerState* Participant = Varkesh->GetPlayerState<AVeyraPlayerState>();
			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(*Participant->GetAbilitySystemComponent(), Id(TEXT("varkesh")));
			ASSERT_THAT(IsTrue(Prepared.bPrepared));
			Participant->SetPassive(Prepared.Passive);
			ASSERT_THAT(IsNotNull(Cast<UVeyraStressTemperPassive>(Prepared.Passive)));
			Progression = Participant->FindComponentByClass<UVeyraProgressionComponent>();
			Progression->AddExperience(ManyLevels);
			for (const EVeyraAbilitySlot Slot : { EVeyraAbilitySlot::Q, EVeyraAbilitySlot::W, EVeyraAbilitySlot::E, EVeyraAbilitySlot::R })
			{
				ASSERT_THAT(IsTrue(Progression->AllocateRank(Slot) == EVeyraRankRefusal::None));
			}
		}

		static FVeyraContentId Id(const TCHAR* Text)
		{
			return FVeyraContentId::FromText(Text).GetValue();
		}

		bool Holds(const AActor& Unit, const TCHAR* Status) const
		{
			return VeyraCombat::HasStatusFrom(&Unit, Id(Status), *Varkesh->GetAbilitySystemComponent());
		}

		FVeyraContentId In(EVeyraAbilitySlot Slot) const
		{
			const FVeyraLoadoutEntry* Entry = Varkesh->GetPlayerState()->FindComponentByClass<UVeyraAbilityLoadoutComponent>()->FindSlot(Slot);
			return Entry ? Entry->Ability : FVeyraContentId();
		}

		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, Step);
				++GFrameCounter;
				World.GetTimerManager().Tick(Step);
				for (TActorIterator<AVeyraProjectile> It(&World); It; ++It)
				{
					if (!It->IsActorBeingDestroyed())
					{
						It->AdvanceBy(Step);
					}
				}
			}
		}

		static const FVeyraSkillshotAbilityTuning& Divide()
		{
			return *UVeyraAbilitiesTuningSubsystem::FindSkillshot(Id(TEXT("varkesh_forge_divide")));
		}

		TEST_METHOD(ForgeDivideLeavesAWallThatShatterforgeDetonates)
		{
			const double Range = Divide().Projectile.Range;
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Range * 0.8, Divide().Projectile.Radius / 2.0, 0.0));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Varkesh, EVeyraAbilitySlot::R, FVector(Range, 0.0, 0.0)) == EVeyraCastRejection::None));
			const double Arming = Divide().Cast.RecastWindow[0].ArmingSeconds;
			Wait(Divide().Cast.WindupSeconds + FMath::Max(Arming, Range / Divide().Projectile.Speed) + 2.0 * Step);
			const AVeyraPlacedMarker* Wall = AVeyraPlacedMarker::FindStanding(*Varkesh->GetAbilitySystemComponent(), Id(TEXT("varkesh_forge_divide")));
			ASSERT_THAT(IsTrue(Wall && Wall->IsWall(), TEXT("the wave cooled into a wall")));
			ASSERT_THAT(IsTrue(In(EVeyraAbilitySlot::R) == Id(TEXT("varkesh_shatterforge")), TEXT("and Shatterforge is armed")));
			ASSERT_THAT(IsTrue(Holds(Enemy, TEXT("varkesh_heated_metal")), TEXT("the wave's hit coats")));
			const double Before = FArchetypeTestWorld::HealthLost(Enemy);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Varkesh, EVeyraAbilitySlot::R, Varkesh->GetActorLocation()) == EVeyraCastRejection::None));
			ASSERT_THAT(IsNull(AVeyraPlacedMarker::FindStanding(*Varkesh->GetAbilitySystemComponent(), Id(TEXT("varkesh_forge_divide"))), TEXT("the wall goes")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(Enemy) > Before && Holds(Enemy, TEXT("varkesh_shatter_slow")), TEXT("and its blast strikes and slows")));
		}

		TEST_METHOD(TemperedShellHoldsItsResistancesUntilItBreaks)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(100.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Varkesh, EVeyraAbilitySlot::W, Varkesh->GetActorLocation()) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Holds(*Varkesh, TEXT("varkesh_shell_tenacity")) && Holds(*Varkesh, TEXT("varkesh_shell_footing"))));
			FVeyraRawDamageEvent Crushing;
			Crushing.Components.Add({ EVeyraDamageType::TrueDamage, Varkesh->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) / 2.0 });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Enemy.GetAbilitySystemComponent(), *Varkesh->GetAbilitySystemComponent(), Crushing)));
			ASSERT_THAT(IsFalse(Holds(*Varkesh, TEXT("varkesh_shell_tenacity")), TEXT("they go as it breaks")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(Enemy) > 0.0, TEXT("and the heat bursts out")));
		}

		TEST_METHOD(AnEnemyDashingOffMoltenGroundIsCaught)
		{
			FArchetypeTestWorld World{ Spawner };
			const FVector Point(400.0, 0.0, 0.0);
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, Point + FVector(100.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Varkesh, EVeyraAbilitySlot::E, Point) == EVeyraCastRejection::None));
			Wait(UVeyraAbilitiesTuningSubsystem::FindArea(Id(TEXT("varkesh_molten_ground")))->Linger[0].PulseSeconds + Step);
			ASSERT_THAT(IsTrue(Holds(Enemy, TEXT("varkesh_heated_metal")), TEXT("the ground coats it")));
			const double Before = FArchetypeTestWorld::HealthLost(Enemy);
			// It dashes clear of the slow's hold: a Slow does not stop a dash.
			ASSERT_THAT(IsTrue(VeyraCombat::Dash(*Enemy.GetAbilitySystemComponent(), FVeyraDash{ FVector::RightVector, DashDistance, DashSpeed, EVeyraDashContact::None })));
			UVeyraMovementComponent* Movement = Enemy.GetVeyraMovement();
			for (int32 Steps = 0; Movement->IsDashing() && Steps < MaxSteps; ++Steps)
			{
				Movement->AdvanceForcedMove(Step);
			}
			ASSERT_THAT(IsTrue(Holds(Enemy, TEXT("varkesh_seized")), TEXT("caught where it lands")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(Enemy) > Before));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
