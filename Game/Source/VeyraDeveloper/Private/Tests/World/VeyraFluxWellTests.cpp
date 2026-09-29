// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Components/ActorTestSpawner.h"
#include "CQTest.h"
#include "Gold/VeyraGoldComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Rewards/VeyraEconomyTuningSubsystem.h"
#include "Rules/VeyraFluxWellRules.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tests/World/VeyraBattlegroundTestLayout.h"
#include "TimerManager.h"
#include "VeyraBattlegroundSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "Wells/VeyraFluxWell.h"
#include "Wells/VeyraFluxWellSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraWorldTests
{
	// Veyra.World.FluxWellRules.*: how presence drains a Flux Well, capped and contested, and when a
	// Well left alone heals (Battleground Bible §6; ADR-014 §4, §9).
	TEST_CLASS(FluxWellRules, "Veyra.World")
	{
		static constexpr double Tolerance = 1e-9;

		static FVeyraFluxWellPresenceTuning Presence()
		{
			// Fixture values in the committed shape: 50 alone, 25 for each more, at most three, half rate contested.
			FVeyraFluxWellPresenceTuning Tuning;
			Tuning.DrainPerSecond = 50.0;
			Tuning.DrainPerAdditional = 25.0;
			Tuning.MaxCounted = 3;
			Tuning.ContestedFactor = 0.5;
			return Tuning;
		}

		TEST_METHOD(MoreVanguardsDrainFasterUpToTheCap)
		{
			ASSERT_THAT(IsTrue(VeyraFluxWellRules::DrainRate(0, Presence()) == 0.0));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraFluxWellRules::DrainRate(1, Presence()), 50.0, Tolerance)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraFluxWellRules::DrainRate(3, Presence()), 100.0, Tolerance)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraFluxWellRules::DrainRate(5, Presence()), 100.0, Tolerance), TEXT("a dogpile is not required (§6)")));
		}

		TEST_METHOD(EqualControlStallsItAndSuperiorControlDrainsAtAReducedRate)
		{
			const FVeyraPresenceDrain Alone = VeyraFluxWellRules::PresenceDrain(2, 0, Presence());
			ASSERT_THAT(IsTrue(Alone.Side == EVeyraTeam::A && FMath::IsNearlyEqual(Alone.PerSecond, 75.0, Tolerance)));
			ASSERT_THAT(IsTrue(VeyraFluxWellRules::PresenceDrain(2, 2, Presence()).Side == EVeyraTeam::None, TEXT("equal control stalls it")));
			ASSERT_THAT(IsTrue(VeyraFluxWellRules::PresenceDrain(0, 0, Presence()).Side == EVeyraTeam::None));
			const FVeyraPresenceDrain Contested = VeyraFluxWellRules::PresenceDrain(1, 3, Presence());
			ASSERT_THAT(IsTrue(Contested.Side == EVeyraTeam::B && FMath::IsNearlyEqual(Contested.PerSecond, 75.0 * 0.5, Tolerance), TEXT("its lead of two, at half rate")));
		}

		TEST_METHOD(ItHealsOnlyAfterItsIdleTime)
		{
			ASSERT_THAT(IsFalse(VeyraFluxWellRules::Regenerates(10.0, 8.0, 5.0)));
			ASSERT_THAT(IsTrue(VeyraFluxWellRules::Regenerates(13.0, 8.0, 5.0)));
		}
	};

	// Veyra.World.FluxWells.*: the Wells stand closed until their time, presence and damage drain an
	// open one, the side of its last hit secures it for its Gold pool, and it opens again after its
	// cycle (Battleground Bible §6; Economy Bible §8.2; ADR-014 §4).
	TEST_CLASS(FluxWells, "Veyra.World")
	{
		static constexpr double Tolerance = 1e-3;

		// Fixture values: one Well on the compact battleground's river, clear of its lane.
		static constexpr double SiteX = 600.0;
		static constexpr double SiteY = -600.0;
		static constexpr double Radius = 300.0;
		static constexpr double OpenSeconds = 20.0;
		static constexpr double RespawnSeconds = 60.0;
		static constexpr double Margin = 0.5;
		static constexpr double Drain = 100.0;
		static constexpr double Tick = 1.0;
		static constexpr double NeverSeconds = 1.0e6;

		FActorTestSpawner Spawner;
		TUniquePtr<FScopedWorldTuning> WorldTuning;
		UVeyraFluxWellSubsystem* Wells = nullptr;
		TArray<FVeyraFluxWellSecuredEvent> Secured;

		BEFORE_EACH()
		{
			WorldTuning = MakeUnique<FScopedWorldTuning>();
			FVeyraFluxWellsTuning& Tuning = WorldTuning->Tuning.FluxWells;
			Tuning.Sites = { { SiteX, SiteY } };
			Tuning.Timing.OpenSeconds = OpenSeconds;
			Tuning.Timing.RespawnSeconds = RespawnSeconds;
			Tuning.Radius = Radius;
			Tuning.Presence.DrainPerSecond = Drain;
			Tuning.Presence.DrainPerAdditional = 0.0;
			Tuning.Presence.MaxCounted = 3;
			Tuning.Presence.ContestedFactor = 0.5;
			// World time stands still in these tests, so a Well left alone heals at once; the tests drive
			// presence themselves, so its timer never fires.
			Tuning.Regeneration.IdleSeconds = 0.0;
			Tuning.Presence.TickSeconds = NeverSeconds;
			UVeyraBattlegroundSubsystem* Battleground = Spawner.GetWorld().GetSubsystem<UVeyraBattlegroundSubsystem>();
			ASSERT_THAT(IsNotNull(Battleground));
			Battleground->SpawnStructures(CompactBattleground());
			Wells = Spawner.GetWorld().GetSubsystem<UVeyraFluxWellSubsystem>();
			ASSERT_THAT(IsNotNull(Wells));
			Secured.Reset();
			Wells->OnFluxWellSecured.AddLambda([this](const FVeyraFluxWellSecuredEvent& Event) { Secured.Add(Event); });
		}

		AFTER_EACH()
		{
			WorldTuning.Reset();
		}

		/** Moves world timers on by Seconds. The timer manager ticks at most once per engine frame, and a
		 *  test runs inside one; its first tick only activates the timers set before it. */
		void Advance(double Seconds)
		{
			FTimerManager& Timers = Spawner.GetWorld().GetTimerManager();
			++GFrameCounter;
			Timers.Tick(0.0f);
			++GFrameCounter;
			Timers.Tick(static_cast<float>(Seconds));
		}

		AVeyraVanguardCharacter& SpawnVanguard(EVeyraTeam Team, const FVector& Where)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Vanguard = World.Spawn(Team, Where);
			Vanguard.GetPlayerState()->FindComponentByClass<UVeyraProgressionComponent>()->Initialize(FVeyraStatGrowth(), 0.0);
			return Vanguard;
		}

		AVeyraFluxWell& Well() const
		{
			return *Wells->GetWells()[0];
		}

		double HealthOf(const AVeyraFluxWell& Target, const FGameplayAttribute& Attribute) const
		{
			return Target.GetAbilitySystemComponent()->GetNumericAttribute(Attribute);
		}

		static void Hit(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, double Amount)
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Amount });
			VeyraCombat::DealDamage(Source, Target, Damage);
		}

		TEST_METHOD(AWellStandsClosedUntilItsTime)
		{
			Wells->Start();
			ASSERT_THAT(AreEqual(1, Wells->GetWells().Num()));
			ASSERT_THAT(IsTrue(Well().GetState() == EVeyraFluxWellState::Closed && Well().GetVeyraTeam() == EVeyraTeam::None));
			AVeyraVanguardCharacter& Early = SpawnVanguard(EVeyraTeam::A, FVector(SiteX + Radius / 2.0, SiteY, 100.0));
			const double Full = HealthOf(Well(), UVeyraVitalsSet::GetMaxHealthAttribute());
			Hit(*Early.GetAbilitySystemComponent(), *Well().GetAbilitySystemComponent(), Drain);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthOf(Well(), UVeyraVitalsSet::GetHealthAttribute()), Full, Tolerance), TEXT("closed, it cannot be damaged")));
			Advance(OpenSeconds + Margin);
			ASSERT_THAT(IsTrue(Well().GetState() == EVeyraFluxWellState::Open));
		}

		TEST_METHOD(PresenceSecuresItForItsSideWhichTakesThePool)
		{
			Wells->Start();
			Wells->Open(0);
			AVeyraVanguardCharacter& Taker = SpawnVanguard(EVeyraTeam::A, FVector(SiteX + Radius / 2.0, SiteY, 100.0));
			const double Full = HealthOf(Well(), UVeyraVitalsSet::GetMaxHealthAttribute());
			Wells->UpdatePresence(Tick);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthOf(Well(), UVeyraVitalsSet::GetHealthAttribute()), Full - Drain * Tick, Tolerance)));
			// Nearly drained by other means, the next presence tick lands the last hit.
			Hit(*Taker.GetAbilitySystemComponent(), *Well().GetAbilitySystemComponent(), Full - Drain * Tick * 1.5);
			Wells->UpdatePresence(Tick);
			ASSERT_THAT(IsTrue(Secured.Num() == 1 && Secured[0].Team == EVeyraTeam::A && Secured[0].Site == 0));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Taker.GetPlayerState()->FindComponentByClass<UVeyraGoldComponent>()->GetGold(),
				UVeyraEconomyTuningSubsystem::Get().Gold.FluxWellPool, Tolerance), TEXT("the whole pool to the only capturer")));
			ASSERT_THAT(IsTrue(Well().GetState() == EVeyraFluxWellState::Respawning));
			Advance(RespawnSeconds + Margin);
			ASSERT_THAT(IsTrue(Well().GetState() == EVeyraFluxWellState::Open && Well().IsStanding()));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthOf(Well(), UVeyraVitalsSet::GetHealthAttribute()), Full, Tolerance), TEXT("whole again")));
		}

		TEST_METHOD(EqualSidesStallItAndADamagingBlowStealsIt)
		{
			Wells->Start();
			Wells->Open(0);
			AVeyraVanguardCharacter& Ours = SpawnVanguard(EVeyraTeam::A, FVector(SiteX + Radius / 2.0, SiteY, 100.0));
			SpawnVanguard(EVeyraTeam::B, FVector(SiteX - Radius / 2.0, SiteY, 100.0));
			AVeyraVanguardCharacter& Thief = SpawnVanguard(EVeyraTeam::B, FVector(SiteX, SiteY + Radius * 3.0, 100.0));
			// Ours is hit a little first, so it is below full and a heal would show.
			Hit(*Ours.GetAbilitySystemComponent(), *Well().GetAbilitySystemComponent(), Drain);
			const double Before = HealthOf(Well(), UVeyraVitalsSet::GetHealthAttribute());
			Wells->UpdatePresence(Tick);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthOf(Well(), UVeyraVitalsSet::GetHealthAttribute()), Before, Tolerance), TEXT("equal control stalls it")));
			// A blow from outside its radius takes it for the thief's side.
			Hit(*Thief.GetAbilitySystemComponent(), *Well().GetAbilitySystemComponent(), Before);
			ASSERT_THAT(IsTrue(Secured.Num() == 1 && Secured[0].Team == EVeyraTeam::B));
			ASSERT_THAT(IsTrue(Thief.GetPlayerState()->FindComponentByClass<UVeyraGoldComponent>()->GetGold() > 0.0, TEXT("the stealer shares the pool")));
			ASSERT_THAT(IsTrue(Ours.GetPlayerState()->FindComponentByClass<UVeyraGoldComponent>()->GetGold() == 0.0));
		}

		TEST_METHOD(LeftAloneItHeals)
		{
			Wells->Start();
			Wells->Open(0);
			AVeyraVanguardCharacter& Passer = SpawnVanguard(EVeyraTeam::A, FVector(SiteX, SiteY + Radius * 3.0, 100.0));
			Hit(*Passer.GetAbilitySystemComponent(), *Well().GetAbilitySystemComponent(), Drain);
			const double Hurt = HealthOf(Well(), UVeyraVitalsSet::GetHealthAttribute());
			Wells->UpdatePresence(Tick);
			ASSERT_THAT(IsTrue(HealthOf(Well(), UVeyraVitalsSet::GetHealthAttribute()) > Hurt));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
