// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraMobilitySet.h"
#include "Attributes/VeyraResourceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "CQTest.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Life/VeyraLifeComponent.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	// Veyra.Combat.FinalDeath.*: Health reaching 0 is a final death; temporary effects end, permanent
	// ones and the unit's stats stay, and Match hears of it (Combat Bible §18, §44).
	TEST_CLASS(FinalDeath, "Veyra.Combat")
	{
		static constexpr double StartingMaxHealth = 100.0;
		static constexpr double StartingMaxResource = 50.0;
		static constexpr double LethalDamage = 1000.0;
		static constexpr double GrantAmount = 20.0;
		static constexpr double GrantSeconds = 10.0;
		static constexpr float BonusSpeed = 25.0f;

		FActorTestSpawner Spawner;
		UAbilitySystemComponent* Killer = nullptr;
		UAbilitySystemComponent* Victim = nullptr;
		TArray<FVeyraDeathEvent> Deaths;
		FDelegateHandle DeathHandle;

		BEFORE_EACH()
		{
			Killer = &SpawnCombatant(Spawner);
			Victim = &SpawnCombatant(Spawner);
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeVitals(*Victim, StartingMaxHealth)));
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeResource(*Victim, StartingMaxResource)));
			DeathHandle = Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnDeath.AddLambda(
				[this](const FVeyraDeathEvent& Event) { Deaths.Add(Event); });
		}

		AFTER_EACH()
		{
			Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnDeath.Remove(DeathHandle);
		}

		bool IsAlive(const UAbilitySystemComponent& Unit) const
		{
			return Unit.GetOwner()->FindComponentByClass<UVeyraLifeComponent>()->IsAlive();
		}

		bool Kill()
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, LethalDamage });
			return VeyraCombat::DealDamage(*Killer, *Victim, Damage);
		}

		TEST_METHOD(LethalDamageIsAFinalDeath)
		{
			ASSERT_THAT(IsTrue(Kill()));
			ASSERT_THAT(IsTrue(Victim->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute()) == 0.0));
			ASSERT_THAT(IsFalse(IsAlive(*Victim)));
			ASSERT_THAT(AreEqual(Deaths.Num(), 1));
			ASSERT_THAT(IsTrue(Deaths[0].Victim.Get() == Victim && Deaths[0].Killer.Get() == Killer));
		}

		TEST_METHOD(TheDeadTakeNoFurtherDamage)
		{
			ASSERT_THAT(IsTrue(Kill()));
			ASSERT_THAT(IsFalse(Kill()));
			ASSERT_THAT(AreEqual(Deaths.Num(), 1));
		}

		TEST_METHOD(TemporaryEffectsEndButPermanentOnesStay)
		{
			const FActiveGameplayEffectHandle Shield = VeyraCombat::GrantShield(*Victim, *Victim, EVeyraShieldCategory::Magic, GrantAmount, GrantSeconds);
			const FActiveGameplayEffectHandle TemporaryHealth = VeyraCombat::GrantTemporaryHealth(*Victim, *Victim, GrantAmount, GrantSeconds);
			UGameplayEffect& Buff = NewTestEffect(EGameplayEffectDurationType::HasDuration,
				{ { UVeyraMobilitySet::GetMoveSpeedAttribute(), EGameplayModOp::AddBase, BonusSpeed } });
			Buff.DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(static_cast<float>(GrantSeconds)));
			const FActiveGameplayEffectHandle TemporaryBuff = ApplyToSelf(*Victim, Buff);
			const FActiveGameplayEffectHandle PermanentBuff = ApplyToSelf(*Victim, NewTestEffect(EGameplayEffectDurationType::Infinite,
				{ { UVeyraMobilitySet::GetMoveSpeedAttribute(), EGameplayModOp::AddBase, BonusSpeed } }));
			for (const FActiveGameplayEffectHandle& Handle : { Shield, TemporaryHealth, TemporaryBuff, PermanentBuff })
			{
				ASSERT_THAT(IsTrue(Handle.WasSuccessfullyApplied()));
			}

			ASSERT_THAT(IsTrue(Kill()));
			ASSERT_THAT(IsNull(Victim->GetActiveGameplayEffect(Shield)));
			ASSERT_THAT(IsNull(Victim->GetActiveGameplayEffect(TemporaryHealth)));
			ASSERT_THAT(IsNull(Victim->GetActiveGameplayEffect(TemporaryBuff)));
			ASSERT_THAT(IsNotNull(Victim->GetActiveGameplayEffect(PermanentBuff)));
		}

		TEST_METHOD(ReviveRestoresTheUnit)
		{
			ASSERT_THAT(IsTrue(VeyraCombat::SpendResource(*Victim, StartingMaxResource)));
			ASSERT_THAT(IsFalse(VeyraCombat::Revive(*Victim)));
			ASSERT_THAT(IsTrue(Kill()));
			ASSERT_THAT(IsTrue(VeyraCombat::Revive(*Victim)));
			ASSERT_THAT(IsTrue(IsAlive(*Victim)));
			ASSERT_THAT(IsTrue(Victim->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute()) == StartingMaxHealth));
			ASSERT_THAT(IsTrue(Victim->GetNumericAttribute(UVeyraResourceSet::GetResourceAttribute()) == StartingMaxResource));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
