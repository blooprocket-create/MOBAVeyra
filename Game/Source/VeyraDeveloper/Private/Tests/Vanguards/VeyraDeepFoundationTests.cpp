// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "CQTest.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Events/VeyraAbilityEvents.h"
#include "Passives/VeyraPassive.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraVanguards.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	// Veyra.Vanguards.DeepFoundation.*: Cairn's passive, fed ability hits as his abilities announce them
	// (Character Bible §18; ADR-008 §5). Expectations come from the committed tuning.
	// Veyra.Net.Vanguards.Cairn casts the whole kit in a match.
	TEST_CLASS(DeepFoundation, "Veyra.Vanguards")
	{
		static constexpr double Tolerance = 1e-3;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Body = nullptr;

		BEFORE_EACH()
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			Body = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraPlayerState* Participant = Body->GetPlayerState<AVeyraPlayerState>();
			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(*Participant->GetAbilitySystemComponent(), FVeyraContentId::FromText(TEXT("cairn")).GetValue());
			ASSERT_THAT(IsTrue(Prepared.bPrepared && Prepared.Passive));
			Participant->SetPassive(Prepared.Passive);
		}

		UAbilitySystemComponent& Self() const
		{
			return *Body->GetAbilitySystemComponent();
		}

		static const FVeyraDeepFoundationTuning& Foundation()
		{
			return *UVeyraVanguardsTuningSubsystem::FindDeepFoundation(FVeyraContentId::FromText(TEXT("cairn_deep_foundation")).GetValue());
		}

		/** The shield one immobilized Vanguard grants now. */
		double OneGrant() const
		{
			return VeyraEffectDelivery::ShieldGrant(Self(), Foundation().Shield, 1).Amount;
		}

		double ShieldTotal() const
		{
			double Total = 0.0;
			for (const FVeyraShieldEntry& Shield : Body->GetPlayerState()->FindComponentByClass<UVeyraDamageAbsorptionComponent>()->GetLedger().Shields)
			{
				Total += Shield.Remaining;
			}
			return Total;
		}

		/** Announces a hit of one of Cairn's abilities on Target, as its delivery would. */
		void Hit(AActor& Target, bool bStunned, bool bDisplaced, bool bCasterShielded = false, UAbilitySystemComponent* Caster = nullptr)
		{
			FVeyraAbilityHit AbilityHit;
			AbilityHit.Caster = Caster ? Caster : &Self();
			AbilityHit.Target = &Target;
			AbilityHit.Ability = FVeyraContentId::FromText(TEXT("cairn_crushing_hold")).GetValue();
			AbilityHit.CastId = 1;
			AbilityHit.bStunned = bStunned;
			AbilityHit.bDisplaced = bDisplaced;
			AbilityHit.bCasterShielded = bCasterShielded;
			UVeyraAbilityEventSubsystem::Announce(&Spawner.GetWorld(), AbilityHit);
		}

		/** Lets at least Seconds of world time pass. */
		void Wait(double Seconds)
		{
			// Fixture value: a step below the longest frame the world accepts in one tick.
			constexpr float StepSeconds = 0.1f;
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, StepSeconds);
			}
		}

		TEST_METHOD(ImmobilizingAnEnemyVanguardGrantsTheShield)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Stunned = World.Spawn(EVeyraTeam::B, FVector(200.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Pulled = World.Spawn(EVeyraTeam::B, FVector(0.0, 200.0, 0.0));
			Hit(Stunned, /*bStunned*/ true, /*bDisplaced*/ false);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ShieldTotal(), OneGrant(), Tolerance), FString::Printf(TEXT("shield %g"), ShieldTotal())));
			Hit(Pulled, /*bStunned*/ false, /*bDisplaced*/ true);
			const double Merged = FMath::Min(2.0 * OneGrant(), VeyraEffectDelivery::ShieldGrant(Self(), Foundation().Shield, 1).MaxAmount);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ShieldTotal(), Merged, Tolerance), TEXT("a pull counts as an immobilization too")));
		}

		TEST_METHOD(SlowsPaidHitsAndOtherUnitsGrantNothing)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(200.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Ally = World.Spawn(EVeyraTeam::A, FVector(0.0, 200.0, 0.0));
			AVeyraTestFluxborn& Minion = World.SpawnFluxborn(EVeyraTeam::B, FVector(-200.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Stranger = World.Spawn(EVeyraTeam::A, FVector(0.0, -200.0, 0.0));
			Hit(Enemy, /*bStunned*/ false, /*bDisplaced*/ false);
			Hit(Enemy, /*bStunned*/ true, /*bDisplaced*/ false, /*bCasterShielded*/ true);
			Hit(Ally, /*bStunned*/ true, /*bDisplaced*/ false);
			Hit(Minion, /*bStunned*/ true, /*bDisplaced*/ false);
			Hit(Enemy, /*bStunned*/ true, /*bDisplaced*/ false, /*bCasterShielded*/ false, Stranger.GetAbilitySystemComponent());
			ASSERT_THAT(IsTrue(ShieldTotal() == 0.0, FString::Printf(TEXT("shield %g"), ShieldTotal())));
		}

		TEST_METHOD(TheSameVanguardGrantsAgainOnlyAfterItsLockout)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(200.0, 0.0, 0.0));
			Hit(Enemy, /*bStunned*/ true, /*bDisplaced*/ true);
			Hit(Enemy, /*bStunned*/ true, /*bDisplaced*/ false);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ShieldTotal(), OneGrant(), Tolerance), TEXT("controls landing together pay once")));
			Wait(Foundation().LockoutSeconds);
			Hit(Enemy, /*bStunned*/ true, /*bDisplaced*/ false);
			ASSERT_THAT(IsTrue(ShieldTotal() > OneGrant() + Tolerance, TEXT("after the lockout it grants again")));
		}

		TEST_METHOD(ShieldsFromSeveralEnemiesMergeUpToTheShieldsMaximum)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			const FVeyraShieldGrant Grant = VeyraEffectDelivery::ShieldGrant(Self(), Foundation().Shield, 1);
			// Enough enemies to pass the maximum.
			const int32 Enemies = FMath::CeilToInt(Grant.MaxAmount / Grant.Amount) + 1;
			for (int32 Index = 0; Index < Enemies; ++Index)
			{
				Hit(World.Spawn(EVeyraTeam::B, FVector(200.0 + 100.0 * Index, 0.0, 0.0)), /*bStunned*/ true, /*bDisplaced*/ false);
			}
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ShieldTotal(), Grant.MaxAmount, Tolerance), FString::Printf(TEXT("shield %g of %g"), ShieldTotal(), Grant.MaxAmount)));
			ASSERT_THAT(AreEqual(1, Body->GetPlayerState()->FindComponentByClass<UVeyraDamageAbsorptionComponent>()->GetLedger().Shields.Num()));
		}

		TEST_METHOD(TheUltimatesShieldSharesTheCap)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			const FVeyraAreaAbilityTuning& Burden = *UVeyraAbilitiesTuningSubsystem::FindArea(FVeyraContentId::FromText(TEXT("cairn_burden_of_the_depths")).GetValue());
			const FVeyraShieldGrant Ultimate = VeyraEffectDelivery::ShieldGrant(Self(), Burden.Zones[0].CasterShieldPerVanguard[0], 1);
			const FVeyraShieldGrant Passive = VeyraEffectDelivery::ShieldGrant(Self(), Foundation().Shield, 1);
			ASSERT_THAT(IsTrue(Ultimate.CapGroup == Passive.CapGroup && Ultimate.CapGroup.IsValid(), TEXT("both shields are in one cap group")));
			// Fill both shields to their maximums; together they stop at the group's total.
			for (int32 Index = 0; Index < 10; ++Index)
			{
				VeyraCombat::GrantShield(Self(), Self(), Ultimate);
				Hit(World.Spawn(EVeyraTeam::B, FVector(200.0 + 100.0 * Index, 0.0, 0.0)), /*bStunned*/ true, /*bDisplaced*/ false);
			}
			const double Expected = FMath::Min(Ultimate.CapGroupTotal, Ultimate.MaxAmount + Passive.MaxAmount);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ShieldTotal(), Expected, Tolerance), FString::Printf(TEXT("shields %g, expected %g"), ShieldTotal(), Expected)));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
