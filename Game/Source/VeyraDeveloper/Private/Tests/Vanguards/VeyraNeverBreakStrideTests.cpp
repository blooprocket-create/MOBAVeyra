// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraBasicAttackComponent.h"
#include "CQTest.h"
#include "Passives/VeyraAttackStridePassive.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraVanguards.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	// Veyra.Vanguards.NeverBreakStride.*: Celandrine's passive and the kit around it (Roster Bible §22;
	// ADR-027 §1–§3, §8), from the committed tuning.
	TEST_CLASS(NeverBreakStride, "Veyra.Vanguards")
	{
		// Fixture values: a place within her reach, and a step of world time.
		static constexpr double Near = 100.0;
		static constexpr float WorldStep = 0.1f;
		static constexpr double Tolerance = 1e-3;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Celandrine = nullptr;
		UVeyraBasicAttackComponent* Attacks = nullptr;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Celandrine = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraPlayerState* Participant = Celandrine->GetPlayerState<AVeyraPlayerState>();
			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(*Participant->GetAbilitySystemComponent(), Id(TEXT("celandrine")));
			ASSERT_THAT(IsTrue(Prepared.bPrepared && Cast<UVeyraAttackStridePassive>(Prepared.Passive) != nullptr));
			Participant->SetPassive(Prepared.Passive);
			Attacks = Participant->FindComponentByClass<UVeyraBasicAttackComponent>();
			// Her shots land at Commit here, as a melee attack's would; the flight is tested on its own.
			FVeyraBasicAttackProfile Profile = Attacks->GetProfile();
			Profile.Projectile.Reset();
			ASSERT_THAT(IsTrue(Attacks->SetProfile(Profile)));
		}

		static FVeyraContentId Id(const TCHAR* Text)
		{
			return FVeyraContentId::FromText(Text).GetValue();
		}

		static const FVeyraAttackStrideTuning& Tuning()
		{
			return *UVeyraVanguardsTuningSubsystem::FindAttackStride(Id(TEXT("celandrine_never_break_stride")));
		}

		bool HasBurst() const
		{
			return Tuning().HitStatuses.ContainsByPredicate([this](const FVeyraContentId& Status) { return FArchetypeTestWorld::Has(*Celandrine, *Status.ToString()); });
		}

		/** Lets at least Seconds of world time pass, as her next attack needs; no timer ticks, so statuses hold. */
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

		TEST_METHOD(SheWalksThroughHerWindupsAndOpenRoadLiftsThePenalty)
		{
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Attacks->GetWindupMovementShare(), Tuning().WindupShare, Tolerance), TEXT("reduced speed, not a stop")));
			// OPEN ROAD!'s full-speed stride (§22): the strongest share applies.
			const FVeyraSelfBuffAbilityTuning& OpenRoad = *UVeyraAbilitiesTuningSubsystem::Get().SelfBuff.Find(Id(TEXT("celandrine_open_road")));
			bool bStrode = false;
			for (const FVeyraContentId& StatusId : OpenRoad.Statuses)
			{
				const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId);
				if (Status.IsSet() && Status->Kind == EVeyraStatusKind::MobileAttack)
				{
					UAbilitySystemComponent& Self = *Celandrine->GetAbilitySystemComponent();
					ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Self, Self, Status.GetValue())));
					bStrode = true;
				}
			}
			ASSERT_THAT(IsTrue(bStrode && FMath::IsNearlyEqual(Attacks->GetWindupMovementShare(), 1.0, Tolerance), TEXT("full speed while OPEN ROAD! lasts")));
			ASSERT_THAT(AreEqual(1, OpenRoad.AttackSecondaryImpact.Num(), TEXT("and its attacks pierce behind their target")));
		}

		TEST_METHOD(OnlyHitsOnEnemyVanguardsSpeedHer)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraTestFluxborn& Fluxborn = World.SpawnFluxborn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			ASSERT_THAT(IsTrue(AttackNow(Fluxborn) == EVeyraAttackRejection::None && FArchetypeTestWorld::HealthLost(Fluxborn) > 0.0));
			ASSERT_THAT(IsFalse(HasBurst(), TEXT("a Fluxborn gives nothing")));
			Wait(Attacks->GetTiming().IntervalSeconds);
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(0.0, Near, 0.0));
			ASSERT_THAT(IsTrue(AttackNow(Enemy) == EVeyraAttackRejection::None && FArchetypeTestWorld::HealthLost(Enemy) > 0.0));
			ASSERT_THAT(IsTrue(HasBurst(), TEXT("an enemy Vanguard speeds her")));
		}

		TEST_METHOD(DoubletimeQuickensThreeAttacksWithoutTouchingTheInterval)
		{
			const FVeyraEmpoweredAttackAbilityTuning& Doubletime = *UVeyraAbilitiesTuningSubsystem::Get().EmpoweredAttack.Find(Id(TEXT("celandrine_doubletime")));
			// Canon: the next three ordinary basic attacks, with shorter windups (§22).
			ASSERT_THAT(AreEqual(3, Doubletime.Attacks));
			ASSERT_THAT(IsTrue(Doubletime.WindupScale < 1.0));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Celandrine, EVeyraAbilitySlot::Q, Id(TEXT("celandrine_doubletime")))));
			const double Interval = Attacks->GetTiming().IntervalSeconds;
			ASSERT_THAT(IsTrue(VeyraAbilities::TryCast(*Celandrine->GetAbilitySystemComponent(), EVeyraAbilitySlot::Q, FVeyraCastTarget()) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Attacks->GetTiming().IntervalSeconds, Interval, Tolerance), TEXT("no attack-timer bypass")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
