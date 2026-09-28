// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Components/ActorTestSpawner.h"
#include "CQTest.h"
#include "Fluxborn/VeyraFluxborn.h"
#include "Gold/VeyraGoldComponent.h"
#include "Life/VeyraLifeComponent.h"
#include "Misc/ScopeExit.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Rewards/VeyraEconomyTuningSubsystem.h"
#include "Rewards/VeyraRewardRules.h"
#include "Rewards/VeyraRewardSubsystem.h"
#include "TimerManager.h"
#include "Structures/VeyraStructure.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tests/World/VeyraBattlegroundTestLayout.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "VeyraBattlegroundSubsystem.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraEconomyTests
{
	// Veyra.Economy.RewardRules.*: the bible's reward arithmetic (Economy & Progression Bible §3–§6).
	TEST_CLASS(RewardRules, "Veyra.Economy")
	{
		static constexpr double Tolerance = 1e-9;

		static bool Near(double A, double B)
		{
			return FMath::IsNearlyEqual(A, B, Tolerance);
		}

		TEST_METHOD(TeamFluxAddsAPercentPerStepUpToTheCap)
		{
			// The bible's prototype rule (§4): 1% per 25 active Flux, at most 10%.
			FVeyraFluxRewardBonusTuning Bonus;
			Bonus.FluxPerStep = 25.0;
			Bonus.BonusPerStep = 0.01;
			Bonus.MaxBonus = 0.1;
			ASSERT_THAT(IsTrue(Near(VeyraRewards::FluxBonusMultiplier(0.0, Bonus), 1.0) && Near(VeyraRewards::FluxBonusMultiplier(24.0, Bonus), 1.0)));
			ASSERT_THAT(IsTrue(Near(VeyraRewards::FluxBonusMultiplier(60.0, Bonus), 1.02)));
			ASSERT_THAT(IsTrue(Near(VeyraRewards::FluxBonusMultiplier(400.0, Bonus), 1.1), TEXT("capped")));
		}

		TEST_METHOD(FarmXpIsWholeAloneAndAPoolWhenShared)
		{
			// The bible's examples (§3.3): two receive 60% each, three 40% each, of a 120% pool.
			constexpr double Base = 100.0;
			constexpr double Pool = 1.2;
			ASSERT_THAT(IsTrue(Near(VeyraRewards::FarmXpShare(Base, 1, Pool), 100.0)));
			ASSERT_THAT(IsTrue(Near(VeyraRewards::FarmXpShare(Base, 2, Pool), 60.0)));
			ASSERT_THAT(IsTrue(Near(VeyraRewards::FarmXpShare(Base, 3, Pool), 40.0)));
			ASSERT_THAT(IsTrue(VeyraRewards::FarmXpShare(Base, 0, Pool) == 0.0, TEXT("nobody near: unclaimed")));
		}

		TEST_METHOD(KillXpGrowsWithTheVictimAndItsParticipants)
		{
			// Fixture values in the bible's shape (§6): 60 + 30 per level, +20% per extra participant, ×1.2 for a higher victim.
			FVeyraExperienceRewardTuning Experience;
			Experience.KillBase = 60.0;
			Experience.KillPerLevel = 30.0;
			Experience.ParticipantBonus = 0.2;
			Experience.HigherLevelVictimMultiplier = 1.2;
			ASSERT_THAT(IsTrue(Near(VeyraRewards::KillExperience(1, Experience), 60.0) && Near(VeyraRewards::KillExperience(5, Experience), 180.0)));
			ASSERT_THAT(IsTrue(Near(VeyraRewards::KillExperiencePool(100.0, 1, false, Experience), 100.0)));
			// One through five participants yield 100%, 120%, 140%, 160% or 180%.
			ASSERT_THAT(IsTrue(Near(VeyraRewards::KillExperiencePool(100.0, 5, false, Experience), 180.0)));
			ASSERT_THAT(IsTrue(Near(VeyraRewards::KillExperiencePool(100.0, 2, true, Experience), 144.0), TEXT("the multiplier applies once to the pool")));
		}

		TEST_METHOD(TheAssistPoolIsHalfTheKillSplitEvenly)
		{
			ASSERT_THAT(IsTrue(Near(VeyraRewards::AssistShare(300.0, 2, 0.5), 75.0)));
			ASSERT_THAT(IsTrue(VeyraRewards::AssistShare(300.0, 0, 0.5) == 0.0, TEXT("no assisters, no pool")));
		}

		TEST_METHOD(TheCommittedEconomyLoads)
		{
			ASSERT_THAT(IsTrue(VeyraRewards::Validate(UVeyraEconomyTuningSubsystem::Get()).IsEmpty()));
			// Every kind of Fluxborn World.json defines pays something.
			for (const TPair<FVeyraContentId, FVeyraFluxbornDefinition>& Unit : UVeyraWorldTuningSubsystem::Get().Fluxborn.Units)
			{
				ASSERT_THAT(IsTrue(UVeyraEconomyTuningSubsystem::Get().Gold.Fluxborn.Contains(Unit.Key), Unit.Key.ToString()));
			}
			// And every species of wildlife (Economy §7).
			for (const TPair<FVeyraContentId, FVeyraWildlifeSpecies>& Species : UVeyraWorldTuningSubsystem::Get().Wildlife.Species)
			{
				ASSERT_THAT(IsTrue(UVeyraEconomyTuningSubsystem::Get().Gold.Wildlife.Contains(Species.Key), Species.Key.ToString()));
			}
		}

		TEST_METHOD(WildlifeGoldAndXpComeInPairs)
		{
			FVeyraEconomyTuning Broken = UVeyraEconomyTuningSubsystem::Get();
			const FVeyraContentId Lone = FVeyraContentId::FromText(TEXT("lone_creature")).GetValue();
			Broken.Gold.Wildlife.Add(Lone, 1.0);
			const TArray<FString> Problems = VeyraRewards::Validate(Broken);
			ASSERT_THAT(IsTrue(Problems.Num() == 1 && Problems[0].StartsWith(TEXT("/experience/wildlife: lone_creature has Gold but no XP")), FString::Join(Problems, TEXT(" | "))));
		}
	};

	// Veyra.Economy.Rewards.*: Fluxborn, Vanguard kills and structures pay Gold and XP to the right
	// participants (Economy & Progression Bible §3, §5, §6, §8).
	TEST_CLASS(Rewards, "Veyra.Economy")
	{
		static constexpr double Tolerance = 1e-6;

		FActorTestSpawner Spawner;
		UVeyraBattlegroundSubsystem* Battleground = nullptr;
		AVeyraVanguardCharacter* Killer = nullptr;
		AVeyraVanguardCharacter* Ally = nullptr;

		BEFORE_EACH()
		{
			Battleground = Spawner.GetWorld().GetSubsystem<UVeyraBattlegroundSubsystem>();
			ASSERT_THAT(IsNotNull(Battleground));
			Battleground->SpawnStructures(VeyraWorldTests::CompactBattleground());
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			Killer = &Spawn(World, EVeyraTeam::A, FVector::ZeroVector);
			Ally = &Spawn(World, EVeyraTeam::A, FVector::ZeroVector);
		}

		static AVeyraVanguardCharacter& Spawn(VeyraAbilitiesTests::FArchetypeTestWorld& World, EVeyraTeam Team, const FVector& Location)
		{
			AVeyraVanguardCharacter& Vanguard = World.Spawn(Team, Location);
			Vanguard.GetPlayerState()->FindComponentByClass<UVeyraProgressionComponent>()->Initialize(FVeyraStatGrowth(), 0.0);
			return Vanguard;
		}

		static double GoldOf(const AVeyraVanguardCharacter& Vanguard)
		{
			return Vanguard.GetPlayerState()->FindComponentByClass<UVeyraGoldComponent>()->GetGold();
		}

		static double XpOf(const AVeyraVanguardCharacter& Vanguard)
		{
			return Vanguard.GetPlayerState()->FindComponentByClass<UVeyraProgressionComponent>()->GetExperience();
		}

		static void Kill(UAbilitySystemComponent& Source, UAbilitySystemComponent& Victim)
		{
			FVeyraRawDamageEvent Lethal;
			Lethal.Components.Add({ EVeyraDamageType::TrueDamage, Victim.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) });
			Lethal.Delivery = EVeyraDamageDelivery::Developer;
			VeyraCombat::DealDamage(Source, Victim, Lethal);
		}

		static FVeyraContentId Strider()
		{
			return FVeyraContentId::FromText(TEXT("strider")).GetValue();
		}

		TEST_METHOD(ALastHitPaysTheHitterAndNearbyAlliesShare)
		{
			const FVeyraEconomyTuning& Tuning = UVeyraEconomyTuningSubsystem::Get();
			AVeyraFluxborn* Enemy = Battleground->SpawnFluxborn(Strider(), EVeyraTeam::B, EVeyraLane::Mid);
			Killer->SetActorLocation(Enemy->GetActorLocation() + FVector(100.0, 0.0, 0.0));
			Ally->SetActorLocation(Enemy->GetActorLocation() - FVector(100.0, 0.0, 0.0));
			Kill(*Killer->GetAbilitySystemComponent(), *Enemy->GetAbilitySystemComponent());

			const double Gold = Tuning.Gold.Fluxborn[Strider()];
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(GoldOf(*Killer), Gold, Tolerance), TEXT("the last hit's Gold, and no share besides")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(GoldOf(*Ally), Gold * Tuning.Gold.ParticipationFraction, Tolerance)));
			const double Xp = Tuning.Experience.Fluxborn[Strider()] * Tuning.Experience.SharedPoolFraction / 2.0;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(XpOf(*Killer), Xp, Tolerance) && FMath::IsNearlyEqual(XpOf(*Ally), Xp, Tolerance), TEXT("two share one pool")));
		}

		TEST_METHOD(AFluxbornsLastHitIsUnclaimedButItsXpIsShared)
		{
			AVeyraFluxborn* Enemy = Battleground->SpawnFluxborn(Strider(), EVeyraTeam::B, EVeyraLane::Mid);
			AVeyraFluxborn* Friend = Battleground->SpawnFluxborn(Strider(), EVeyraTeam::A, EVeyraLane::Mid);
			Killer->SetActorLocation(Enemy->GetActorLocation() + FVector(100.0, 0.0, 0.0));
			Kill(*Friend->GetAbilitySystemComponent(), *Enemy->GetAbilitySystemComponent());
			ASSERT_THAT(IsTrue(GoldOf(*Killer) == 0.0, TEXT("no Vanguard fought it: no Gold at all")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(XpOf(*Killer), UVeyraEconomyTuningSubsystem::Get().Experience.Fluxborn[Strider()], Tolerance),
				TEXT("alone nearby: all of it")));
		}

		TEST_METHOD(AKillPaysTheKillerFirstBloodAndTheAssisters)
		{
			const FVeyraEconomyTuning& Tuning = UVeyraEconomyTuningSubsystem::Get();
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Victim = Spawn(World, EVeyraTeam::B, FVector(200.0, 0.0, 0.0));
			FVeyraRawDamageEvent Chip;
			Chip.Components.Add({ EVeyraDamageType::TrueDamage, 1.0 });
			VeyraCombat::DealDamage(*Ally->GetAbilitySystemComponent(), *Victim.GetAbilitySystemComponent(), Chip);
			Kill(*Killer->GetAbilitySystemComponent(), *Victim.GetAbilitySystemComponent());

			const double KillGold = Tuning.Gold.VanguardKill;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(GoldOf(*Killer), KillGold * (1.0 + Tuning.Gold.FirstBloodFraction), Tolerance)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(GoldOf(*Ally), KillGold * Tuning.Gold.AssistPoolFraction, Tolerance)));
			// Killer and a living, nearby assister share one pool.
			const double Pool = VeyraRewards::KillExperiencePool(VeyraRewards::KillExperience(1, Tuning.Experience), 2, false, Tuning.Experience);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(XpOf(*Killer), Pool / 2.0, Tolerance)));
		}

		TEST_METHOD(AFallenSpirePaysItsContributorsAndTheFirstPaysTheTeam)
		{
			const FVeyraEconomyTuning& Tuning = UVeyraEconomyTuningSubsystem::Get();
			AVeyraStructure* Outer = Battleground->FindStructure(EVeyraTeam::B, EVeyraStructureKind::LaneSpire, EVeyraLane::Mid, 0);
			Kill(*Killer->GetAbilitySystemComponent(), *Outer->GetAbilitySystemComponent());
			ASSERT_THAT(IsTrue(Outer->IsDestroyed()));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(GoldOf(*Killer), Tuning.Gold.StructurePool + Tuning.Gold.FirstStructureBonus, Tolerance)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(GoldOf(*Ally), Tuning.Gold.FirstStructureBonus, Tolerance), TEXT("the whole team takes the first bonus")));
			ASSERT_THAT(IsTrue(XpOf(*Killer) == 0.0, TEXT("structures give no XP")));
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

		TEST_METHOD(PassiveGoldPaysEveryoneOnItsTimerUntilTheMatchEnds)
		{
			// Fixture values: a payment every 10 seconds, from 30 seconds in.
			FVeyraEconomyTuning Tuning = UVeyraEconomyTuningSubsystem::Get();
			Tuning.PassiveGold.PerPayment = 9.0;
			Tuning.PassiveGold.IntervalSeconds = 10.0;
			Tuning.PassiveGold.StartSeconds = 30.0;
			UVeyraEconomyTuningSubsystem::SetTestOverride(&Tuning);
			ON_SCOPE_EXIT { UVeyraEconomyTuningSubsystem::SetTestOverride(nullptr); };
			UVeyraRewardSubsystem& Rewards = *Spawner.GetWorld().GetSubsystem<UVeyraRewardSubsystem>();
			// The dead earn it too.
			Ally->GetPlayerState()->FindComponentByClass<UVeyraLifeComponent>()->SetState(EVeyraLifeState::Dead);
			Rewards.StartPassiveGold();

			const FVeyraPassiveGoldTuning& Passive = Tuning.PassiveGold;
			Advance(Passive.StartSeconds + Passive.IntervalSeconds / 2.0);
			ASSERT_THAT(IsTrue(GoldOf(*Killer) == 0.0, TEXT("nothing before its first interval ends")));
			Advance(Passive.IntervalSeconds);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(GoldOf(*Killer), Passive.PerPayment, Tolerance)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(GoldOf(*Ally), Passive.PerPayment, Tolerance), TEXT("dead or alive")));

			// Nothing is paid once the match ends (§8.2).
			Rewards.Stop();
			Advance(Passive.IntervalSeconds * 3.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(GoldOf(*Killer), Passive.PerPayment, Tolerance)));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
