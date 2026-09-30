// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraBasicAttackComponent.h"
#include "CQTest.h"
#include "Passives/VeyraReclaimPassive.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraVanguards.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	// Veyra.Vanguards.Reclaim.*: Silt's passive and the kit around it (Roster Bible §3; ADR-028 §1, §4),
	// from the committed tuning.
	TEST_CLASS(Reclaim, "Veyra.Vanguards")
	{
		// Fixture values: a place within his reach, a wound to heal, and a step of world time.
		static constexpr double Near = 100.0;
		static constexpr double Wound = 200.0;
		static constexpr float WorldStep = 0.1f;
		static constexpr double Tolerance = 1e-3;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Silt = nullptr;
		UVeyraBasicAttackComponent* Attacks = nullptr;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Silt = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraPlayerState* Participant = Silt->GetPlayerState<AVeyraPlayerState>();
			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(*Participant->GetAbilitySystemComponent(), Id(TEXT("silt")));
			ASSERT_THAT(IsTrue(Prepared.bPrepared && Cast<UVeyraReclaimPassive>(Prepared.Passive) != nullptr));
			Participant->SetPassive(Prepared.Passive);
			Attacks = Participant->FindComponentByClass<UVeyraBasicAttackComponent>();
			// His shots land at Commit here; the flight is tested on its own.
			FVeyraBasicAttackProfile Profile = Attacks->GetProfile();
			Profile.Projectile.Reset();
			ASSERT_THAT(IsTrue(Attacks->SetProfile(Profile)));
		}

		static FVeyraContentId Id(const TCHAR* Text)
		{
			return FVeyraContentId::FromText(Text).GetValue();
		}

		static const FVeyraReclaimTuning& Tuning()
		{
			return *UVeyraVanguardsTuningSubsystem::FindReclaim(Id(TEXT("silt_reclaim")));
		}

		void Coat(AActor& Unit) const
		{
			const TOptional<FVeyraStatusSpec> Sandy = UVeyraAbilitiesTuningSubsystem::FindStatus(Tuning().Mark);
			VeyraCombat::ApplyStatus(*Silt->GetAbilitySystemComponent(), *UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit), Sandy.GetValue());
		}

		/** Lets at least Seconds of world time pass; no timer ticks, so statuses hold. */
		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, WorldStep);
			}
		}

		EVeyraAttackRejection AttackNow(AActor& Target) const
		{
			const EVeyraAttackRejection Rejection = Attacks->StartAttack(Target);
			if (Rejection == EVeyraAttackRejection::None)
			{
				Attacks->Commit();
			}
			return Rejection;
		}

		TEST_METHOD(AnAttackOnASandyEnemyTearsTheSandBackIntoHimOncePerLockout)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			FVeyraRawDamageEvent Hurt;
			Hurt.Components.Add({ EVeyraDamageType::TrueDamage, Wound });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Enemy.GetAbilitySystemComponent(), *Silt->GetAbilitySystemComponent(), Hurt)));
			const FString Mark = Tuning().Mark.ToString();

			ASSERT_THAT(IsTrue(AttackNow(Enemy) == EVeyraAttackRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FArchetypeTestWorld::HealthLost(*Silt), Wound, Tolerance), TEXT("nothing without Sandy")));

			Coat(Enemy);
			Wait(Attacks->GetTiming().IntervalSeconds);
			ASSERT_THAT(IsTrue(AttackNow(Enemy) == EVeyraAttackRejection::None));
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(Enemy, *Mark), TEXT("the attack consumes Sandy")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FArchetypeTestWorld::HealthLost(*Silt), Wound - Tuning().HealAmount, Tolerance),
				*FString::Printf(TEXT("and heals him: lost %g"), FArchetypeTestWorld::HealthLost(*Silt))));

			// Within the lockout the next attack leaves the fresh Sandy for later.
			Coat(Enemy);
			Wait(Attacks->GetTiming().IntervalSeconds);
			ASSERT_THAT(IsTrue(AttackNow(Enemy) == EVeyraAttackRejection::None));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(Enemy, *Mark), TEXT("once per lockout per target")));
			Wait(Tuning().LockoutSeconds);
			ASSERT_THAT(IsTrue(AttackNow(Enemy) == EVeyraAttackRejection::None));
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(Enemy, *Mark), TEXT("after it, again")));
		}

		TEST_METHOD(TheKitMatchesItsCanon)
		{
			const FVeyraAbilitiesTuning& Abilities = UVeyraAbilitiesTuningSubsystem::Get();
			const auto KindOf = [&Abilities](const FVeyraContentId& Status) { return Abilities.Statuses.FindChecked(Status).Kind; };
			const FVeyraAreaAbilityTuning& Sink = Abilities.Area.FindChecked(Id(TEXT("silt_sink")));
			ASSERT_THAT(IsTrue(Sink.DelaySeconds > 0.0 && Sink.Zones.Num() == 2, TEXT("Sink: after a delay, a centre and an edge (§3)")));
			ASSERT_THAT(IsTrue(Sink.Zones[0].Effects.Statuses.ContainsByPredicate([&KindOf](const FVeyraContentId& Each) { return KindOf(Each) == EVeyraStatusKind::Stun; })));
			ASSERT_THAT(IsTrue(Sink.Zones[1].Effects.Statuses.ContainsByPredicate([&KindOf](const FVeyraContentId& Each) { return KindOf(Each) == EVeyraStatusKind::Slow; })));
			const FVeyraAreaAbilityTuning& Sandstorm = Abilities.Area.FindChecked(Id(TEXT("silt_sandstorm")));
			ASSERT_THAT(IsTrue(Sandstorm.Linger.Num() == 1
				&& Sandstorm.Linger[0].EnemyStatuses.ContainsByPredicate([&KindOf](const FVeyraContentId& Each) { return KindOf(Each) == EVeyraStatusKind::Blind; }),
				TEXT("Sandstorm: a lasting storm that blinds")));
			const FVeyraAreaAbilityTuning& Buried = Abilities.Area.FindChecked(Id(TEXT("silt_buried_alive")));
			ASSERT_THAT(IsTrue(Buried.Linger.Num() == 1 && Buried.Linger[0].PulseEffects.Num() == 1 && !Buried.Linger[0].PulseEffects[0].Reactions.IsEmpty()
				&& Buried.Linger[0].EndEffects.Num() == 1, TEXT("Buried Alive: Sandy erupts at its pulses, and it ends in a knockup")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
