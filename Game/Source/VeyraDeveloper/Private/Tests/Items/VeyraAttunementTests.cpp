// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Components/ActorTestSpawner.h"
#include "CQTest.h"
#include "Engine/World.h"
#include "Gold/VeyraGoldComponent.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Shop/VeyraShopSubsystem.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tests/Items/VeyraItemsTestCatalog.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraItemsTests
{
	// Veyra.Items.Attunements.*: the Attunements a hit on an enemy Vanguard sets off, through Combat's
	// dealt-damage event and verbs (Item Bible §8–§9; ADR-022 §3–§5). Each test gives the fixture
	// catalog's Masterwork the Attunement under test.
	TEST_CLASS(Attunements, "Veyra.Items")
	{
		// Fixture values.
		static constexpr double Purse = 10000.0;
		static constexpr double Blow = 50.0;
		static constexpr float WorldStep = 0.1f;

		FActorTestSpawner Spawner;
		FVeyraItemsTuning Tuning = TestCatalog();
		UVeyraShopSubsystem* Shop = nullptr;
		AVeyraPlayerState* Participant = nullptr;
		UAbilitySystemComponent* Enemy = nullptr;
		TArray<FVeyraDamageDealtEvent> Dealt;

		BEFORE_EACH()
		{
			UVeyraItemsTuningSubsystem::SetTestOverride(&Tuning);
			Shop = Spawner.GetWorld().GetSubsystem<UVeyraShopSubsystem>();
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			Participant = World.Spawn(EVeyraTeam::A, FVector::ZeroVector).GetPlayerState<AVeyraPlayerState>();
			Enemy = World.Spawn(EVeyraTeam::B, FVector(200.0, 0.0, 0.0)).GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(Shop && Participant && Enemy));
			UVeyraShopSubsystem::InitializeInventory(*Participant);
			ASSERT_THAT(IsTrue(Participant->FindComponentByClass<UVeyraGoldComponent>()->Grant(Purse, EVeyraGoldReason::Developer)));
			Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnDamageDealt.AddLambda([this](const FVeyraDamageDealtEvent& Event) { Dealt.Add(Event); });
		}

		AFTER_EACH()
		{
			UVeyraItemsTuningSubsystem::SetTestOverride(nullptr);
		}

		UAbilitySystemComponent& Holder() const
		{
			return *Participant->GetAbilitySystemComponent();
		}

		/** Gives the Masterwork Attunement and buys it at the fountain. */
		void Hold(const TCHAR* Attunement)
		{
			Tuning.Items[ItemId(TEXT("test_temper"))].Attunement = { ItemId(Attunement) };
			Tuning.WeightOfWar.Reset();
			Shop->SetAtFountain(*Participant, true);
			ASSERT_THAT(IsTrue(Shop->Buy(*Participant, ItemId(TEXT("test_temper"))) == EVeyraShopRefusal::None));
		}

		void Hit(EVeyraDamageType Type, double Amount, EVeyraDamageDelivery Delivery)
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ Type, Amount });
			Damage.Delivery = Delivery;
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Holder(), *Enemy, Damage)));
		}

		double HolderShield() const
		{
			double Sum = 0.0;
			for (const FVeyraShieldEntry& Shield : Participant->FindComponentByClass<UVeyraDamageAbsorptionComponent>()->GetLedger().Shields)
			{
				Sum += Shield.Remaining;
			}
			return Sum;
		}

		const UVeyraStatusComponent& EnemyStatuses() const
		{
			return *Enemy->GetOwner()->FindComponentByClass<UVeyraStatusComponent>();
		}

		int32 Procs() const
		{
			return Dealt.FilterByPredicate([](const FVeyraDamageDealtEvent& Event) { return Event.Delivery == EVeyraDamageDelivery::Proc; }).Num();
		}

		void RunFor(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, WorldStep);
			}
		}

		TEST_METHOD(ReprisalGuardShieldsAShareOfTheHitThenWaits)
		{
			// Fixture values: a fifth of the hit, at most 15, for a long while; 5 s of cooldown.
			constexpr double Fraction = 0.2;
			constexpr double Cap = 15.0;
			constexpr double Cooldown = 5.0;
			FVeyraReprisalGuardTuning& Guard = Tuning.ReprisalGuard.Add(ItemId(TEXT("test_guard")));
			Guard.DamageFraction = Fraction;
			Guard.MaxShield = Cap;
			Guard.ShieldSeconds = 60.0;
			Guard.CooldownSeconds = Cooldown;
			Hold(TEXT("test_guard"));

			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::Proc);
			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::Periodic);
			ASSERT_THAT(IsTrue(HolderShield() == 0.0, TEXT("a proc or a tick is no triggering attack or ability")));
			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::Ability);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HolderShield(), Blow * Fraction), FString::SanitizeFloat(HolderShield())));
			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::BasicAttack);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HolderShield(), Blow * Fraction), TEXT("then it waits")));

			// Past its cooldown, a heavy hit's shield stops at the cap, in place of the last.
			RunFor(Cooldown + WorldStep);
			Hit(EVeyraDamageType::TrueDamage, Blow * 4.0, EVeyraDamageDelivery::BasicAttack);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HolderShield(), Cap), FString::SanitizeFloat(HolderShield())));
		}

		TEST_METHOD(DragSlowsOnlyOnDamagingAbilities)
		{
			// Fixture values: 30% for a second.
			constexpr double Slow = 0.3;
			FVeyraDragTuning& Drag = Tuning.Drag.Add(ItemId(TEXT("test_drag")));
			Drag.Slow = Slow;
			Drag.DurationSeconds = 1.0;
			Hold(TEXT("test_drag"));

			Hit(EVeyraDamageType::Physical, Blow, EVeyraDamageDelivery::BasicAttack);
			ASSERT_THAT(IsTrue(EnemyStatuses().GetStrongestSlow() == 0.0, TEXT("a basic attack does not")));
			Hit(EVeyraDamageType::Magic, Blow, EVeyraDamageDelivery::Ability);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(EnemyStatuses().GetStrongestSlow(), Slow)));
		}

		TEST_METHOD(ConvergencePrimesAndTheNextAbilityHitConsumesIt)
		{
			// Fixture values: a 4 s window; 40 magic damage and a fifth of Magic Power. No Magic
			// Resistance, so the bonus lands whole.
			constexpr double Window = 4.0;
			constexpr double Base = 40.0;
			constexpr double Ratio = 0.2;
			FVeyraConvergenceTuning& Convergence = Tuning.Convergence.Add(ItemId(TEXT("test_converge")));
			Convergence.WindowSeconds = Window;
			Convergence.BaseDamage = Base;
			Convergence.MagicPowerRatio = Ratio;
			Hold(TEXT("test_converge"));
			Enemy->SetNumericAttributeBase(UVeyraDefenceSet::GetMagicResistAttribute(), 0.0f);

			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::BasicAttack);
			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::BasicAttack);
			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::Ability);
			ASSERT_THAT(AreEqual(0, Procs(), TEXT("basic attacks neither prime nor consume; the ability primes")));
			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::Ability);
			ASSERT_THAT(AreEqual(1, Procs(), TEXT("the next ability consumes the prime")));
			const FVeyraDamageDealtEvent& Bonus = *Dealt.FindByPredicate([](const FVeyraDamageDealtEvent& Event) { return Event.Delivery == EVeyraDamageDelivery::Proc; });
			const double Expected = Base + Ratio * Holder().GetNumericAttribute(UVeyraOffenceSet::GetMagicPowerAttribute());
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Bonus.Of(EVeyraDamageType::Magic), Expected, 1e-3), FString::SanitizeFloat(Bonus.Of(EVeyraDamageType::Magic))));

			// Consuming does not prime again, and a prime lapses after its window.
			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::Ability);
			ASSERT_THAT(AreEqual(1, Procs()));
			RunFor(Window + WorldStep);
			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::Ability);
			ASSERT_THAT(AreEqual(1, Procs(), TEXT("the lapsed prime is gone; this one primes afresh")));
			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::Ability);
			ASSERT_THAT(AreEqual(2, Procs()));
		}

		TEST_METHOD(FractureStacksOnMagicDamageUpToItsCap)
		{
			// Fixture values: 5% a stack, three at most.
			constexpr double PerStack = 0.05;
			constexpr int32 MaxStacks = 3;
			FVeyraStackingAttunementTuning& Fracture = Tuning.Fracture.Add(ItemId(TEXT("test_fracture")));
			Fracture.PerStack = PerStack;
			Fracture.MaxStacks = MaxStacks;
			Fracture.DurationSeconds = 4.0;
			Hold(TEXT("test_fracture"));
			const auto Retained = [this] { return Enemy->GetNumericAttribute(UVeyraDefenceSet::GetMagicResistReductionRetainedAttribute()); };

			Hit(EVeyraDamageType::Physical, Blow, EVeyraDamageDelivery::BasicAttack);
			ASSERT_THAT(IsTrue(Retained() == 1.0, TEXT("physical damage does not")));
			Hit(EVeyraDamageType::Magic, Blow, EVeyraDamageDelivery::Ability);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Retained(), 1.0 - PerStack, 1e-5)));
			for (int32 More = 0; More < MaxStacks; ++More)
			{
				Hit(EVeyraDamageType::Magic, Blow, EVeyraDamageDelivery::BasicAttack);
			}
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Retained(), 1.0 - PerStack * MaxStacks, 1e-5), TEXT("any magic damage adds one, up to the cap")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
