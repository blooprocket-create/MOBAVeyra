// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "CQTest.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	// Veyra.Combat.Shields.*: shields with an identity, a source, merging, a maximum and cap groups
	// (Combat Bible §7, ADR-009 §3).
	TEST_CLASS(Shields, "Veyra.Combat")
	{
		// Fixture values: a long duration, so no shield ends during a test.
		static constexpr double LongSeconds = 60.0;

		FActorTestSpawner Spawner;
		UAbilitySystemComponent* Caster = nullptr;
		UAbilitySystemComponent* Unit = nullptr;
		UVeyraDamageAbsorptionComponent* Absorption = nullptr;

		BEFORE_EACH()
		{
			Caster = &SpawnCombatant(Spawner);
			Unit = &SpawnCombatant(Spawner);
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(*Unit, ExampleStats())));
			Absorption = Unit->GetOwner()->FindComponentByClass<UVeyraDamageAbsorptionComponent>();
			ASSERT_THAT(IsNotNull(Absorption));
		}

		static FVeyraShieldGrant Grant(const TCHAR* Id, double Amount, double MaxAmount, EVeyraShieldReapply Reapply,
			const TCHAR* CapGroup = nullptr, double CapGroupTotal = 0.0)
		{
			FVeyraShieldGrant Result;
			Result.Id = FVeyraContentId::FromText(Id).GetValue();
			Result.Category = EVeyraShieldCategory::Universal;
			Result.Amount = Amount;
			Result.MaxAmount = MaxAmount;
			Result.DurationSeconds = LongSeconds;
			Result.Reapply = Reapply;
			if (CapGroup)
			{
				Result.CapGroup = FVeyraContentId::FromText(CapGroup).GetValue();
				Result.CapGroupTotal = CapGroupTotal;
			}
			return Result;
		}

		const TArray<FVeyraShieldEntry>& ShieldEntries() const
		{
			return Absorption->GetLedger().Shields;
		}

		double Remaining(int32 Index) const
		{
			return ShieldEntries().IsValidIndex(Index) ? ShieldEntries()[Index].Remaining : -1.0;
		}

		/** True Damage, which only Universal shields absorb and no resistance mitigates. */
		bool HitFor(double Amount)
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Amount });
			return VeyraCombat::DealDamage(*Caster, *Unit, Damage);
		}

		TEST_METHOD(ANamedTemporaryHealthGrantTopsUpToItsMostAsOneGrant)
		{
			// Fixture values: 30 at a time, at most 50 (Combat Bible §7).
			const FVeyraContentId Tide = FVeyraContentId::FromText(TEXT("tide")).GetValue();
			const TArray<FVeyraTemporaryHealthGrant>& Grants = Absorption->GetLedger().TemporaryHealth;
			ASSERT_THAT(IsTrue(VeyraCombat::GrantTemporaryHealth(*Caster, *Unit, Tide, 30.0, 50.0, LongSeconds).IsValid()));
			ASSERT_THAT(IsTrue(VeyraCombat::GrantTemporaryHealth(*Caster, *Unit, Tide, 30.0, 50.0, LongSeconds).IsValid()));
			ASSERT_THAT(IsTrue(Grants.Num() == 1 && Grants[0].Remaining == 50.0, TEXT("one grant, topped up to its most")));
			ASSERT_THAT(IsFalse(VeyraCombat::GrantTemporaryHealth(*Caster, *Unit, Tide, 10.0, 50.0, LongSeconds).IsValid(), TEXT("full, it grants nothing")));
			ASSERT_THAT(IsTrue(HitFor(20.0)));
			ASSERT_THAT(IsTrue(Grants.Num() == 1 && Grants[0].Remaining == 30.0));
			ASSERT_THAT(IsTrue(VeyraCombat::GrantTemporaryHealth(*Caster, *Unit, Tide, 10.0, 50.0, LongSeconds).IsValid()));
			ASSERT_THAT(IsTrue(Grants.Num() == 1 && Grants[0].Remaining == 40.0, TEXT("spent, it tops up again")));
			ASSERT_THAT(IsTrue(VeyraCombat::GrantTemporaryHealth(*Unit, *Unit, Tide, 10.0, 50.0, LongSeconds).IsValid() && Grants.Num() == 2,
				TEXT("another source's is its own")));
		}

		TEST_METHOD(AReplacedShieldStartsAgain)
		{
			const FActiveGameplayEffectHandle First = VeyraCombat::GrantShield(*Caster, *Unit, Grant(TEXT("veil"), 100.0, 100.0, EVeyraShieldReapply::Replace));
			ASSERT_THAT(IsTrue(HitFor(30.0)));
			ASSERT_THAT(IsTrue(Remaining(0) == 70.0));
			const FActiveGameplayEffectHandle Second = VeyraCombat::GrantShield(*Caster, *Unit, Grant(TEXT("veil"), 100.0, 100.0, EVeyraShieldReapply::Replace));
			ASSERT_THAT(IsTrue(Second.IsValid()));
			ASSERT_THAT(AreEqual(1, ShieldEntries().Num()));
			ASSERT_THAT(IsTrue(Remaining(0) == 100.0));
			ASSERT_THAT(IsNull(Unit->GetActiveGameplayEffect(First), TEXT("the replaced shield's effect should have ended")));
		}

		TEST_METHOD(MergingAddsUpToTheShieldsMaximum)
		{
			const FVeyraShieldGrant Contribution = Grant(TEXT("foundation"), 60.0, 100.0, EVeyraShieldReapply::Merge);
			ASSERT_THAT(IsTrue(VeyraCombat::GrantShield(*Caster, *Unit, Contribution).IsValid()));
			ASSERT_THAT(IsTrue(VeyraCombat::GrantShield(*Caster, *Unit, Contribution).IsValid()));
			ASSERT_THAT(AreEqual(1, ShieldEntries().Num()));
			ASSERT_THAT(IsTrue(Remaining(0) == Contribution.MaxAmount));
		}

		TEST_METHOD(AMergedShieldKeepsItsAge)
		{
			// Oldest first: an unnamed shield, the named one, then another unnamed one.
			ASSERT_THAT(IsTrue(VeyraCombat::GrantShield(*Caster, *Unit, EVeyraShieldCategory::Universal, 50.0, LongSeconds).IsValid()));
			const FVeyraShieldGrant Contribution = Grant(TEXT("foundation"), 40.0, 100.0, EVeyraShieldReapply::Merge);
			ASSERT_THAT(IsTrue(VeyraCombat::GrantShield(*Caster, *Unit, Contribution).IsValid()));
			ASSERT_THAT(IsTrue(VeyraCombat::GrantShield(*Caster, *Unit, EVeyraShieldCategory::Universal, 30.0, LongSeconds).IsValid()));
			FVeyraShieldGrant More = Contribution;
			More.Amount = 20.0;
			ASSERT_THAT(IsTrue(VeyraCombat::GrantShield(*Caster, *Unit, More).IsValid()));

			// 50 empties the oldest, and the named shield, still second oldest, takes the other 20.
			ASSERT_THAT(IsTrue(HitFor(70.0)));
			ASSERT_THAT(AreEqual(2, ShieldEntries().Num()));
			ASSERT_THAT(IsTrue(Remaining(0) == 40.0 && Remaining(1) == 30.0, TEXT("merging made the shield the youngest")));
		}

		TEST_METHOD(ACapGroupBoundsTheSourcesShieldsTogether)
		{
			constexpr double GroupTotal = 150.0;
			constexpr double ShieldMax = 200.0;
			const FVeyraShieldGrant Passive = Grant(TEXT("passive_shield"), 100.0, ShieldMax, EVeyraShieldReapply::Merge, TEXT("cairn_shields"), GroupTotal);
			const FVeyraShieldGrant Ultimate = Grant(TEXT("ultimate_shield"), 100.0, ShieldMax, EVeyraShieldReapply::Merge, TEXT("cairn_shields"), GroupTotal);
			ASSERT_THAT(IsTrue(VeyraCombat::GrantShield(*Caster, *Unit, Passive).IsValid()));
			ASSERT_THAT(IsTrue(VeyraCombat::GrantShield(*Caster, *Unit, Ultimate).IsValid()));
			ASSERT_THAT(IsTrue(Remaining(0) == 100.0 && Remaining(1) == GroupTotal - 100.0, TEXT("the group should hold its total")));

			// A full group grants nothing new, and merging never lowers what a shield has.
			ASSERT_THAT(IsTrue(VeyraCombat::GrantShield(*Caster, *Unit, Ultimate).IsValid(), TEXT("a merge at the cap still refreshes the shield")));
			ASSERT_THAT(IsTrue(Remaining(1) == GroupTotal - 100.0));
			const FVeyraShieldGrant Third = Grant(TEXT("third_shield"), 10.0, 10.0, EVeyraShieldReapply::Replace, TEXT("cairn_shields"), GroupTotal);
			ASSERT_THAT(IsFalse(VeyraCombat::GrantShield(*Caster, *Unit, Third).IsValid()));
			ASSERT_THAT(AreEqual(2, ShieldEntries().Num()));

			// Damage to the group makes room again.
			ASSERT_THAT(IsTrue(HitFor(60.0)));
			ASSERT_THAT(IsTrue(VeyraCombat::GrantShield(*Caster, *Unit, Ultimate).IsValid()));
			ASSERT_THAT(IsTrue(Remaining(1) == GroupTotal - Remaining(0)));
		}

		TEST_METHOD(AnotherSourcesShieldsAreSeparate)
		{
			UAbilitySystemComponent& OtherCaster = SpawnCombatant(Spawner);
			const FVeyraShieldGrant Veil = Grant(TEXT("veil"), 100.0, 100.0, EVeyraShieldReapply::Replace, TEXT("veils"), 100.0);
			ASSERT_THAT(IsTrue(VeyraCombat::GrantShield(*Caster, *Unit, Veil).IsValid()));
			ASSERT_THAT(IsTrue(VeyraCombat::GrantShield(OtherCaster, *Unit, Veil).IsValid()));
			ASSERT_THAT(AreEqual(2, ShieldEntries().Num()));
			ASSERT_THAT(IsTrue(Remaining(0) == 100.0 && Remaining(1) == 100.0));
		}

		TEST_METHOD(RefusesGrantsOutsideTheRules)
		{
			FVeyraShieldGrant NoAmount = Grant(TEXT("veil"), 0.0, 100.0, EVeyraShieldReapply::Replace);
			FVeyraShieldGrant SmallMaximum = Grant(TEXT("veil"), 100.0, 50.0, EVeyraShieldReapply::Merge);
			FVeyraShieldGrant GroupWithoutTotal = Grant(TEXT("veil"), 100.0, 100.0, EVeyraShieldReapply::Replace, TEXT("veils"), 0.0);
			FVeyraShieldGrant TotalWithoutGroup = Grant(TEXT("veil"), 100.0, 100.0, EVeyraShieldReapply::Replace);
			TotalWithoutGroup.CapGroupTotal = 100.0;
			const FVeyraShieldGrant Refused[] = { NoAmount, SmallMaximum, GroupWithoutTotal, TotalWithoutGroup };
			TestRunner->AddExpectedMessagePlain(TEXT("Refused shield"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains,
				static_cast<int32>(UE_ARRAY_COUNT(Refused)));
			for (const FVeyraShieldGrant& Refusal : Refused)
			{
				ASSERT_THAT(IsFalse(VeyraCombat::GrantShield(*Caster, *Unit, Refusal).IsValid()));
			}
			ASSERT_THAT(IsTrue(ShieldEntries().IsEmpty()));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
