// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "AbilitySystemComponent.h"
#include "CQTest.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatEventTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	// Veyra.Combat.CombatEvents.*: what Combat reports for statistics (ADR-017 §1) and Attunements
	// (ADR-023 §4). A resolved hit says what it cost and whose shields took it; a dealt one what each
	// type cost an enemy; a heal what it restored and who gave it; a status when it began and ends.
	TEST_CLASS(CombatEvents, "Veyra.Combat")
	{
		// Fixture values, not tuning.
		static constexpr double Hit = 100.0;
		static constexpr double ProvidedShield = 30.0;
		static constexpr double OwnShield = 20.0;
		static constexpr double LongSeconds = 60.0;
		static constexpr double StunSeconds = 1.5;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Attacker = nullptr;
		AVeyraVanguardCharacter* Target = nullptr;
		AVeyraVanguardCharacter* Ally = nullptr;
		TArray<FVeyraDamageResolution> Resolved;
		TArray<FVeyraHealthRestored> Restored;
		TArray<FVeyraStatusApplied> Applied;
		TArray<FVeyraDamageDealtEvent> Dealt;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Attacker = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Target = &World.Spawn(EVeyraTeam::B, FVector(300.0, 0.0, 0.0));
			Ally = &World.Spawn(EVeyraTeam::B, FVector(300.0, 300.0, 0.0));
			UVeyraCombatEventSubsystem* Events = Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>();
			ASSERT_THAT(IsNotNull(Events));
			Events->OnDamageResolved.AddLambda([this](const FVeyraDamageResolution& Event) { Resolved.Add(Event); });
			Events->OnHealthRestored.AddLambda([this](const FVeyraHealthRestored& Event) { Restored.Add(Event); });
			Events->OnStatusApplied.AddLambda([this](const FVeyraStatusApplied& Event) { Applied.Add(Event); });
			Events->OnDamageDealt.AddLambda([this](const FVeyraDamageDealtEvent& Event) { Dealt.Add(Event); });
		}

		static UAbilitySystemComponent& Abilities(AVeyraVanguardCharacter& Vanguard)
		{
			return *Vanguard.GetAbilitySystemComponent();
		}

		static FVeyraRawDamageEvent TrueDamage(double Amount)
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Amount });
			return Damage;
		}

		TEST_METHOD(AResolvedHitSaysWhatItCostAndWhoseShieldsTookIt)
		{
			// The ally's shield is older, so it absorbs first (Combat Bible §7).
			ASSERT_THAT(IsTrue(VeyraCombat::GrantShield(Abilities(*Ally), Abilities(*Target), EVeyraShieldCategory::Universal, ProvidedShield, LongSeconds).IsValid()));
			ASSERT_THAT(IsTrue(VeyraCombat::GrantShield(Abilities(*Target), Abilities(*Target), EVeyraShieldCategory::Universal, OwnShield, LongSeconds).IsValid()));
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Abilities(*Attacker), Abilities(*Target), TrueDamage(Hit))));

			ASSERT_THAT(AreEqual(Resolved.Num(), 1));
			const FVeyraDamageResolution& Event = Resolved[0];
			ASSERT_THAT(IsTrue(Event.Source.Get() == &Abilities(*Attacker) && Event.Target.Get() == &Abilities(*Target) && Event.Type == EVeyraDamageType::TrueDamage));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Event.HealthLost, Hit - ProvidedShield - OwnShield), TEXT("health lost is what the shields did not take")));
			ASSERT_THAT(AreEqual(Event.Shields.Num(), 2));
			ASSERT_THAT(IsTrue(Event.Shields[0].Provider.Get() == &Abilities(*Ally) && FMath::IsNearlyEqual(Event.Shields[0].Absorbed, ProvidedShield)));
			ASSERT_THAT(IsTrue(Event.Shields[1].Provider.Get() == &Abilities(*Target) && FMath::IsNearlyEqual(Event.Shields[1].Absorbed, OwnShield)));
		}

		TEST_METHOD(ADealtHitSaysWhatEachTypeCostShieldsIncluded)
		{
			ASSERT_THAT(IsTrue(VeyraCombat::GrantShield(Abilities(*Target), Abilities(*Target), EVeyraShieldCategory::Universal, OwnShield, LongSeconds).IsValid()));
			FVeyraRawDamageEvent Damage = TrueDamage(Hit);
			Damage.Components.Add({ EVeyraDamageType::Magic, Hit });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Abilities(*Attacker), Abilities(*Target), Damage)));

			ASSERT_THAT(AreEqual(1, Dealt.Num(), TEXT("one event for the instance")));
			const FVeyraDamageDealtEvent& Event = Dealt[0];
			ASSERT_THAT(IsTrue(Event.Source.Get() == &Abilities(*Attacker) && Event.Target.Get() == &Abilities(*Target) && Event.Delivery == EVeyraDamageDelivery::Ability));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Event.Of(EVeyraDamageType::TrueDamage), Hit), TEXT("true damage costs all of itself, the shield's share included")));
			double Cost = 0.0;
			double MagicCost = 0.0;
			for (const FVeyraDamageResolution& Resolution : Resolved)
			{
				double Each = Resolution.HealthLost + Resolution.TemporaryHealthSpent;
				for (const FVeyraShieldShare& Share : Resolution.Shields)
				{
					Each += Share.Absorbed;
				}
				Cost += Each;
				MagicCost += Resolution.Type == EVeyraDamageType::Magic ? Each : 0.0;
			}
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Event.Of(EVeyraDamageType::Magic), MagicCost) && MagicCost > 0.0, TEXT("magic after Magic Resistance")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Event.Total(), Cost), TEXT("the whole instance, and nothing else")));

			// Damage between allies is dealt to no enemy.
			VeyraCombat::DealDamage(Abilities(*Ally), Abilities(*Target), TrueDamage(Hit));
			ASSERT_THAT(AreEqual(1, Dealt.Num()));
		}

		TEST_METHOD(AHealSaysWhatItRestoredAndWhoGaveIt)
		{
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Abilities(*Attacker), Abilities(*Target), TrueDamage(Hit))));
			// Healing more than is missing restores only what is missing (Combat Bible §6).
			const double Given = VeyraCombat::RestoreHealthFrom(Abilities(*Ally), Abilities(*Target), Hit * 2.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Given, Hit)));
			ASSERT_THAT(IsTrue(Restored.Num() == 1 && Restored[0].Provider.Get() == &Abilities(*Ally) && FMath::IsNearlyEqual(Restored[0].Restored, Hit)));

			// At full Health, nothing is restored and nothing is reported.
			ASSERT_THAT(IsTrue(VeyraCombat::RestoreHealthFrom(Abilities(*Ally), Abilities(*Target), Hit) == 0.0 && Restored.Num() == 1));
			// Regeneration and the fountain heal as no one.
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Abilities(*Attacker), Abilities(*Target), TrueDamage(Hit))));
			ASSERT_THAT(IsTrue(VeyraCombat::RestoreHealth(Abilities(*Target), Hit)));
			ASSERT_THAT(IsTrue(Restored.Num() == 2 && !Restored[1].Provider.IsValid()));
		}

		TEST_METHOD(AStatusSaysWhenItBeganAndEnds)
		{
			FVeyraStatusSpec Stun;
			Stun.Id = FVeyraContentId::FromText(TEXT("test_stun")).GetValue();
			Stun.Kind = EVeyraStatusKind::Stun;
			Stun.DurationSeconds = StunSeconds;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Abilities(*Attacker), Abilities(*Target), Stun)));
			ASSERT_THAT(AreEqual(Applied.Num(), 1));
			ASSERT_THAT(IsTrue(Applied[0].Source.Get() == &Abilities(*Attacker) && Applied[0].Target.Get() == &Abilities(*Target) && Applied[0].Kind == EVeyraStatusKind::Stun));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Applied[0].EndsAt - Applied[0].StartsAt, StunSeconds), TEXT("no Tenacity: its whole duration")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
