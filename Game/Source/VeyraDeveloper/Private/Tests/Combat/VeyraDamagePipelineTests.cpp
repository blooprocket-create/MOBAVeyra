// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "CQTest.h"
#include "Effects/VeyraCombatEffects.h"
#include "Tags/VeyraShieldTags.h"
#include "Tags/VeyraStatusTags.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	// Veyra.Combat.DamagePipeline.*: one damage event from verb to Health between two PlayerStates:
	// SetByCaller amounts, captured offence and defence, the resolver, the meta attributes, shields,
	// Temporary Health and Health (Combat Bible §25).
	TEST_CLASS(DamagePipeline, "Veyra.Combat")
	{
		static constexpr double Tolerance = 1e-2;
		static constexpr double StartingMaxHealth = 1000.0;
		static constexpr double GrantSeconds = 10.0;

		FActorTestSpawner Spawner;
		UAbilitySystemComponent* Attacker = nullptr;
		UAbilitySystemComponent* Defender = nullptr;
		double MitigationConstant = 0.0;

		BEFORE_EACH()
		{
			Attacker = &SpawnCombatant(Spawner);
			Defender = &SpawnCombatant(Spawner);
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeVitals(*Defender, StartingMaxHealth)));
			MitigationConstant = UVeyraCombatTuningSubsystem::Get().Resistance.MitigationConstant;
		}

		double Health() const
		{
			return Defender->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
		}

		double Mitigated(double Amount, double Resistance) const
		{
			return Amount * MitigationConstant / (MitigationConstant + Resistance);
		}

		const FVeyraAbsorptionLedger& Ledger() const
		{
			return Defender->GetOwner()->FindComponentByClass<UVeyraDamageAbsorptionComponent>()->GetLedger();
		}

		static FVeyraRawDamageEvent Damage(std::initializer_list<FVeyraDamageComponent> Components)
		{
			FVeyraRawDamageEvent Event;
			Event.Components = Components;
			return Event;
		}

		TEST_METHOD(MitigatesEachTypeAgainstItsOwnResistance)
		{
			Defender->SetNumericAttributeBase(UVeyraDefenceSet::GetArmorAttribute(), 100.0f);
			Defender->SetNumericAttributeBase(UVeyraDefenceSet::GetMagicResistAttribute(), 50.0f);

			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Attacker, *Defender,
				Damage({ { EVeyraDamageType::Physical, 200.0 }, { EVeyraDamageType::Magic, 150.0 }, { EVeyraDamageType::TrueDamage, 50.0 } }))));
			const double Expected = StartingMaxHealth - Mitigated(200.0, 100.0) - Mitigated(150.0, 50.0) - 50.0;
			ASSERT_THAT(IsNear(Expected, Health(), Tolerance));
		}

		TEST_METHOD(AppliesTheAttackersPenetrationAndAmplification)
		{
			Defender->SetNumericAttributeBase(UVeyraDefenceSet::GetArmorAttribute(), 100.0f);
			Attacker->SetNumericAttributeBase(UVeyraOffenceSet::GetPhysicalPenetrationFlatAttribute(), 50.0f);
			Attacker->SetNumericAttributeBase(UVeyraOffenceSet::GetOutgoingDamageMultiplierAttribute(), 1.5f);

			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Attacker, *Defender, Damage({ { EVeyraDamageType::Physical, 200.0 } }))));
			ASSERT_THAT(IsNear(StartingMaxHealth - Mitigated(200.0 * 1.5, 50.0), Health(), Tolerance));
		}

		TEST_METHOD(ShieldsAbsorbFirstAndEndWhenEmptied)
		{
			const FActiveGameplayEffectHandle Shield = VeyraCombat::GrantShield(*Attacker, *Defender, EVeyraShieldCategory::Physical, 100.0, GrantSeconds);
			ASSERT_THAT(IsTrue(Shield.IsValid()));
			ASSERT_THAT(AreEqual(1, Ledger().Shields.Num()));

			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Attacker, *Defender, Damage({ { EVeyraDamageType::Physical, 250.0 } }))));
			ASSERT_THAT(IsNear(StartingMaxHealth - 150.0, Health(), Tolerance));
			ASSERT_THAT(IsTrue(Ledger().Shields.IsEmpty(), TEXT("The emptied shield is still in the ledger")));
			ASSERT_THAT(IsNull(Defender->GetActiveGameplayEffect(Shield), TEXT("The emptied shield's effect is still active")));
		}

		TEST_METHOD(AShieldOfTheWrongCategoryAbsorbsNothing)
		{
			VeyraCombat::GrantShield(*Attacker, *Defender, EVeyraShieldCategory::Magic, 100.0, GrantSeconds);
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Attacker, *Defender, Damage({ { EVeyraDamageType::Physical, 40.0 } }))));
			ASSERT_THAT(IsNear(StartingMaxHealth - 40.0, Health(), Tolerance));
			ASSERT_THAT(IsNear(100.0, Ledger().Shields[0].Remaining, Tolerance));
		}

		TEST_METHOD(TemporaryHealthIsSpentBeforeHealth)
		{
			const FActiveGameplayEffectHandle Grant = VeyraCombat::GrantTemporaryHealth(*Attacker, *Defender, 50.0, GrantSeconds);
			ASSERT_THAT(IsTrue(Grant.IsValid()));

			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Attacker, *Defender, Damage({ { EVeyraDamageType::TrueDamage, 80.0 } }))));
			ASSERT_THAT(IsNear(StartingMaxHealth - 30.0, Health(), Tolerance));
			ASSERT_THAT(IsNull(Defender->GetActiveGameplayEffect(Grant), TEXT("The spent grant's effect is still active")));
		}

		TEST_METHOD(InvulnerabilityStopsTheDamage)
		{
			VeyraCombat::GrantShield(*Attacker, *Defender, EVeyraShieldCategory::Universal, 100.0, GrantSeconds);
			Defender->AddLooseGameplayTag(VeyraTags::Status_Invulnerable);

			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Attacker, *Defender, Damage({ { EVeyraDamageType::TrueDamage, 300.0 } }))));
			ASSERT_THAT(IsNear(StartingMaxHealth, Health(), Tolerance));
			ASSERT_THAT(IsNear(100.0, Ledger().Shields[0].Remaining, Tolerance));
		}

		TEST_METHOD(RefusesDamageDataThatIsNotADamageType)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("is not a damage type"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
			const FGameplayEffectSpecHandle Spec = Attacker->MakeOutgoingSpec(UVeyraDamageEffect::StaticClass(), TestEffectLevel, Attacker->MakeEffectContext());
			Spec.Data->SetSetByCallerMagnitude(VeyraTags::Shield_Type_Physical, 100.0f);
			Attacker->ApplyGameplayEffectSpecToTarget(*Spec.Data, Defender);
			ASSERT_THAT(IsNear(StartingMaxHealth, Health(), Tolerance));
		}

		TEST_METHOD(RefusesEmptyOrRepeatedDamageTypes)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("Refused damage"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 2);
			ASSERT_THAT(IsFalse(VeyraCombat::DealDamage(*Attacker, *Defender, FVeyraRawDamageEvent())));
			ASSERT_THAT(IsFalse(VeyraCombat::DealDamage(*Attacker, *Defender,
				Damage({ { EVeyraDamageType::Magic, 10.0 }, { EVeyraDamageType::Magic, 10.0 } }))));
			ASSERT_THAT(IsNear(StartingMaxHealth, Health(), Tolerance));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
