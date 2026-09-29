// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Components/ActorTestSpawner.h"
#include "CQTest.h"
#include "Gold/VeyraGoldComponent.h"
#include "Layout/VeyraLayout.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Rewards/VeyraEconomyTuningSubsystem.h"
#include "Rules/VeyraWildlifeRules.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tests/World/VeyraBattlegroundTestLayout.h"
#include "TimerManager.h"
#include "VeyraBattlegroundSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "Wildlife/VeyraJungleSubsystem.h"
#include "Wildlife/VeyraWildlife.h"
#include "Wildlife/VeyraWildlifeController.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraWorldTests
{
	// Veyra.World.WildlifeRules.*: where a camp's creatures stand, and how far they fight (Battleground
	// Bible §17; ADR-014 §2).
	TEST_CLASS(WildlifeRules, "Veyra.World")
	{
		static constexpr double Tolerance = 1e-6;

		TEST_METHOD(ACampsCreaturesStandRoundItsCentre)
		{
			const FVector2D Center(100.0, -200.0);
			constexpr double Spacing = 150.0;
			const TArray<FVector2D> Alone = VeyraWildlifeRules::Positions(Center, 1, Spacing);
			ASSERT_THAT(IsTrue(Alone.Num() == 1 && Alone[0].Equals(Center, Tolerance)));
			const TArray<FVector2D> Pack = VeyraWildlifeRules::Positions(Center, 3, Spacing);
			ASSERT_THAT(AreEqual(3, Pack.Num()));
			for (const FVector2D& Spot : Pack)
			{
				ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FVector2D::Distance(Spot, Center), Spacing, Tolerance)));
			}
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FVector2D::Distance(Pack[0], Pack[1]), FVector2D::Distance(Pack[1], Pack[2]), Tolerance), TEXT("evenly")));
		}

		TEST_METHOD(ACreatureFightsOnlyWithinItsLeash)
		{
			const FVector2D Home(0.0, 0.0);
			constexpr double Leash = 400.0;
			const FVector2D Inside(300.0, 0.0);
			const FVector2D Outside(500.0, 0.0);
			ASSERT_THAT(IsTrue(VeyraWildlifeRules::KeepsFighting(Home, Leash, Inside, Inside, true)));
			ASSERT_THAT(IsFalse(VeyraWildlifeRules::KeepsFighting(Home, Leash, Inside, Outside, true), TEXT("its target left the leash")));
			ASSERT_THAT(IsFalse(VeyraWildlifeRules::KeepsFighting(Home, Leash, Outside, Inside, true), TEXT("it was carried out of it")));
			ASSERT_THAT(IsFalse(VeyraWildlifeRules::KeepsFighting(Home, Leash, Inside, Inside, false), TEXT("its target can no longer be fought")));
		}
	};

	// Veyra.World.JungleCamps.*: camps spawn on the match clock on both halves, answer together, pay each
	// creature's death, grant their trait on a clear and respawn on their own timers; a creature gives
	// up beyond its leash and heals at home (Battleground Bible §8, §17; Economy Bible §7; ADR-014 §2, §3).
	TEST_CLASS(JungleCamps, "Veyra.World")
	{
		static constexpr double Tolerance = 1e-3;

		// Fixture values: one camp of three on Team A's half of the compact battleground, clear of its lane.
		static constexpr double CampX = 800.0;
		static constexpr double CampY = -1600.0;
		static constexpr double Leash = 400.0;
		static constexpr double Spacing = 150.0;
		static constexpr int32 Pack = 3;
		static constexpr double SpawnSeconds = 10.0;
		static constexpr double RespawnSeconds = 30.0;
		static constexpr double Margin = 0.5;
		static constexpr double Scratch = 50.0;

		FActorTestSpawner Spawner;
		TUniquePtr<FScopedWorldTuning> WorldTuning;
		UVeyraJungleSubsystem* Jungle = nullptr;

		static FVeyraContentId Skittermaw()
		{
			return FVeyraContentId::FromText(TEXT("skittermaw")).GetValue();
		}

		BEFORE_EACH()
		{
			WorldTuning = MakeUnique<FScopedWorldTuning>();
			FVeyraCampTuning Camp;
			Camp.Species = Skittermaw();
			Camp.Count = Pack;
			Camp.Center = { CampX, CampY };
			Camp.SpawnSeconds = SpawnSeconds;
			Camp.RespawnSeconds = RespawnSeconds;
			Camp.LeashRadius = Leash;
			Camp.Spacing = Spacing;
			WorldTuning->Tuning.Wildlife.Camps = { Camp };
			UVeyraBattlegroundSubsystem* Battleground = Spawner.GetWorld().GetSubsystem<UVeyraBattlegroundSubsystem>();
			ASSERT_THAT(IsNotNull(Battleground));
			Battleground->SpawnStructures(CompactBattleground());
			Jungle = Spawner.GetWorld().GetSubsystem<UVeyraJungleSubsystem>();
			ASSERT_THAT(IsNotNull(Jungle));
		}

		AFTER_EACH()
		{
			WorldTuning.Reset();
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

		/** A Vanguard on Team A at Where, with Gold and progression, as a hunter in the jungle. */
		AVeyraVanguardCharacter& SpawnHunter(const FVector& Where)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Hunter = World.Spawn(EVeyraTeam::A, Where);
			Hunter.GetPlayerState()->FindComponentByClass<UVeyraProgressionComponent>()->Initialize(FVeyraStatGrowth(), 0.0);
			return Hunter;
		}

		static void Hit(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, double Amount)
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Amount });
			VeyraCombat::DealDamage(Source, Target, Damage);
		}

		static double HealthOf(const AVeyraWildlife& Creature, const FGameplayAttribute& Attribute)
		{
			return Creature.GetAbilitySystemComponent()->GetNumericAttribute(Attribute);
		}

		TEST_METHOD(CampsSpawnOnTheirTimeOnBothHalves)
		{
			Jungle->Start();
			TArray<FVeyraCampState> Camps = Jungle->GetCamps();
			ASSERT_THAT(AreEqual(2, Camps.Num()));
			ASSERT_THAT(IsTrue(Camps[0].Half == EVeyraTeam::A && Camps[1].Half == EVeyraTeam::B));
			ASSERT_THAT(IsTrue(VeyraLayout::Mirror(Camps[0].Center).Equals(Camps[1].Center, Tolerance), TEXT("Team B's camp is Team A's mirror")));
			ASSERT_THAT(IsTrue(Camps[0].Alive == 0 && Camps[0].SpawnsAt > 0.0, TEXT("nothing before its spawn time")));

			Advance(SpawnSeconds + Margin);
			Camps = Jungle->GetCamps();
			ASSERT_THAT(IsTrue(Camps[0].Alive == Pack && Camps[1].Alive == Pack));
			const AVeyraWildlife& Creature = *Jungle->GetCreatures(0)[0];
			const FVeyraWildlifeSpecies& Species = *UVeyraWorldTuningSubsystem::Get().FindSpecies(Skittermaw());
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthOf(Creature, UVeyraVitalsSet::GetMaxHealthAttribute()), Species.Stats.MaxHealth, Tolerance)));
			ASSERT_THAT(IsTrue(Creature.GetVeyraTeam() == EVeyraTeam::None && Creature.GetVeyraUnitKind() == EVeyraUnitKind::Wildlife));
			ASSERT_THAT(IsNotNull(Cast<AVeyraWildlifeController>(Creature.GetController()), TEXT("its server controller possesses it")));
		}

		TEST_METHOD(AHurtCreaturesWholeCampAnswers)
		{
			Jungle->Start();
			ASSERT_THAT(AreEqual(Pack, Jungle->SpawnCamp(0)));
			const TArray<AVeyraWildlife*> Creatures = Jungle->GetCreatures(0);
			AVeyraVanguardCharacter& Hunter = SpawnHunter(FVector(CampX, CampY + Spacing * 2.0, 100.0));
			Hit(*Hunter.GetAbilitySystemComponent(), *Creatures[0]->GetAbilitySystemComponent(), Scratch);
			for (const AVeyraWildlife* Creature : Creatures)
			{
				ASSERT_THAT(IsTrue(Cast<AVeyraWildlifeController>(Creature->GetController())->GetTarget() == &Hunter));
			}
		}

		TEST_METHOD(AClearedCampPaysGrantsItsTraitAndRespawns)
		{
			Jungle->Start();
			Jungle->SpawnCamp(0);
			AVeyraVanguardCharacter& Hunter = SpawnHunter(FVector(CampX, CampY + Spacing * 2.0, 100.0));
			for (AVeyraWildlife* Creature : Jungle->GetCreatures(0))
			{
				Hit(*Hunter.GetAbilitySystemComponent(), *Creature->GetAbilitySystemComponent(), HealthOf(*Creature, UVeyraVitalsSet::GetMaxHealthAttribute()));
			}
			// Each creature's Gold to the hunter (Economy Bible §7).
			const double Gold = UVeyraEconomyTuningSubsystem::Get().Gold.Wildlife[Skittermaw()] * Pack;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Hunter.GetPlayerState()->FindComponentByClass<UVeyraGoldComponent>()->GetGold(), Gold, Tolerance)));
			// The camp's trait to the Vanguard credited with its last kill (Battleground Bible §8).
			const FVeyraContentId Trait = UVeyraWorldTuningSubsystem::Get().FindSpecies(Skittermaw())->Traits[0];
			const UVeyraStatusComponent* Statuses = Hunter.GetPlayerState()->FindComponentByClass<UVeyraStatusComponent>();
			ASSERT_THAT(IsTrue(Statuses->GetLedger().Entries.ContainsByPredicate([&Trait](const FVeyraStatusEntry& Entry) { return Entry.Id == Trait; })));
			// Its own respawn timer (§17).
			FVeyraCampState Cleared = Jungle->GetCamps()[0];
			ASSERT_THAT(IsTrue(Cleared.Alive == 0 && FMath::IsNearlyEqual(Cleared.SpawnsAt, Spawner.GetWorld().GetTimeSeconds() + RespawnSeconds, Tolerance)));
			Advance(RespawnSeconds + Margin);
			ASSERT_THAT(AreEqual(Pack, Jungle->GetCamps()[0].Alive));
		}

		TEST_METHOD(BeyondItsLeashACreatureGoesHomeAndHeals)
		{
			Jungle->Start();
			Jungle->SpawnCamp(0);
			AVeyraWildlife& Creature = *Jungle->GetCreatures(0)[0];
			AVeyraWildlifeController& Controller = *Cast<AVeyraWildlifeController>(Creature.GetController());
			// A hunter well beyond the leash hits it: it answers, is drawn off its spot, then gives up.
			AVeyraVanguardCharacter& Hunter = SpawnHunter(FVector(CampX, CampY + Leash * 2.0, 100.0));
			Hit(*Hunter.GetAbilitySystemComponent(), *Creature.GetAbilitySystemComponent(), Scratch);
			ASSERT_THAT(IsTrue(Controller.GetTarget() == &Hunter));
			Creature.SetActorLocation(Creature.GetHome() + FVector(0.0, Leash / 2.0, 0.0));
			Controller.Think();
			ASSERT_THAT(IsTrue(Controller.IsReturning() && Controller.GetTarget() == nullptr));
			ASSERT_THAT(IsTrue(HealthOf(Creature, UVeyraVitalsSet::GetHealthAttribute()) < HealthOf(Creature, UVeyraVitalsSet::GetMaxHealthAttribute())));
			// Home again, it is whole.
			Creature.SetActorLocation(Creature.GetHome());
			Controller.Think();
			ASSERT_THAT(IsFalse(Controller.IsReturning()));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthOf(Creature, UVeyraVitalsSet::GetHealthAttribute()), HealthOf(Creature, UVeyraVitalsSet::GetMaxHealthAttribute()), Tolerance)));
		}

		TEST_METHOD(APacksLeashIsTheCampsNotEachCreaturesSpot)
		{
			Jungle->Start();
			Jungle->SpawnCamp(0);
			// A pack's creatures stand round the camp's centre: this one Spacing out from it.
			const FVector2D Center(CampX, CampY);
			AVeyraWildlife& Creature = *Jungle->GetCreatures(0).Last();
			const FVector2D Out = (FVector2D(Creature.GetHome()) - Center).GetSafeNormal();
			ASSERT_THAT(IsFalse(Out.IsNearlyZero(), TEXT("an outer creature")));
			// Within the leash of its own spot, but beyond the camp's, which validation keeps clear.
			const FVector2D Beyond = Center + Out * (Leash + Spacing / 2.0);
			AVeyraVanguardCharacter& Hunter = SpawnHunter(FVector(Beyond, 100.0));
			Hit(*Hunter.GetAbilitySystemComponent(), *Creature.GetAbilitySystemComponent(), Scratch);
			// Drawn in to the camp's centre, off its own spot (at home it would be whole again at once).
			Creature.SetActorLocation(FVector(Center, Creature.GetHome().Z));
			AVeyraWildlifeController& Controller = *Cast<AVeyraWildlifeController>(Creature.GetController());
			Controller.Think();
			ASSERT_THAT(IsTrue(Controller.IsReturning() && Controller.GetTarget() == nullptr, TEXT("it will not chase beyond its camp's leash")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
