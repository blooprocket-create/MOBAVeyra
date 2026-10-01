// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Engine/World.h"
#include "Passives/VeyraWildDominionPassive.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tests/World/VeyraBattlegroundTestLayout.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraBattlegroundSubsystem.h"
#include "VeyraVanguards.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	// Veyra.Vanguards.WildDominion.*: Moro's passive (Roster Bible §12; ADR-026 §5), from the committed
	// tuning, on the compact test battleground.
	TEST_CLASS(WildDominion, "Veyra.Vanguards")
	{
		// Fixture values: places on the compact battleground (its mid lane runs along Y = X, its river
		// along Y = -X), each beyond every tower's reach; a wound to restore from, a blow to wildlife,
		// and a small step of world time.
		static constexpr double InJungleX = 1400.0;
		static constexpr double InJungleY = -600.0;
		static constexpr double OnLane = 0.0;
		static constexpr double Wound = 300.0;
		static constexpr double Blow = 100.0;
		static constexpr float WorldStep = 0.05f;
		static constexpr double Tolerance = 1e-3;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Moro = nullptr;

		BEFORE_EACH()
		{
			Spawner.GetWorld().GetSubsystem<UVeyraBattlegroundSubsystem>()->SpawnStructures(VeyraWorldTests::CompactBattleground());
			FArchetypeTestWorld World{ Spawner };
			Moro = &World.Spawn(EVeyraTeam::A, FVector(OnLane, OnLane, 0.0));
			AVeyraPlayerState* Participant = Moro->GetPlayerState<AVeyraPlayerState>();
			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(*Participant->GetAbilitySystemComponent(), FVeyraContentId::FromText(TEXT("moro")).GetValue());
			ASSERT_THAT(IsTrue(Prepared.bPrepared && Cast<UVeyraWildDominionPassive>(Prepared.Passive) != nullptr));
			Participant->SetPassive(Prepared.Passive);
		}

		static const FVeyraWildDominionTuning& Tuning()
		{
			return *UVeyraVanguardsTuningSubsystem::FindWildDominion(FVeyraContentId::FromText(TEXT("moro_wild_dominion")).GetValue());
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

		bool HoldsAll() const
		{
			for (const FVeyraContentId& Status : Tuning().Statuses)
			{
				if (!FArchetypeTestWorld::Has(*Moro, *Status.ToString()))
				{
					return false;
				}
			}
			return !Tuning().Statuses.IsEmpty();
		}

		bool HoldsAny() const
		{
			return Tuning().Statuses.ContainsByPredicate([this](const FVeyraContentId& Status) { return FArchetypeTestWorld::Has(*Moro, *Status.ToString()); });
		}

		TEST_METHOD(InTheJungleHeHoldsItsStatusesAndOutOfItTheyLapse)
		{
			AdvanceWorld(Tuning().CheckSeconds * 2.0);
			ASSERT_THAT(IsFalse(HoldsAny(), TEXT("not where the lane crosses the river")));
			Moro->SetActorLocation(FVector(InJungleX, InJungleY, 0.0));
			AdvanceWorld(Tuning().CheckSeconds + WorldStep);
			ASSERT_THAT(IsTrue(HoldsAll(), TEXT("in the jungle")));
			// Given again at each check, they last while he stays.
			double Longest = 0.0;
			for (const FVeyraContentId& Status : Tuning().Statuses)
			{
				Longest = FMath::Max(Longest, UVeyraAbilitiesTuningSubsystem::Get().Statuses.FindChecked(Status).DurationSeconds);
			}
			AdvanceWorld(Longest + WorldStep);
			ASSERT_THAT(IsTrue(HoldsAll(), TEXT("still in the jungle")));
			Moro->SetActorLocation(FVector(OnLane, OnLane, 0.0));
			AdvanceWorld(Longest + Tuning().CheckSeconds + WorldStep);
			ASSERT_THAT(IsFalse(HoldsAny(), TEXT("they lapse once he leaves")));
		}

		TEST_METHOD(DamagingWildlifeRestoresAShareOfIt)
		{
			AVeyraTestWildlife& Creature = Spawner.SpawnActorAt<AVeyraTestWildlife>(FVector(InJungleX, InJungleY, 0.0), FRotator::ZeroRotator);
			VeyraCombat::InitializeStats(*Creature.GetAbilitySystemComponent(), VeyraCombatTests::ExampleStats());
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(-InJungleX, -InJungleY, 0.0));
			UAbilitySystemComponent& Self = *Moro->GetAbilitySystemComponent();
			FVeyraRawDamageEvent Hurt;
			Hurt.Components.Add({ EVeyraDamageType::TrueDamage, Wound });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Enemy.GetAbilitySystemComponent(), Self, Hurt)));
			FVeyraRawDamageEvent Strike;
			Strike.Components.Add({ EVeyraDamageType::TrueDamage, Blow });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Self, *Enemy.GetAbilitySystemComponent(), Strike)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FArchetypeTestWorld::HealthLost(*Moro), Wound, Tolerance), TEXT("nothing from a Vanguard")));
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Self, *Creature.GetAbilitySystemComponent(), Strike)));
			const double Expected = Wound - Blow * Tuning().WildlifeHealFraction;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FArchetypeTestWorld::HealthLost(*Moro), Expected, Tolerance), FString::SanitizeFloat(FArchetypeTestWorld::HealthLost(*Moro))));
		}

		TEST_METHOD(PounceSpeedsHimForTheEnemyVanguardsItLandsOn)
		{
			// A unit world never moves a dash to its landing (Veyra.Net.ForcedMovement does), so this reads the kit.
			const FVeyraContentId Pounce = FVeyraContentId::FromText(TEXT("moro_pounce")).GetValue();
			const FVeyraDashAbilityTuning& Dash = *UVeyraAbilitiesTuningSubsystem::Get().Dash.Find(Pounce);
			ASSERT_THAT(IsTrue(Dash.EndZones.Num() == 1 && Dash.EndZones[0].CasterStatusesPerVanguard.Contains(FVeyraContentId::FromText(TEXT("moro_pounce_rush")).GetValue()),
				TEXT("its landing zone speeds him for each enemy Vanguard it catches (Roster Bible §12)")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
