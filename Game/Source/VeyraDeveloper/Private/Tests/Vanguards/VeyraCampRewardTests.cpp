// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Passives/VeyraCampRewardPassive.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraVanguards.h"
#include "Wildlife/VeyraJungleSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	// Veyra.Vanguards.NoTimeToBleed.*: Gorraveth's passive (Roster Bible §23), from the committed tuning.
	TEST_CLASS(NoTimeToBleed, "Veyra.Vanguards")
	{
		// Fixture values: a wound to restore from, and a small step of world time.
		static constexpr double Wound = 300.0;
		static constexpr float WorldStep = 0.1f;
		static constexpr double Tolerance = 1e-3;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Gorraveth = nullptr;
		AVeyraVanguardCharacter* Enemy = nullptr;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Gorraveth = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraPlayerState* Participant = Gorraveth->GetPlayerState<AVeyraPlayerState>();
			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(*Participant->GetAbilitySystemComponent(), FVeyraContentId::FromText(TEXT("gorraveth")).GetValue());
			ASSERT_THAT(IsTrue(Prepared.bPrepared && Cast<UVeyraCampRewardPassive>(Prepared.Passive) != nullptr));
			Participant->SetPassive(Prepared.Passive);
			Enemy = &World.Spawn(EVeyraTeam::B, FVector(1000.0, 0.0, 0.0));
			FVeyraRawDamageEvent Hurt;
			Hurt.Components.Add({ EVeyraDamageType::TrueDamage, Wound });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Enemy->GetAbilitySystemComponent(), *Gorraveth->GetAbilitySystemComponent(), Hurt)));
		}

		static const FVeyraCampRewardTuning& Tuning()
		{
			return *UVeyraVanguardsTuningSubsystem::FindCampReward(FVeyraContentId::FromText(TEXT("gorraveth_no_time_to_bleed")).GetValue());
		}

		double Restore() const
		{
			const double MaxHealth = Gorraveth->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
			return MaxHealth * Tuning().HealthRatio + Tuning().HealthAmount;
		}

		void ClearCamp(TArray<TWeakObjectPtr<UAbilitySystemComponent>> Contributors)
		{
			FVeyraCampCleared Cleared;
			Cleared.Index = 0;
			Cleared.Contributors = MoveTemp(Contributors);
			Spawner.GetWorld().GetSubsystem<UVeyraJungleSubsystem>()->OnCampCleared.Broadcast(Cleared);
		}

		void TakeDown()
		{
			FVeyraDeathEvent Death;
			Death.Victim = Enemy->GetAbilitySystemComponent();
			Death.CreditedKiller = Gorraveth->GetAbilitySystemComponent();
			Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnDeath.Broadcast(Death);
		}

		double Lost() const
		{
			return FArchetypeTestWorld::HealthLost(*Gorraveth);
		}

		TEST_METHOD(HelpingClearACampRestoresHealthAndSpeedsHimUp)
		{
			ClearCamp({ Enemy->GetAbilitySystemComponent() });
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Lost(), Wound, Tolerance), TEXT("a camp he did not help with gives nothing")));
			ClearCamp({ Gorraveth->GetAbilitySystemComponent() });
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Lost(), Wound - Restore(), Tolerance), FString::Printf(TEXT("lost %g"), Lost())));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(*Gorraveth, *Tuning().Statuses[0].ToString())));
		}

		TEST_METHOD(ATakedownRewardsHimOncePerItsCooldown)
		{
			TakeDown();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Lost(), Wound - Restore(), Tolerance)));
			TakeDown();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Lost(), Wound - Restore(), Tolerance), TEXT("still on its cooldown")));
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Tuning().TakedownCooldownSeconds + WorldStep;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, WorldStep);
			}
			TakeDown();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Lost(), FMath::Max(0.0, Wound - Restore() * 2.0), Tolerance)));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
