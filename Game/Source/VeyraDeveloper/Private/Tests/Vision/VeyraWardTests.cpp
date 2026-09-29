// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "EngineUtils.h"
#include "Gold/VeyraGoldComponent.h"
#include "Life/VeyraLifeComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Rewards/VeyraEconomyTuningSubsystem.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tools/VeyraVisionToolComponent.h"
#include "Tuning/VeyraVisionTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardCharacter.h"
#include "VeyraVisionSubsystem.h"
#include "Wards/VeyraWard.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVisionTests
{
	// Veyra.Vision.Wards.*: the Persistent Ward's charges, the wards it places, what they show their
	// side and hide from the enemy, and what destroying one pays (Vision Bible §4, §8; ADR-016 §6).
	TEST_CLASS(Wards, "Veyra.Vision")
	{
		static constexpr double Tolerance = 1e-6;

		FActorTestSpawner Spawner;
		/** Places the wards, on Team B. */
		AVeyraVanguardCharacter* Warder = nullptr;
		/** On Team A: destroys them. */
		AVeyraVanguardCharacter* Destroyer = nullptr;
		AVeyraVanguardCharacter* DestroyersAlly = nullptr;

		BEFORE_EACH()
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			Warder = &Spawn(World, EVeyraTeam::B, FVector::ZeroVector);
			Destroyer = &Spawn(World, EVeyraTeam::A, FVector(0.0, Far(), 0.0));
			DestroyersAlly = &Spawn(World, EVeyraTeam::A, FVector(0.0, -Far(), 0.0));
		}

		static AVeyraVanguardCharacter& Spawn(VeyraAbilitiesTests::FArchetypeTestWorld& World, EVeyraTeam Team, const FVector& Location)
		{
			AVeyraVanguardCharacter& Vanguard = World.Spawn(Team, Location);
			Vanguard.GetPlayerState()->FindComponentByClass<UVeyraProgressionComponent>()->Initialize(FVeyraStatGrowth(), 0.0);
			return Vanguard;
		}

		/** Further than anything in this world sees. */
		static double Far()
		{
			const FVeyraSightTuning& Sight = UVeyraVisionTuningSubsystem::Get().Sight;
			return 10.0 * FMath::Max(Sight.Vanguard, Sight.Ward);
		}

		static UVeyraVisionToolComponent& ToolOf(const AVeyraVanguardCharacter& Vanguard)
		{
			return *Vanguard.GetPlayerState()->FindComponentByClass<UVeyraVisionToolComponent>();
		}

		UVeyraVisionSubsystem& Vision()
		{
			return *Spawner.GetWorld().GetSubsystem<UVeyraVisionSubsystem>();
		}

		/** The wards in the world. */
		TArray<AVeyraWard*> PlacedWards()
		{
			TArray<AVeyraWard*> Found;
			for (TActorIterator<AVeyraWard> It(&Spawner.GetWorld()); It; ++It)
			{
				if (IsValid(*It))
				{
					Found.Add(*It);
				}
			}
			return Found;
		}

		/** Moves world time on by Seconds. The timer manager ticks at most once per engine frame, and a
		 *  test runs inside one; its first tick only activates the timers set before it. */
		void Advance(double Seconds)
		{
			FTimerManager& Timers = Spawner.GetWorld().GetTimerManager();
			++GFrameCounter;
			Timers.Tick(0.0f);
			++GFrameCounter;
			Timers.Tick(static_cast<float>(Seconds));
		}

		TEST_METHOD(APersistentWardSpendsAChargeAndStandsWithinReach)
		{
			const FVeyraVisionTuning& Tuning = UVeyraVisionTuningSubsystem::Get();
			UVeyraVisionToolComponent& Tool = ToolOf(*Warder);
			ASSERT_THAT(IsTrue(Tool.GetEquipped() == EVeyraVisionTool::PersistentWard && Tool.GetWardCharges() == Tuning.WardCharges.Max,
				TEXT("every Vanguard begins with Persistent Ward and all its charges")));

			// Far beyond its reach: brought back within it, in the same direction.
			const FVector Aim = Warder->GetActorLocation() + FVector(Far(), 0.0, 0.0);
			ASSERT_THAT(IsTrue(Tool.Use(Aim) == EVeyraVisionToolRejection::None));
			const TArray<AVeyraWard*> Placed = PlacedWards();
			ASSERT_THAT(IsTrue(Placed.Num() == 1));
			const AVeyraWard& Ward = *Placed[0];
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FVector::Dist2D(Ward.GetActorLocation(), Warder->GetActorLocation()), Tuning.PersistentWard.PlacementRange, Tolerance)));
			ASSERT_THAT(IsTrue(Ward.GetActorLocation().Y == Warder->GetActorLocation().Y));
			ASSERT_THAT(IsTrue(Ward.GetVeyraTeam() == EVeyraTeam::B && Ward.GetPlacer() == Warder->GetPlayerState()));
			ASSERT_THAT(IsTrue(Ward.GetAbilitySystemComponent()->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute()) == Tuning.PersistentWard.HitsToDestroy,
				TEXT("a point of Health per hit it takes")));
			ASSERT_THAT(IsTrue(Tool.GetWardCharges() == Tuning.WardCharges.Max - 1 && Tool.GetNextChargeAt() >= 0.0));
		}

		TEST_METHOD(WithNoChargeLeftItRefusesUntilTheFountainRefillsIt)
		{
			UVeyraVisionToolComponent& Tool = ToolOf(*Warder);
			const int32 Max = UVeyraVisionTuningSubsystem::Get().WardCharges.Max;
			for (int32 Charge = 0; Charge < Max; ++Charge)
			{
				ASSERT_THAT(IsTrue(Tool.Use(Warder->GetActorLocation()) == EVeyraVisionToolRejection::None));
			}
			ASSERT_THAT(IsTrue(Tool.Use(Warder->GetActorLocation()) == EVeyraVisionToolRejection::NoCharge));
			ASSERT_THAT(IsTrue(PlacedWards().Num() == Max, TEXT("three charges carried, but no cap on wards placed")));
			Tool.RefillWardCharges();
			ASSERT_THAT(IsTrue(Tool.GetWardCharges() == Max && Tool.GetNextChargeAt() < 0.0));
		}

		TEST_METHOD(ChargesComeBackOneAtATime)
		{
			const FVeyraPersistentWardTuning& Ward = UVeyraVisionTuningSubsystem::Get().PersistentWard;
			const int32 Max = UVeyraVisionTuningSubsystem::Get().WardCharges.Max;
			UVeyraVisionToolComponent& Tool = ToolOf(*Warder);
			ASSERT_THAT(IsTrue(Tool.Use(Warder->GetActorLocation()) == EVeyraVisionToolRejection::None));
			ASSERT_THAT(IsTrue(Tool.Use(Warder->GetActorLocation()) == EVeyraVisionToolRejection::None));
			// A timer fires once time is past its due time, not at it.
			constexpr double JustPast = 0.1;
			Advance(Ward.RechargeSeconds + JustPast);
			ASSERT_THAT(AreEqual(Tool.GetWardCharges(), Max - 1, TEXT("one back")));
			Advance(Ward.RechargeSeconds + JustPast);
			ASSERT_THAT(AreEqual(Tool.GetWardCharges(), Max, TEXT("then the other")));
			ASSERT_THAT(IsTrue(Tool.GetNextChargeAt() < 0.0, TEXT("and none after")));
		}

		TEST_METHOD(TheDeadPlaceNoWard)
		{
			UVeyraVisionToolComponent& Tool = ToolOf(*Warder);
			Warder->GetPlayerState()->FindComponentByClass<UVeyraLifeComponent>()->SetState(EVeyraLifeState::Dead);
			ASSERT_THAT(IsTrue(Tool.Use(Warder->GetActorLocation()) == EVeyraVisionToolRejection::NoVanguard));
			ASSERT_THAT(IsTrue(PlacedWards().IsEmpty() && Tool.GetWardCharges() == UVeyraVisionTuningSubsystem::Get().WardCharges.Max));
		}

		TEST_METHOD(AWardLastsItsLifetime)
		{
			ASSERT_THAT(IsTrue(ToolOf(*Warder).Use(Warder->GetActorLocation()) == EVeyraVisionToolRejection::None));
			const double Lifetime = UVeyraVisionTuningSubsystem::Get().PersistentWard.LifetimeSeconds;
			Advance(Lifetime / 2.0);
			ASSERT_THAT(IsTrue(PlacedWards().Num() == 1));
			Advance(Lifetime);
			ASSERT_THAT(IsTrue(PlacedWards().IsEmpty()));
		}

		TEST_METHOD(AWardShowsItsSideWhatItSeesButTheEnemyNeverSeesIt)
		{
			Vision().Start();
			ASSERT_THAT(IsTrue(ToolOf(*Warder).Use(Warder->GetActorLocation()) == EVeyraVisionToolRejection::None));
			AVeyraWard& Ward = *PlacedWards()[0];
			// The warder walks far away; an enemy stands beside its ward.
			Warder->SetActorLocation(FVector(-Far(), 0.0, 0.0));
			const double Near = UVeyraVisionTuningSubsystem::Get().Sight.Ward / 2.0;
			Destroyer->SetActorLocation(Ward.GetActorLocation() + FVector(Near, 0.0, 0.0));
			Vision().UpdateNow();
			ASSERT_THAT(IsTrue(Vision().IsVisibleToTeam(EVeyraTeam::B, *Destroyer), TEXT("the ward shows its side the enemy")));
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, Ward), TEXT("an enemy beside it still cannot see it")));
			ASSERT_THAT(IsFalse(Vision().CanSee(*Destroyer, Ward)));
			ASSERT_THAT(IsTrue(Vision().IsVisibleToTeam(EVeyraTeam::B, Ward), TEXT("its own side always sees it")));
		}

		TEST_METHOD(AVanguardsBasicAttacksDestroyAWardAndPayOnlyItsDestroyer)
		{
			Vision().Start();
			ASSERT_THAT(IsTrue(ToolOf(*Warder).Use(Warder->GetActorLocation()) == EVeyraVisionToolRejection::None));
			AVeyraWard& Ward = *PlacedWards()[0];
			const double GoldBefore = Destroyer->GetPlayerState()->FindComponentByClass<UVeyraGoldComponent>()->GetGold();
			FVeyraRawDamageEvent Hit;
			Hit.Components.Add({ EVeyraDamageType::Physical, 1.0 });
			Hit.Delivery = EVeyraDamageDelivery::BasicAttack;
			for (int32 Blow = 0; Blow < UVeyraVisionTuningSubsystem::Get().PersistentWard.HitsToDestroy; ++Blow)
			{
				ASSERT_THAT(IsTrue(Ward.IsAlive()));
				ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Destroyer->GetAbilitySystemComponent(), *Ward.GetAbilitySystemComponent(), Hit)));
			}
			ASSERT_THAT(IsFalse(Ward.IsAlive()));
			const double Bounty = UVeyraEconomyTuningSubsystem::Get().VisionTools.WardBounty;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Destroyer->GetPlayerState()->FindComponentByClass<UVeyraGoldComponent>()->GetGold(), GoldBefore + Bounty, Tolerance)));
			ASSERT_THAT(IsTrue(DestroyersAlly->GetPlayerState()->FindComponentByClass<UVeyraGoldComponent>()->GetGold() == 0.0, TEXT("no share")));
			ASSERT_THAT(IsTrue(Destroyer->GetPlayerState()->FindComponentByClass<UVeyraProgressionComponent>()->GetExperience() == 0.0, TEXT("and no XP")));
			// It leaves once every listener has heard of its death: on the next tick.
			constexpr double OneStep = 0.1;
			Advance(OneStep);
			ASSERT_THAT(IsTrue(PlacedWards().IsEmpty(), TEXT("it leaves the battleground")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
