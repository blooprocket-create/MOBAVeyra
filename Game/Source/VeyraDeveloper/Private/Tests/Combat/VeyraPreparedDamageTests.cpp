// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "CQTest.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	// Veyra.Combat.PreparedDamage.*: damage prepared at Commit keeps its source's offence and meets
	// each target's defences when it lands; an event can carry its own penetration (Combat Bible §3,
	// §50; ADR-009 §4).
	TEST_CLASS(PreparedDamage, "Veyra.Combat")
	{
		// Fixture values: a physical hit into a known Armor.
		static constexpr double Hit = 100.0;
		static constexpr double DefenderArmor = 100.0;

		FActorTestSpawner Spawner;
		UAbilitySystemComponent* Attacker = nullptr;
		UAbilitySystemComponent* Defender = nullptr;

		BEFORE_EACH()
		{
			Attacker = &SpawnCombatant(Spawner);
			Defender = &SpawnDefender();
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(*Attacker, ExampleStats())));
		}

		UAbilitySystemComponent& SpawnDefender()
		{
			UAbilitySystemComponent& Unit = SpawnCombatant(Spawner);
			VeyraCombat::InitializeStats(Unit, ExampleStats());
			Unit.SetNumericAttributeBase(UVeyraDefenceSet::GetArmorAttribute(), static_cast<float>(DefenderArmor));
			return Unit;
		}

		static FVeyraRawDamageEvent PhysicalHit()
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::Physical, Hit });
			return Damage;
		}

		static double HealthLost(const UAbilitySystemComponent& Unit)
		{
			return Unit.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) - Unit.GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
		}

		/** What Hit does through Armor, by the §3 formula with the loaded constant. */
		static double Mitigated(double Armor)
		{
			const double K = UVeyraCombatTuningSubsystem::Get().Resistance.MitigationConstant;
			return Hit * K / (K + Armor);
		}

		TEST_METHOD(TheSourcesOffenceIsFixedWhenPrepared)
		{
			const FVeyraPreparedDamage Prepared = VeyraCombat::PrepareDamage(*Attacker, PhysicalHit());
			ASSERT_THAT(IsTrue(Prepared.IsValid()));
			ApplyToSelf(*Attacker, NewTestEffect(EGameplayEffectDurationType::Infinite,
				{ { UVeyraOffenceSet::GetOutgoingDamageMultiplierAttribute(), EGameplayModOp::MultiplyCompound, 1.5f } }));
			ASSERT_THAT(IsTrue(VeyraCombat::DealPreparedDamage(Prepared, *Defender)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthLost(*Defender), Mitigated(DefenderArmor), 1e-3),
				TEXT("amplification gained after Commit strengthened the damage")));
		}

		TEST_METHOD(TheTargetsDefenceIsReadWhenDealt)
		{
			constexpr double RaisedArmor = 300.0;
			const FVeyraPreparedDamage Prepared = VeyraCombat::PrepareDamage(*Attacker, PhysicalHit());
			Defender->SetNumericAttributeBase(UVeyraDefenceSet::GetArmorAttribute(), static_cast<float>(RaisedArmor));
			ASSERT_THAT(IsTrue(VeyraCombat::DealPreparedDamage(Prepared, *Defender)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthLost(*Defender), Mitigated(RaisedArmor), 1e-3)));
		}

		TEST_METHOD(OnePreparationCanHitSeveralTargets)
		{
			UAbilitySystemComponent& Second = SpawnDefender();
			const FVeyraPreparedDamage Prepared = VeyraCombat::PrepareDamage(*Attacker, PhysicalHit());
			ASSERT_THAT(IsTrue(VeyraCombat::DealPreparedDamage(Prepared, *Defender)));
			ASSERT_THAT(IsTrue(VeyraCombat::DealPreparedDamage(Prepared, Second)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthLost(*Defender), Mitigated(DefenderArmor), 1e-3)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthLost(Second), Mitigated(DefenderArmor), 1e-3)));
		}

		TEST_METHOD(TheEventsPenetrationJoinsTheAttackers)
		{
			constexpr double EventRetained = 0.6;
			constexpr double AttackerRetained = 0.5;
			constexpr double EventFlat = 10.0;
			ApplyToSelf(*Attacker, NewTestEffect(EGameplayEffectDurationType::Infinite,
				{ { UVeyraOffenceSet::GetPhysicalPenetrationRetainedAttribute(), EGameplayModOp::MultiplyCompound, static_cast<float>(AttackerRetained) } }));
			FVeyraRawDamageEvent Damage = PhysicalHit();
			Damage.PhysicalPenetration.Retained = EventRetained;
			Damage.PhysicalPenetration.Flat = EventFlat;
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Attacker, *Defender, Damage)));
			// Percentage penetration from both multiplies, then the flat penetration subtracts (§3).
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthLost(*Defender), Mitigated(DefenderArmor * AttackerRetained * EventRetained - EventFlat), 1e-3)));
		}

		TEST_METHOD(RefusesPenetrationOutsideItsRange)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("Refused damage"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 2);
			FVeyraRawDamageEvent TooMuch = PhysicalHit();
			TooMuch.MagicPenetration.Retained = 1.5;
			FVeyraRawDamageEvent Negative = PhysicalHit();
			Negative.PhysicalPenetration.Flat = -1.0;
			ASSERT_THAT(IsFalse(VeyraCombat::PrepareDamage(*Attacker, TooMuch).IsValid()));
			ASSERT_THAT(IsFalse(VeyraCombat::PrepareDamage(*Attacker, Negative).IsValid()));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
