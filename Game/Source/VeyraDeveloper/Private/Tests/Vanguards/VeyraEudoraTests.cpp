// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraVitalsSet.h"
#include "Companions/VeyraCompanion.h"
#include "Companions/VeyraCompanionSubsystem.h"
#include "CQTest.h"
#include "Delivery/VeyraProjectile.h"
#include "EngineUtils.h"
#include "Passives/VeyraAllHandsPassive.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "VeyraCombatVerbs.h"
#include "VeyraVanguards.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	/** Fixture values for Eudora's tests: XP for many levels, where units stand, a shot, and a step of time. */
	namespace EudoraFixture
	{
		constexpr double ManyLevels = 50000.0;
		constexpr double Set = 500.0;
		constexpr double Behind = 200.0;
		constexpr double Ahead = 1100.0;
		constexpr double Beyond = 1500.0;
		constexpr double Shot = 100.0;
		constexpr float Step = 0.1f;
	}

	// Veyra.Vanguards.Fieldwright.*: Eudora's kit and Picket (Roster Bible §25; ADR-037), from the committed tuning.
	TEST_CLASS(Fieldwright, "Veyra.Vanguards")
	{
		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Eudora = nullptr;
		UVeyraAllHandsPassive* Hands = nullptr;

		static FVeyraContentId Id(const TCHAR* Text)
		{
			return FVeyraContentId::FromText(Text).GetValue();
		}

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Eudora = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraPlayerState* Participant = Eudora->GetPlayerState<AVeyraPlayerState>();
			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(*Participant->GetAbilitySystemComponent(), Id(TEXT("eudora")));
			ASSERT_THAT(IsTrue(Prepared.bPrepared));
			Participant->SetPassive(Prepared.Passive);
			Hands = Cast<UVeyraAllHandsPassive>(Prepared.Passive);
			ASSERT_THAT(IsNotNull(Hands, TEXT("All Hands is her passive")));
			UVeyraProgressionComponent* Progression = Participant->FindComponentByClass<UVeyraProgressionComponent>();
			Progression->AddExperience(EudoraFixture::ManyLevels);
			for (const EVeyraAbilitySlot Slot : { EVeyraAbilitySlot::Q, EVeyraAbilitySlot::W, EVeyraAbilitySlot::E, EVeyraAbilitySlot::R })
			{
				ASSERT_THAT(IsTrue(Progression->AllocateRank(Slot) == EVeyraRankRefusal::None));
			}
		}

		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, EudoraFixture::Step);
				++GFrameCounter;
				World.GetTimerManager().Tick(EudoraFixture::Step);
			}
		}

		AVeyraCompanion* Picket()
		{
			return Spawner.GetWorld().GetSubsystem<UVeyraCompanionSubsystem>()->FindLiving(*Eudora->GetAbilitySystemComponent());
		}

		bool SetThePicket()
		{
			if (FArchetypeTestWorld::CastAt(*Eudora, EVeyraAbilitySlot::W, FVector(EudoraFixture::Set, 0.0, 0.0)) != EVeyraCastRejection::None)
			{
				return false;
			}
			// Its readable setup.
			Wait(1.0);
			return Picket() != nullptr;
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

		static bool ShootAt(AActor& Source, AActor& Target)
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, EudoraFixture::Shot });
			Damage.bProjectile = true;
			return VeyraCombat::DealDamage(*UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Source),
				*UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Target), Damage);
		}

		TEST_METHOD(DriveRivetStrikesAndDesignatesTheFirstEnemyVanguard)
		{
			using namespace EudoraFixture;
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Set, 0.0, 0.0));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Eudora, EVeyraAbilitySlot::Q, Enemy.GetActorLocation()) == EVeyraCastRejection::None));
			Wait(0.5);
			for (TActorIterator<AVeyraProjectile> It(&Spawner.GetWorld()); It; ++It)
			{
				It->AdvanceBy(1.0);
			}
			ASSERT_THAT(IsTrue(Lost(Enemy) > 0.0, TEXT("physical damage")));
			ASSERT_THAT(IsTrue(Has(Enemy, TEXT("eudora_designated")), TEXT("and Designated")));
		}

		TEST_METHOD(SetThePicketDeploysItInGunPlatform)
		{
			ASSERT_THAT(IsTrue(SetThePicket()));
			ASSERT_THAT(IsTrue(Picket()->GetMode() == EVeyraCompanionMode::Anchored));
			ASSERT_THAT(IsTrue(Has(*Picket(), TEXT("picket_gun_platform"))));
			ASSERT_THAT(IsFalse(Picket()->HoldsFire()));
		}

		TEST_METHOD(RaiseTheBulwarkCoversAnAllyBehindItAndHoldsItsFire)
		{
			using namespace EudoraFixture;
			ASSERT_THAT(IsTrue(SetThePicket()));
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Ally = World.Spawn(EVeyraTeam::A, FVector(Set - Behind, Behind, 0.0));
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Ahead, 0.0, 0.0));
			ASSERT_THAT(IsTrue(ShootAt(Enemy, Ally)));
			const double Open = Lost(Ally);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Eudora, EVeyraAbilitySlot::E, FVector(Beyond, 0.0, 0.0)) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Has(*Picket(), TEXT("picket_bulwark")) && Picket()->HoldsFire()));
			ASSERT_THAT(IsTrue(ShootAt(Enemy, Ally)));
			const double Covered = Lost(Ally) - Open;
			ASSERT_THAT(IsTrue(Covered > 0.0 && Covered < Open, FString::Printf(TEXT("open %g, covered %g"), Open, Covered)));
		}

		TEST_METHOD(MoveTheLineWalksPicketWithAnAllyThenSetsItDown)
		{
			ASSERT_THAT(IsTrue(SetThePicket()));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Eudora, EVeyraAbilitySlot::E, FVector(EudoraFixture::Beyond, 0.0, 0.0)) == EVeyraCastRejection::None));
			FVeyraCastTarget Self;
			Self.Actor = Eudora;
			Self.bHasLocation = true;
			Self.Location = FVector(0.0, EudoraFixture::Beyond, 0.0);
			ASSERT_THAT(IsTrue(VeyraAbilities::TryCast(*Eudora->GetAbilitySystemComponent(), EVeyraAbilitySlot::R, Self) == EVeyraCastRejection::None));
			Wait(0.5);
			AVeyraCompanion& Moved = *Picket();
			ASSERT_THAT(IsTrue(Moved.IsMoving() && Has(Moved, TEXT("picket_on_the_move")) && !Has(Moved, TEXT("picket_bulwark"))));
			Wait(7.0);
			ASSERT_THAT(IsTrue(!Moved.IsMoving() && Moved.GetMode() == EVeyraCompanionMode::Anchored, TEXT("set down")));
			ASSERT_THAT(IsTrue(Has(Moved, TEXT("picket_bulwark")) && !Has(Moved, TEXT("picket_on_the_move")), TEXT("as it was before")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
