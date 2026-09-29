// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "CQTest.h"
#include "Engine/World.h"
#include "Gold/VeyraGoldComponent.h"
#include "Layout/VeyraLayout.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Statistics/VeyraMatchStatistics.h"
#include "Statistics/VeyraMatchStatisticsSubsystem.h"
#include "Statistics/VeyraScoreComponent.h"
#include "Structures/VeyraStructure.h"
#include "Tests/Abilities/VeyraTestFluxborn.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"
#include "VeyraVisionSubsystem.h"
#include "Wards/VeyraWard.h"
#include "Wells/VeyraFluxWellSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraMatchStatisticsTests
{
	// Veyra.Match.StatisticsRules.*: the arithmetic the statistics could get wrong (ADR-017 §3).
	TEST_CLASS(StatisticsRules, "Veyra.Match")
	{
		TEST_METHOD(OverlappingSpansCountOnce)
		{
			ASSERT_THAT(IsTrue(VeyraStatisticsRules::UnionSeconds({}) == 0.0));
			// Overlapping, nested, touching and apart, in any order.
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraStatisticsRules::UnionSeconds({ { 1.0, 3.0 }, { 2.0, 4.0 } }), 3.0)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraStatisticsRules::UnionSeconds({ { 0.0, 10.0 }, { 2.0, 4.0 } }), 10.0)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraStatisticsRules::UnionSeconds({ { 5.0, 6.0 }, { 1.0, 2.0 }, { 2.0, 3.0 } }), 3.0)));
			// A span that ended before it began, as one cut short by a death, adds nothing.
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraStatisticsRules::UnionSeconds({ { 4.0, 1.0 }, { 1.0, 2.0 } }), 1.0)));
		}

		TEST_METHOD(EarnedGoldCountsRewardsButNotRefundsOrDeveloperGrants)
		{
			FVeyraGoldBySource Sources;
			ASSERT_THAT(IsTrue(VeyraStatisticsRules::AddEarned(Sources, EVeyraGoldReason::Starting, 500.0)));
			ASSERT_THAT(IsTrue(VeyraStatisticsRules::AddEarned(Sources, EVeyraGoldReason::Kill, 300.0)));
			ASSERT_THAT(IsTrue(VeyraStatisticsRules::AddEarned(Sources, EVeyraGoldReason::FirstBlood, 150.0)));
			ASSERT_THAT(IsTrue(VeyraStatisticsRules::AddEarned(Sources, EVeyraGoldReason::LastHit, 21.0)));
			ASSERT_THAT(IsTrue(VeyraStatisticsRules::AddEarned(Sources, EVeyraGoldReason::Participation, 2.0)));
			ASSERT_THAT(IsTrue(VeyraStatisticsRules::AddEarned(Sources, EVeyraGoldReason::Wildlife, 80.0)));
			ASSERT_THAT(IsTrue(VeyraStatisticsRules::AddEarned(Sources, EVeyraGoldReason::FluxWell, 60.0)));
			ASSERT_THAT(IsTrue(VeyraStatisticsRules::AddEarned(Sources, EVeyraGoldReason::WardDestroyed, 30.0)));
			ASSERT_THAT(IsTrue(VeyraStatisticsRules::AddEarned(Sources, EVeyraGoldReason::Passive, 15.0)));
			ASSERT_THAT(IsFalse(VeyraStatisticsRules::AddEarned(Sources, EVeyraGoldReason::Sale, 400.0)));
			ASSERT_THAT(IsFalse(VeyraStatisticsRules::AddEarned(Sources, EVeyraGoldReason::Undo, 400.0)));
			ASSERT_THAT(IsFalse(VeyraStatisticsRules::AddEarned(Sources, EVeyraGoldReason::Developer, 400.0)));
			ASSERT_THAT(IsFalse(VeyraStatisticsRules::AddEarned(Sources, EVeyraGoldReason::Kill, -1.0)));

			ASSERT_THAT(IsTrue(Sources.Starting == 500.0 && Sources.Kills == 450.0 && Sources.Minions == 23.0 && Sources.Jungle == 80.0));
			ASSERT_THAT(IsTrue(Sources.Objectives == 60.0 && Sources.Wards == 30.0 && Sources.Passive == 15.0 && Sources.Assists == 0.0));
			ASSERT_THAT(IsTrue(Sources.Total() == 1158.0));
		}
	};

	// Veyra.Match.StatisticsRecord.*: the statistics service records what Combat, Economy, Vision and
	// World report, per participant, and publishes the public part (ADR-017 §3).
	TEST_CLASS(StatisticsRecord, "Veyra.Match")
	{
		// Fixture values, not tuning.
		static constexpr double Lethal = 100000.0;
		static constexpr double Hit = 100.0;
		static constexpr double Shield = 30.0;
		static constexpr double LongSeconds = 60.0;
		static constexpr double StunSeconds = 1.5;
		static constexpr double SlowSeconds = 3.0;
		static constexpr float Step = 0.1f;
		static constexpr double Apart = 500.0;
		static constexpr double ShortStunSeconds = 1.0;
		static constexpr double TemporaryHealth = 40.0;

		FActorTestSpawner Spawner;
		UVeyraMatchStatisticsSubsystem* Statistics = nullptr;
		AVeyraPlayerState* Attacker = nullptr;
		AVeyraPlayerState* Helper = nullptr;
		AVeyraPlayerState* Target = nullptr;
		AVeyraPlayerState* TargetAlly = nullptr;

		BEFORE_EACH()
		{
			Attacker = &Spawn(EVeyraTeam::A);
			Helper = &Spawn(EVeyraTeam::A);
			Target = &Spawn(EVeyraTeam::B);
			TargetAlly = &Spawn(EVeyraTeam::B);
			Statistics = Spawner.GetWorld().GetSubsystem<UVeyraMatchStatisticsSubsystem>();
			ASSERT_THAT(IsNotNull(Statistics));
			Statistics->Start();
			for (AVeyraPlayerState* Participant : { Attacker, Helper, Target, TargetAlly })
			{
				Statistics->AddParticipant(*Participant);
			}
		}

		AFTER_EACH()
		{
			Statistics->Stop();
		}

		AVeyraPlayerState& Spawn(EVeyraTeam Side)
		{
			UAbilitySystemComponent& Unit = VeyraCombatTests::SpawnCombatant(Spawner);
			AVeyraPlayerState& Participant = *CastChecked<AVeyraPlayerState>(Unit.GetOwner());
			Participant.SetVeyraTeam(Side);
			VeyraCombat::InitializeStats(Unit, VeyraCombatTests::ExampleStats());
			return Participant;
		}

		static UAbilitySystemComponent& Abilities(AActor& Unit)
		{
			return *CastChecked<IAbilitySystemInterface>(&Unit)->GetAbilitySystemComponent();
		}

		static FVeyraRawDamageEvent TrueDamage(double Amount, EVeyraDamageDelivery Delivery = EVeyraDamageDelivery::Ability)
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Amount });
			Damage.Delivery = Delivery;
			return Damage;
		}

		static FVeyraStatusSpec Status(const TCHAR* Id, EVeyraStatusKind Kind, double Magnitude, double Seconds)
		{
			FVeyraStatusSpec Spec;
			Spec.Id = FVeyraContentId::FromText(Id).GetValue();
			Spec.Kind = Kind;
			Spec.Magnitude = Magnitude;
			Spec.DurationSeconds = Seconds;
			return Spec;
		}

		FVeyraPlayerStatistics Of(const AVeyraPlayerState& Participant) const
		{
			return Statistics->Snapshot(Participant).Get(FVeyraPlayerStatistics());
		}

		static const FVeyraScore& ScoreOf(const AVeyraPlayerState& Participant)
		{
			return Participant.FindComponentByClass<UVeyraScoreComponent>()->GetScore();
		}

		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, Step);
			}
		}

		TEST_METHOD(KillsDeathsAndAssistsReachTheRecordAndThePublicScore)
		{
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Abilities(*Helper), Abilities(*Target), TrueDamage(Hit))));
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Abilities(*Attacker), Abilities(*Target), TrueDamage(Lethal))));

			ASSERT_THAT(IsTrue(Of(*Attacker).Kills == 1 && Of(*Attacker).Assists == 0 && Of(*Attacker).Deaths == 0));
			ASSERT_THAT(IsTrue(Of(*Helper).Assists == 1 && Of(*Helper).Kills == 0));
			ASSERT_THAT(IsTrue(Of(*Target).Deaths == 1));
			// The public score says the same, for every client's scoreboard.
			ASSERT_THAT(IsTrue(ScoreOf(*Attacker).Kills == 1 && ScoreOf(*Helper).Assists == 1 && ScoreOf(*Target).Deaths == 1));
			ASSERT_THAT(IsTrue(ScoreOf(*TargetAlly) == FVeyraScore()));
		}

		TEST_METHOD(DamageIsHealthRemovedAndShieldingGoesToItsProvider)
		{
			ASSERT_THAT(IsTrue(VeyraCombat::GrantShield(Abilities(*TargetAlly), Abilities(*Target), EVeyraShieldCategory::Universal, Shield, LongSeconds).IsValid()));
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Abilities(*Attacker), Abilities(*Target), TrueDamage(Hit))));

			// The shield took its part; the rest is damage, dealt and taken (Match Statistics Bible §3).
			const FVeyraPlayerStatistics Dealt = Of(*Attacker);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Dealt.DamageDealt.TrueDamage, Hit - Shield) && Dealt.DamageDealt.Physical == 0.0));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Dealt.VanguardDamage, Hit - Shield), TEXT("an enemy Vanguard's Health counts apart")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Of(*Target).DamageTaken.Total(), Hit - Shield)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Of(*TargetAlly).DamageShielded, Shield), TEXT("shielding belongs to the shield's provider")));
			ASSERT_THAT(IsTrue(Of(*Target).DamageShielded == 0.0));
		}

		TEST_METHOD(HealingIsSelfOrTeammateAndNeverOverheal)
		{
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Abilities(*Attacker), Abilities(*Target), TrueDamage(Hit))));
			// Only what was missing is restored (§3).
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraCombat::RestoreHealthFrom(Abilities(*TargetAlly), Abilities(*Target), Hit * 0.5), Hit * 0.5)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraCombat::RestoreHealthFrom(Abilities(*Target), Abilities(*Target), Hit), Hit * 0.5)));
			// Regeneration and the fountain are no one's healing (ADR-017 §9.2).
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Abilities(*Attacker), Abilities(*Target), TrueDamage(Hit))));
			ASSERT_THAT(IsTrue(VeyraCombat::RestoreHealth(Abilities(*Target), Hit)));

			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Of(*TargetAlly).TeammateHealing, Hit * 0.5) && Of(*TargetAlly).SelfHealing == 0.0));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Of(*Target).SelfHealing, Hit * 0.5) && Of(*Target).TeammateHealing == 0.0));
		}

		TEST_METHOD(CrowdControlCountsOverlapsOnceAndEndsWithItsTarget)
		{
			UWorld& World = Spawner.GetWorld();
			const double StunStart = World.GetTimeSeconds();
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Abilities(*Attacker), Abilities(*Target), Status(TEXT("test_stun"), EVeyraStatusKind::Stun, 0.0, StunSeconds))));
			Wait(StunSeconds / 2.0);
			// Stunned again while stunned: the overlap counts once (§4).
			const double RestunAt = World.GetTimeSeconds();
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Abilities(*Attacker), Abilities(*Target), Status(TEXT("test_stun"), EVeyraStatusKind::Stun, 0.0, StunSeconds))));
			// Crowd control on an ally is not crowd control on an enemy Vanguard.
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Abilities(*Attacker), Abilities(*Helper), Status(TEXT("test_slow"), EVeyraStatusKind::Slow, 0.3, SlowSeconds))));
			Wait(StunSeconds * 2.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Of(*Attacker).CrowdControl.Stun, RestunAt + StunSeconds - StunStart, 1e-3)));
			ASSERT_THAT(IsTrue(Of(*Attacker).CrowdControl.Slow == 0.0));

			// A slow counts only until its target dies (Combat Bible §18), and only what has elapsed.
			const double SlowStart = World.GetTimeSeconds();
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Abilities(*Helper), Abilities(*Target), Status(TEXT("test_slow"), EVeyraStatusKind::Slow, 0.3, SlowSeconds))));
			Wait(SlowSeconds / 3.0);
			ASSERT_THAT(IsTrue(Of(*Helper).CrowdControl.Slow < SlowSeconds / 2.0, TEXT("not ahead of the clock")));
			const double DiedAt = World.GetTimeSeconds();
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Abilities(*Attacker), Abilities(*Target), TrueDamage(Lethal))));
			Wait(SlowSeconds);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Of(*Helper).CrowdControl.Slow, DiedAt - SlowStart, 1e-3)));
		}

		TEST_METHOD(AStunAndASlowAtOnceCountOnceInTheTotal)
		{
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Abilities(*Attacker), Abilities(*Target), Status(TEXT("test_stun"), EVeyraStatusKind::Stun, 0.0, ShortStunSeconds))));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Abilities(*Attacker), Abilities(*Target), Status(TEXT("test_slow"), EVeyraStatusKind::Slow, 0.3, StunSeconds))));
			Wait(StunSeconds * 2.0);
			const FVeyraCrowdControlByKind CrowdControl = Of(*Attacker).CrowdControl;
			// By kind, each its own; in total, the slow covers the stun (§4).
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(CrowdControl.Stun, ShortStunSeconds, 1e-3) && FMath::IsNearlyEqual(CrowdControl.Slow, StunSeconds, 1e-3)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(CrowdControl.Total, StunSeconds, 1e-3), FString::Printf(TEXT("total %g"), CrowdControl.Total)));
		}

		TEST_METHOD(TemporaryHealthIsHealthSoWhatItTakesIsDamage)
		{
			ASSERT_THAT(IsTrue(VeyraCombat::GrantTemporaryHealth(Abilities(*TargetAlly), Abilities(*Target), TemporaryHealth, LongSeconds).IsValid()));
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Abilities(*Attacker), Abilities(*Target), TrueDamage(Hit))));
			// Temporary Health is not a shield (Combat Bible §7): the whole hit is damage, and no one shielded it.
			const FVeyraPlayerStatistics Dealt = Of(*Attacker);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Dealt.DamageDealt.TrueDamage, Hit) && FMath::IsNearlyEqual(Dealt.VanguardDamage, Hit), FString::Printf(TEXT("dealt %g"), Dealt.VanguardDamage)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Of(*Target).DamageTaken.Total(), Hit) && Of(*TargetAlly).DamageShielded == 0.0));
		}

		TEST_METHOD(LastHitsCountFluxbornAndJungleApart)
		{
			AVeyraTestFluxborn& Minion = Spawner.SpawnActorAt<AVeyraTestFluxborn>(FVector(Apart, 0.0, 0.0), FRotator::ZeroRotator);
			Minion.SetVeyraTeam(EVeyraTeam::B);
			VeyraCombat::InitializeStats(Abilities(Minion), VeyraCombatTests::ExampleStats());
			AVeyraTestFluxborn& AlliedMinion = Spawner.SpawnActorAt<AVeyraTestFluxborn>(FVector(2.0 * Apart, 0.0, 0.0), FRotator::ZeroRotator);
			AlliedMinion.SetVeyraTeam(EVeyraTeam::A);
			VeyraCombat::InitializeStats(Abilities(AlliedMinion), VeyraCombatTests::ExampleStats());
			AVeyraTestFluxborn& Unhit = Spawner.SpawnActorAt<AVeyraTestFluxborn>(FVector(3.0 * Apart, 0.0, 0.0), FRotator::ZeroRotator);
			Unhit.SetVeyraTeam(EVeyraTeam::B);
			VeyraCombat::InitializeStats(Abilities(Unhit), VeyraCombatTests::ExampleStats());
			AVeyraTestWildlife& Creature = Spawner.SpawnActorAt<AVeyraTestWildlife>(FVector(4.0 * Apart, 0.0, 0.0), FRotator::ZeroRotator);
			VeyraCombat::InitializeStats(Abilities(Creature), VeyraCombatTests::ExampleStats());

			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Abilities(*Attacker), Abilities(Minion), TrueDamage(Lethal))));
			// A Fluxborn's last hit is no one's when a Fluxborn lands it (§2).
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Abilities(*Attacker), Abilities(Unhit), TrueDamage(Hit))));
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Abilities(AlliedMinion), Abilities(Unhit), TrueDamage(Lethal))));
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Abilities(*Attacker), Abilities(Creature), TrueDamage(Lethal))));

			ASSERT_THAT(IsTrue(Of(*Attacker).MinionKills == 1 && Of(*Attacker).JungleKills == 1 && Of(*Attacker).Kills == 0));
			ASSERT_THAT(IsTrue(ScoreOf(*Attacker).MinionKills == 1 && ScoreOf(*Attacker).JungleKills == 1));
		}

		TEST_METHOD(TowerDamageIsLaneSpiresAndBaseTowersOnly)
		{
			AVeyraStructure& Spire = SpawnStructure(EVeyraStructureKind::LaneSpire);
			AVeyraStructure& Inhibitor = SpawnStructure(EVeyraStructureKind::Inhibitor);
			const double SpireBefore = Abilities(Spire).GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
			const double InhibitorBefore = Abilities(Inhibitor).GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Abilities(*Attacker), Abilities(Spire), TrueDamage(Hit, EVeyraDamageDelivery::BasicAttack))));
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Abilities(*Attacker), Abilities(Inhibitor), TrueDamage(Hit, EVeyraDamageDelivery::BasicAttack))));
			const double SpireLost = SpireBefore - Abilities(Spire).GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
			const double InhibitorLost = InhibitorBefore - Abilities(Inhibitor).GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
			ASSERT_THAT(IsTrue(SpireLost > 0.0 && InhibitorLost > 0.0));

			const FVeyraPlayerStatistics Dealt = Of(*Attacker);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Dealt.TowerDamage, SpireLost), TEXT("an inhibitor is no tower")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Dealt.DamageDealt.Total(), SpireLost + InhibitorLost) && Dealt.VanguardDamage == 0.0));
		}

		AVeyraStructure& SpawnStructure(EVeyraStructureKind Kind)
		{
			FVeyraStructurePlacement Placement;
			Placement.Kind = Kind;
			Placement.Team = EVeyraTeam::B;
			Placement.Lane = EVeyraLane::Mid;
			AVeyraStructure* Structure = Spawner.GetWorld().SpawnActorDeferred<AVeyraStructure>(AVeyraStructure::StaticClass(), FTransform::Identity);
			check(Structure);
			Structure->Configure(Placement);
			Structure->FinishSpawning(FTransform::Identity);
			verify(Structure->InitializeStats());
			return *Structure;
		}

		TEST_METHOD(EarnedGoldIsEveryRewardFromTheStartAndNoRefund)
		{
			UVeyraGoldComponent& Gold = *Attacker->FindComponentByClass<UVeyraGoldComponent>();
			ASSERT_THAT(IsTrue(Gold.Grant(500.0, EVeyraGoldReason::Starting) && Gold.Grant(21.0, EVeyraGoldReason::LastHit)));
			ASSERT_THAT(IsTrue(Gold.Grant(300.0, EVeyraGoldReason::Kill) && Gold.Grant(100.0, EVeyraGoldReason::Sale)));
			ASSERT_THAT(IsTrue(Gold.Grant(1000.0, EVeyraGoldReason::Developer) && Gold.Spend(400.0)));

			FVeyraPlayerStatistics Earned = Of(*Attacker);
			// Spending never reduces what was earned; the sources sum to the total (§7).
			ASSERT_THAT(IsTrue(Earned.GoldEarned == 821.0 && Earned.GoldBySource.Starting == 500.0 && Earned.GoldBySource.Minions == 21.0));
			ASSERT_THAT(IsTrue(Earned.GoldBySource.Kills == 300.0 && Earned.GoldEarned == Earned.GoldBySource.Total()));

			// Nothing counts once the match has ended.
			Statistics->Stop();
			ASSERT_THAT(IsTrue(Gold.Grant(15.0, EVeyraGoldReason::Passive)));
			ASSERT_THAT(IsTrue(Of(*Attacker).GoldEarned == 821.0));
		}

		TEST_METHOD(WardsComeFromVisionAndWellsFromWorld)
		{
			UVeyraVisionSubsystem* Vision = Spawner.GetWorld().GetSubsystem<UVeyraVisionSubsystem>();
			ASSERT_THAT(IsNotNull(Vision));
			AVeyraWard* Ward = Vision->PlaceWard(*Target, FVector::ZeroVector);
			ASSERT_THAT(IsNotNull(Ward));
			ASSERT_THAT(IsTrue(Of(*Target).WardsPlaced == 1));
			// A ward falls to basic attacks, one hit each (ADR-016 §6).
			for (int32 Hits = 0; Hits < 10 && Ward->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute()) > 0.0; ++Hits)
			{
				VeyraCombat::DealDamage(Abilities(*Attacker), *Ward->GetAbilitySystemComponent(), TrueDamage(Hit, EVeyraDamageDelivery::BasicAttack));
			}
			ASSERT_THAT(IsTrue(Of(*Attacker).WardsDestroyed == 1));
			ASSERT_THAT(IsTrue(Of(*Attacker).DamageDealt.Total() == 0.0, TEXT("a ward counts hits, not damage")));

			UVeyraFluxWellSubsystem* Wells = Spawner.GetWorld().GetSubsystem<UVeyraFluxWellSubsystem>();
			ASSERT_THAT(IsNotNull(Wells));
			FVeyraFluxWellSecuredEvent Secured;
			Secured.Team = EVeyraTeam::A;
			Secured.Capturers = { &Abilities(*Attacker), &Abilities(*Helper) };
			Secured.FinalHitter = &Abilities(*Helper);
			Wells->OnFluxWellSecured.Broadcast(Secured);
			ASSERT_THAT(IsTrue(Of(*Attacker).WellsSecured == 1 && Of(*Attacker).WellFinalHits == 0));
			ASSERT_THAT(IsTrue(Of(*Helper).WellsSecured == 1 && Of(*Helper).WellFinalHits == 1));
		}

		TEST_METHOD(EachParticipantIsRecordedOnce)
		{
			Statistics->AddParticipant(*Attacker);
			UVeyraGoldComponent& Gold = *Attacker->FindComponentByClass<UVeyraGoldComponent>();
			ASSERT_THAT(IsTrue(Gold.Grant(21.0, EVeyraGoldReason::LastHit)));
			ASSERT_THAT(IsTrue(Of(*Attacker).GoldEarned == 21.0));
			// One who was never added has no record.
			ASSERT_THAT(IsFalse(Statistics->Snapshot(Spawn(EVeyraTeam::A)).IsSet()));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
