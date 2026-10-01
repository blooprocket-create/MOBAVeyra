// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "CQTest.h"
#include "Delivery/VeyraLingeringArea.h"
#include "Delivery/VeyraProjectile.h"
#include "EngineUtils.h"
#include "Passives/VeyraMistTrailPassive.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Targeting/VeyraVisibility.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "VeyraVanguards.h"
#include "VeyraVisionSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	/** Fixture values for Sylra's tests: XP for many levels, where units stand, and a step of time. */
	namespace SylraFixture
	{
		constexpr double ManyLevels = 50000.0;
		constexpr double Near = 600.0;
		constexpr double Far = 1500.0;
		constexpr float Step = 0.1f;
	}

	// Veyra.Vanguards.Mistwarden.*: Sylra's kit (Roster Bible §16; ADR-036), from the committed tuning.
	TEST_CLASS(Mistwarden, "Veyra.Vanguards")
	{
		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Sylra = nullptr;
		UVeyraMistTrailPassive* Bell = nullptr;

		static FVeyraContentId Id(const TCHAR* Text)
		{
			return FVeyraContentId::FromText(Text).GetValue();
		}

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Sylra = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraPlayerState* Participant = Sylra->GetPlayerState<AVeyraPlayerState>();
			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(*Participant->GetAbilitySystemComponent(), Id(TEXT("sylra")));
			ASSERT_THAT(IsTrue(Prepared.bPrepared));
			Participant->SetPassive(Prepared.Passive);
			Bell = Cast<UVeyraMistTrailPassive>(Prepared.Passive);
			ASSERT_THAT(IsNotNull(Bell, TEXT("Follow the Bell is her passive")));
			UVeyraProgressionComponent* Progression = Participant->FindComponentByClass<UVeyraProgressionComponent>();
			Progression->AddExperience(SylraFixture::ManyLevels);
			for (const EVeyraAbilitySlot Slot : { EVeyraAbilitySlot::Q, EVeyraAbilitySlot::W, EVeyraAbilitySlot::E, EVeyraAbilitySlot::R })
			{
				ASSERT_THAT(IsTrue(Progression->AllocateRank(Slot) == EVeyraRankRefusal::None));
			}
			Spawner.GetWorld().GetSubsystem<UVeyraVisionSubsystem>()->Start();
		}

		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, SylraFixture::Step);
				++GFrameCounter;
				World.GetTimerManager().Tick(SylraFixture::Step);
			}
		}

		bool InFog(const FVector& Where)
		{
			return VeyraVisibility::FogVolumeAt(&Spawner.GetWorld(), Where) != INDEX_NONE;
		}

		static bool Has(const AActor& Unit, const TCHAR* Status)
		{
			return FArchetypeTestWorld::Has(Unit, Status);
		}

		static double Lost(const AActor& Unit)
		{
			const UAbilitySystemComponent& Health = *UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
			return Health.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) - Health.GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
		}

		static double ShieldOf(const AVeyraVanguardCharacter& Unit)
		{
			const UVeyraDamageAbsorptionComponent* Absorption = Unit.GetPlayerState()->FindComponentByClass<UVeyraDamageAbsorptionComponent>();
			double Sum = 0.0;
			for (const FVeyraShieldEntry& Each : Absorption ? Absorption->GetLedger().Shields : TArray<FVeyraShieldEntry>())
			{
				Sum += Each.Remaining;
			}
			return Sum;
		}

		TEST_METHOD(HarborBellSoundsTheFirstEnemyVanguardItStrikes)
		{
			using namespace SylraFixture;
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Sylra, EVeyraAbilitySlot::Q, Enemy.GetActorLocation()) == EVeyraCastRejection::None));
			// Its windup, then its pulse's flight: a test world moves nothing on its own.
			Wait(0.5);
			for (TActorIterator<AVeyraProjectile> It(&Spawner.GetWorld()); It; ++It)
			{
				It->AdvanceBy(1.0);
			}
			ASSERT_THAT(IsTrue(Lost(Enemy) > 0.0, TEXT("magic damage")));
			ASSERT_THAT(IsTrue(Has(Enemy, TEXT("sylra_sounded")), TEXT("and Sounded")));
		}

		TEST_METHOD(LayTheMistAndThroughTheWhiteLayDenseFog)
		{
			using namespace SylraFixture;
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Sylra, EVeyraAbilitySlot::W, FVector(Near, 0.0, 0.0)) == EVeyraCastRejection::None));
			Wait(0.5);
			ASSERT_THAT(IsTrue(InFog(FVector(Near, 0.0, 0.0)), TEXT("fog at the point")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Sylra, EVeyraAbilitySlot::R, FVector(0.0, Near, 0.0)) == EVeyraCastRejection::None));
			Wait(0.5);
			ASSERT_THAT(IsTrue(InFog(FVector(0.0, Far, 0.0)), TEXT("a corridor along her aim")));
			ASSERT_THAT(IsFalse(InFog(FVector(0.0, -Far, 0.0)), TEXT("never behind her")));
		}

		TEST_METHOD(WaymarkShieldsTheAllyBesideIt)
		{
			using namespace SylraFixture;
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Ally = World.Spawn(EVeyraTeam::A, FVector(Near, 0.0, 0.0));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Sylra, EVeyraAbilitySlot::E, Ally.GetActorLocation()) == EVeyraCastRejection::None));
			Wait(0.5);
			ASSERT_THAT(IsTrue(ShieldOf(Ally) > 0.0));
			const double First = ShieldOf(Ally);
			Wait(1.5);
			ASSERT_THAT(IsTrue(ShieldOf(Ally) > First, TEXT("and builds while it stays")));
		}

		TEST_METHOD(SteppingIntoHerOwnMistLeavesATrail)
		{
			using namespace SylraFixture;
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Sylra, EVeyraAbilitySlot::W, FVector(Near, 0.0, 0.0)) == EVeyraCastRejection::None));
			Wait(0.5);
			Bell->Look();
			Sylra->SetActorLocation(FVector(Near, 0.0, Sylra->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
			Bell->Look();
			ASSERT_THAT(AreEqual(1, Bell->GetTrailCount()));
			int32 Trail = 0;
			for (TActorIterator<AVeyraLingeringArea> It(&Spawner.GetWorld()); It; ++It)
			{
				Trail += It->GetAbility() == Id(TEXT("sylra_mist_trail")) ? 1 : 0;
			}
			ASSERT_THAT(IsTrue(Trail > 0, TEXT("the Mist Trail lies behind her")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
