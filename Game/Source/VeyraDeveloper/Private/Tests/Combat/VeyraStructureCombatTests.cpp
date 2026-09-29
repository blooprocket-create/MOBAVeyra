// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraVitalsSet.h"
#include "CQTest.h"
#include "Effects/VeyraCombatEffects.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Life/VeyraKillCredit.h"
#include "Life/VeyraLifeComponent.h"
#include "Regeneration/VeyraRegenerationComponent.h"
#include "Shapes/VeyraShapes.h"
#include "Targeting/VeyraTargeting.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraStructureCombatTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	FVeyraRawDamageEvent DeliveredDamage(EVeyraDamageType Type, double Amount, EVeyraDamageDelivery Delivery)
	{
		FVeyraRawDamageEvent Damage;
		Damage.Components.Add({ Type, Amount });
		Damage.Delivery = Delivery;
		return Damage;
	}

	FVeyraStatusSpec StructureTestStatus(const TCHAR* Id, EVeyraStatusKind Kind, double Magnitude, double DurationSeconds)
	{
		FVeyraStatusSpec Spec;
		Spec.Id = FVeyraContentId::FromText(Id).GetValue();
		Spec.Kind = Kind;
		Spec.Magnitude = Magnitude;
		Spec.DurationSeconds = DurationSeconds;
		return Spec;
	}

	UAbilitySystemComponent& AbilitySystemOf(AActor& Unit)
	{
		return *UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
	}

	// Veyra.Combat.StructureDamage.*: basic attacks and tower attacks damage structures; abilities,
	// procs, enemy statuses, penetration and areas do not reach them (Combat Bible §33, §55; ADR-011 §5).
	TEST_CLASS(StructureDamage, "Veyra.Combat")
	{
		// Fixture values, not tuning.
		static constexpr double Hit = 100.0;
		static constexpr double LongSeconds = 60.0;
		static constexpr double Tolerance = 1e-3;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Attacker = nullptr;
		AVeyraTestStructure* Spire = nullptr;
		AVeyraTestFluxborn* Minion = nullptr;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Attacker = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Spire = &World.SpawnStructure(EVeyraTeam::B, FVector(300.0, 0.0, 0.0));
			Minion = &World.SpawnFluxborn(EVeyraTeam::B, FVector(0.0, 300.0, 0.0));
		}

		bool Deal(AActor& Target, const FVeyraRawDamageEvent& Damage) const
		{
			return VeyraCombat::DealDamage(AbilitySystemOf(*Attacker), AbilitySystemOf(Target), Damage);
		}

		TEST_METHOD(OnlyBasicTowerAndDeveloperDamageReachAStructure)
		{
			for (const EVeyraDamageDelivery Refused : { EVeyraDamageDelivery::Ability, EVeyraDamageDelivery::Proc })
			{
				ASSERT_THAT(IsFalse(Deal(*Spire, DeliveredDamage(EVeyraDamageType::TrueDamage, Hit, Refused))));
			}
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(*Spire) == 0.0, TEXT("abilities and procs do not damage structures")));
			ASSERT_THAT(IsTrue(Deal(*Minion, DeliveredDamage(EVeyraDamageType::TrueDamage, Hit, EVeyraDamageDelivery::Ability)),
				TEXT("an ability still damages anything else")));

			double Expected = 0.0;
			for (const EVeyraDamageDelivery Allowed : { EVeyraDamageDelivery::BasicAttack, EVeyraDamageDelivery::StructureAttack, EVeyraDamageDelivery::Developer })
			{
				ASSERT_THAT(IsTrue(Deal(*Spire, DeliveredDamage(EVeyraDamageType::TrueDamage, Hit, Allowed))));
				Expected += Hit;
				ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FArchetypeTestWorld::HealthLost(*Spire), Expected, Tolerance)));
			}
		}

		TEST_METHOD(PenetrationDoesNotReachAStructuresArmor)
		{
			const double Armor = VeyraCombatTests::ExampleStats().Armor;
			FVeyraRawDamageEvent Piercing = DeliveredDamage(EVeyraDamageType::Physical, Hit, EVeyraDamageDelivery::BasicAttack);
			Piercing.PhysicalPenetration.Flat = Armor;

			ASSERT_THAT(IsTrue(Deal(*Minion, Piercing)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FArchetypeTestWorld::HealthLost(*Minion), Hit, Tolerance), TEXT("penetration strips a unit's armour")));

			ASSERT_THAT(IsTrue(Deal(*Spire, Piercing)));
			const double K = UVeyraCombatTuningSubsystem::Get().Resistance.MitigationConstant;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FArchetypeTestWorld::HealthLost(*Spire), Hit * K / (K + Armor), Tolerance),
				TEXT("a structure keeps its own armour")));
		}

		TEST_METHOD(EnemyStatusesDoNotAffectAStructureButItsOwnDo)
		{
			UAbilitySystemComponent& Structure = AbilitySystemOf(*Spire);
			ASSERT_THAT(IsFalse(VeyraCombat::ApplyStatus(AbilitySystemOf(*Attacker), Structure,
				StructureTestStatus(TEXT("chill"), EVeyraStatusKind::Slow, 0.3, LongSeconds))));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Structure, Structure,
				StructureTestStatus(TEXT("fortify"), EVeyraStatusKind::DamageReduction, 0.5, LongSeconds)), TEXT("backdoor protection is a structure's own status")));
		}

		TEST_METHOD(AbilitiesCannotTargetAStructureButBasicAttacksCan)
		{
			const double Range = 1000.0;
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(*Attacker, Spire, Range) == EVeyraTargetValidity::Structure));
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(*Attacker, Spire, Range, EVeyraStructureTargeting::Allow) == EVeyraTargetValidity::Valid));
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(*Attacker, Minion, Range) == EVeyraTargetValidity::Valid));
		}

		TEST_METHOD(AreasDoNotGatherStructures)
		{
			FVeyraShape Everything;
			Everything.Kind = EVeyraShapeKind::Circle;
			Everything.Radius = 1000.0;
			const FVeyraPlacedShape Placed{ Everything, FVector::ZeroVector, FVector::ForwardVector };
			const TArray<AActor*> Gathered = VeyraShapes::GatherUnits(Spawner.GetWorld(), Placed, [](const AActor&) { return true; });
			ASSERT_THAT(IsTrue(Gathered.Contains(Minion) && !Gathered.Contains(Spire)));
			const TArray<AActor*> ForAttackMove = VeyraShapes::GatherUnits(Spawner.GetWorld(), Placed, [](const AActor&) { return true; },
				EVeyraStructureTargeting::Allow);
			ASSERT_THAT(IsTrue(ForAttackMove.Contains(Spire), TEXT("an attack-move may pick a structure")));
		}

		TEST_METHOD(InvulnerabilityGrantsCountAndHoldDamageOff)
		{
			UAbilitySystemComponent& Structure = AbilitySystemOf(*Spire);
			const FVeyraRawDamageEvent Damage = DeliveredDamage(EVeyraDamageType::TrueDamage, Hit, EVeyraDamageDelivery::BasicAttack);
			VeyraCombat::GrantInvulnerability(Structure);
			VeyraCombat::GrantInvulnerability(Structure);
			ASSERT_THAT(IsTrue(VeyraCombat::IsInvulnerable(Structure)));
			Deal(*Spire, Damage);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(*Spire) == 0.0));

			VeyraCombat::RevokeInvulnerability(Structure);
			ASSERT_THAT(IsTrue(VeyraCombat::IsInvulnerable(Structure), TEXT("each grant needs its own revoke")));
			VeyraCombat::RevokeInvulnerability(Structure);
			ASSERT_THAT(IsFalse(VeyraCombat::IsInvulnerable(Structure)));
			Deal(*Spire, Damage);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FArchetypeTestWorld::HealthLost(*Spire), Hit, Tolerance)));
		}

		TEST_METHOD(HostileDamageIsAnnouncedWithItsDelivery)
		{
			UVeyraCombatEventSubsystem* Events = Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>();
			TArray<FVeyraHostileDamageEvent> Heard;
			const FDelegateHandle Handle = Events->OnHostileDamage.AddLambda([&Heard](const FVeyraHostileDamageEvent& Event) { Heard.Add(Event); });
			Deal(*Spire, DeliveredDamage(EVeyraDamageType::TrueDamage, Hit, EVeyraDamageDelivery::BasicAttack));
			Deal(*Spire, DeliveredDamage(EVeyraDamageType::TrueDamage, Hit, EVeyraDamageDelivery::Ability));
			VeyraCombat::DealDamage(AbilitySystemOf(*Attacker), AbilitySystemOf(*Attacker), DeliveredDamage(EVeyraDamageType::TrueDamage, Hit, EVeyraDamageDelivery::Ability));
			Events->OnHostileDamage.Remove(Handle);
			ASSERT_THAT(AreEqual(1, Heard.Num()));
			ASSERT_THAT(IsTrue(Heard[0].Target.Get() == &AbilitySystemOf(*Spire) && Heard[0].Delivery == EVeyraDamageDelivery::BasicAttack,
				TEXT("refused and self-inflicted damage is no hostile damage")));
		}
	};

	// Veyra.Combat.KillCredit.*: who is credited when something other than an enemy Vanguard finishes
	// a unit, and who gets a takedown (Combat Bible §18; ADR-011 §6).
	TEST_CLASS(KillCredit, "Veyra.Combat")
	{
		static constexpr double Lethal = 100000.0;
		static constexpr double Graze = 10.0;
		static constexpr double LongSeconds = 60.0;
		static constexpr double Tolerance = 1e-3;

		FActorTestSpawner Spawner;
		TArray<FVeyraDeathEvent> Deaths;
		FDelegateHandle DeathHandle;

		BEFORE_EACH()
		{
			DeathHandle = Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnDeath.AddLambda(
				[this](const FVeyraDeathEvent& Event) { Deaths.Add(Event); });
		}

		AFTER_EACH()
		{
			Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnDeath.Remove(DeathHandle);
		}

		static bool Hurt(AActor& Source, AActor& Target, double Amount)
		{
			return VeyraCombat::DealDamage(AbilitySystemOf(Source), AbilitySystemOf(Target),
				DeliveredDamage(EVeyraDamageType::TrueDamage, Amount, EVeyraDamageDelivery::BasicAttack));
		}

		TEST_METHOD(TheRuleCreditsTheLethalEnemyVanguardElseTheLatestContributorInTheWindow)
		{
			FArchetypeTestWorld World{ Spawner };
			UAbilitySystemComponent& Early = AbilitySystemOf(World.Spawn(EVeyraTeam::B, FVector::ZeroVector));
			UAbilitySystemComponent& Late = AbilitySystemOf(World.Spawn(EVeyraTeam::B, FVector::ZeroVector));
			UAbilitySystemComponent& Tower = AbilitySystemOf(World.SpawnStructure(EVeyraTeam::B, FVector::ZeroVector));
			constexpr double Window = 10.0;
			const TArray<FVeyraContribution> Contributions = { { &Early, 1.0 }, { &Late, 5.0 } };

			ASSERT_THAT(IsTrue(VeyraKillCredit::Resolve(&Early, true, Contributions, 12.0, Window) == &Early, TEXT("the killer itself")));
			ASSERT_THAT(IsTrue(VeyraKillCredit::Resolve(&Tower, false, Contributions, 12.0, Window) == &Late, TEXT("the latest in the window")));
			ASSERT_THAT(IsTrue(VeyraKillCredit::Resolve(&Tower, false, Contributions, 12.5, Window) == &Late));
			ASSERT_THAT(IsTrue(VeyraKillCredit::Resolve(&Tower, false, Contributions, 15.5, Window) == nullptr, TEXT("an Execution")));
			ASSERT_THAT(IsTrue(VeyraKillCredit::Resolve(nullptr, false, {}, 0.0, Window) == nullptr));
		}

		TEST_METHOD(ATowerFinishingAVanguardCreditsTheEnemyWhoFoughtIt)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Victim = World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Diver = World.Spawn(EVeyraTeam::B, FVector::ZeroVector);
			AVeyraTestStructure& Tower = World.SpawnStructure(EVeyraTeam::B, FVector::ZeroVector);

			ASSERT_THAT(IsTrue(Hurt(Diver, Victim, Graze)));
			ASSERT_THAT(IsTrue(Hurt(Tower, Victim, Lethal)));
			ASSERT_THAT(AreEqual(1, Deaths.Num()));
			ASSERT_THAT(IsTrue(Deaths[0].Killer.Get() == &AbilitySystemOf(Tower), TEXT("the tower dealt the lethal damage")));
			ASSERT_THAT(IsTrue(Deaths[0].CreditedKiller.Get() == &AbilitySystemOf(Diver), TEXT("the diver gets the kill")));
			ASSERT_THAT(IsTrue(Deaths[0].Assisters.IsEmpty(), TEXT("the credited killer is not also an assister")));
		}

		TEST_METHOD(AnUncontestedEnvironmentalDeathIsAnExecution)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Victim = World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraTestFluxborn& Minion = World.SpawnFluxborn(EVeyraTeam::B, FVector::ZeroVector);
			ASSERT_THAT(IsTrue(Hurt(Minion, Victim, Lethal)));
			ASSERT_THAT(AreEqual(1, Deaths.Num()));
			ASSERT_THAT(IsNull(Deaths[0].CreditedKiller.Get()));
		}

		TEST_METHOD(ANonVanguardVictimRecordsItsContributorsButGrantsNoTakedown)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Farmer = World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Helper = World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraTestFluxborn& Minion = World.SpawnFluxborn(EVeyraTeam::B, FVector::ZeroVector);
			constexpr double Duration = 10.0;
			FVeyraStatusSpec Frenzy = StructureTestStatus(TEXT("frenzy"), EVeyraStatusKind::AttackSpeed, 0.5, Duration);
			Frenzy.TakedownExtensionSeconds = 2.0;
			Frenzy.TakedownExtensionMaxSeconds = 3.0;
			UAbilitySystemComponent& FarmerUnit = AbilitySystemOf(Farmer);
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(FarmerUnit, FarmerUnit, Frenzy)));

			ASSERT_THAT(IsTrue(Hurt(Helper, Minion, Graze)));
			ASSERT_THAT(IsTrue(Hurt(Farmer, Minion, Lethal)));
			ASSERT_THAT(AreEqual(1, Deaths.Num()));
			const FVeyraDeathEvent& Death = Deaths[0];
			ASSERT_THAT(IsTrue(Death.CreditedKiller.Get() == &FarmerUnit));
			ASSERT_THAT(IsTrue(Death.Assisters.IsEmpty(), TEXT("assists are for Vanguard victims")));
			ASSERT_THAT(AreEqual(2, Death.Contributions.Num()));
			ASSERT_THAT(IsTrue(Death.Contributions.ContainsByPredicate([&Helper](const FVeyraContribution& Contribution) {
				return Contribution.Contributor.Get() == &AbilitySystemOf(Helper); })));

			FGameplayEffectQuery Query;
			Query.EffectDefinition = UVeyraStatusEffect::StaticClass();
			const TArray<float> Remaining = FarmerUnit.GetActiveEffectsTimeRemaining(Query);
			ASSERT_THAT(IsTrue(Remaining.Num() == 1 && FMath::IsNearlyEqual(Remaining[0], Duration, Tolerance), TEXT("killing a minion is no takedown")));
		}
	};

	// Veyra.Combat.HealthRestoration.*: restoring Health and Health Regeneration (Combat Bible §6;
	// author ruling 2026-09-28, ADR-011 §11).
	TEST_CLASS(HealthRestoration, "Veyra.Combat")
	{
		static constexpr double Wound = 100.0;
		static constexpr double Tolerance = 1e-3;

		FActorTestSpawner Spawner;
		UAbilitySystemComponent* Unit = nullptr;

		BEFORE_EACH()
		{
			Unit = &VeyraCombatTests::SpawnCombatant(Spawner);
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(*Unit, VeyraCombatTests::ExampleStats())));
		}

		double Missing() const
		{
			return VeyraCombat::GetMissingHealth(*Unit);
		}

		void Wounded() const
		{
			VeyraCombat::DealDamage(*Unit, *Unit, DeliveredDamage(EVeyraDamageType::TrueDamage, Wound, EVeyraDamageDelivery::Developer));
		}

		TEST_METHOD(RestoringHealthNeverOverheals)
		{
			Wounded();
			ASSERT_THAT(IsTrue(VeyraCombat::RestoreHealth(*Unit, Wound / 4.0)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Missing(), Wound * 3.0 / 4.0, Tolerance)));
			ASSERT_THAT(IsTrue(VeyraCombat::RestoreHealth(*Unit, Wound * 10.0)));
			ASSERT_THAT(IsTrue(Missing() == 0.0));
		}

		TEST_METHOD(TheDeadRestoreNothing)
		{
			Unit->GetOwner()->FindComponentByClass<UVeyraLifeComponent>()->SetState(EVeyraLifeState::Dead);
			ASSERT_THAT(IsFalse(VeyraCombat::RestoreHealth(*Unit, Wound)));
		}

		TEST_METHOD(HealthRegenerationRestoresItsRatePerSecondAndGrowsByLevel)
		{
			constexpr double Regeneration = 2.0;
			constexpr double Growth = 0.5;
			constexpr double Seconds = 5.0;
			FVeyraStatBlock Stats = VeyraCombatTests::ExampleStats();
			Stats.HealthRegen = Regeneration;
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(*Unit, Stats)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Unit->GetNumericAttribute(UVeyraVitalsSet::GetHealthRegenAttribute()), Regeneration, Tolerance)));

			Wounded();
			UVeyraRegenerationComponent* Regenerator = Unit->GetOwner()->FindComponentByClass<UVeyraRegenerationComponent>();
			ASSERT_THAT(IsNotNull(Regenerator));
			Regenerator->ApplyTick(Seconds);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Missing(), Wound - Regeneration * Seconds, Tolerance)));

			FVeyraStatBlock PerLevel;
			PerLevel.HealthRegen = Growth;
			ASSERT_THAT(IsTrue(VeyraCombat::GrowBaseStats(*Unit, PerLevel)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Unit->GetNumericAttribute(UVeyraVitalsSet::GetHealthRegenAttribute()), Regeneration + Growth, Tolerance)));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
