// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CombatState/VeyraCombatStateComponent.h"
#include "CQTest.h"
#include "Passives/VeyraUnreturnedPassive.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraVanguards.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	// Veyra.Vanguards.Unreturned.*: Torr's passive and the kit around it (Roster Bible §9; ADR-028 §3,
	// §5–§7), from the committed tuning.
	TEST_CLASS(Unreturned, "Veyra.Vanguards")
	{
		static constexpr double Tolerance = 1e-3;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Torr = nullptr;
		UVeyraUnreturnedPassive* Passive = nullptr;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Torr = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraPlayerState* Participant = Torr->GetPlayerState<AVeyraPlayerState>();
			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(*Participant->GetAbilitySystemComponent(), Id(TEXT("torr")));
			Passive = Cast<UVeyraUnreturnedPassive>(Prepared.Passive);
			ASSERT_THAT(IsTrue(Prepared.bPrepared && Passive != nullptr));
			Participant->SetPassive(Prepared.Passive);
		}

		static FVeyraContentId Id(const TCHAR* Text)
		{
			return FVeyraContentId::FromText(Text).GetValue();
		}

		static const FVeyraUnreturnedTuning& Tuning()
		{
			return *UVeyraVanguardsTuningSubsystem::FindUnreturned(Id(TEXT("torr_unreturned")));
		}

		double MaxHealth() const
		{
			return Torr->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
		}

		/** Wounds him to Fraction of his Health from an enemy Vanguard, which puts him in Vanguard combat. */
		void WoundTo(AActor& Enemy, double Fraction) const
		{
			FVeyraRawDamageEvent Hurt;
			Hurt.Components.Add({ EVeyraDamageType::TrueDamage, MaxHealth() * (1.0 - Fraction) - FArchetypeTestWorld::HealthLost(*Torr) });
			VeyraCombat::DealDamage(*UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Enemy), *Torr->GetAbilitySystemComponent(), Hurt);
		}

		TEST_METHOD(OutOfVanguardCombatHisCoreRestoresAShareOfWhatHeLacks)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(1000.0, 0.0, 0.0));
			WoundTo(Enemy, 0.6);
			const double Lost = FArchetypeTestWorld::HealthLost(*Torr);
			Passive->Check();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FArchetypeTestWorld::HealthLost(*Torr), Lost, Tolerance), TEXT("nothing while in Vanguard combat")));
			Torr->GetPlayerState()->FindComponentByClass<UVeyraCombatStateComponent>()->Clear();
			Passive->Check();
			const double Expected = Lost - Lost * Tuning().RestoreFractionPerSecond * Tuning().CheckSeconds;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FArchetypeTestWorld::HealthLost(*Torr), Expected, Tolerance),
				*FString::Printf(TEXT("out of it, a share of what he lacks: lost %g"), FArchetypeTestWorld::HealthLost(*Torr))));
		}

		TEST_METHOD(AsHisHealthFallsHisCoreGrowsLessEncumbered)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(1000.0, 0.0, 0.0));
			const TArray<FVeyraUnreturnedThresholdTuning>& Thresholds = Tuning().Thresholds;
			ASSERT_THAT(IsTrue(Thresholds.Num() >= 2 && Thresholds[0].HealthFraction > Thresholds[1].HealthFraction));
			const auto Holds = [this](const FVeyraUnreturnedThresholdTuning& Threshold) {
				return FArchetypeTestWorld::Has(*Torr, *Threshold.Statuses[0].ToString());
			};
			Passive->Check();
			ASSERT_THAT(IsFalse(Holds(Thresholds[0]) || Holds(Thresholds[1]), TEXT("whole, nothing")));
			WoundTo(Enemy, (Thresholds[0].HealthFraction + Thresholds[1].HealthFraction) / 2.0);
			Passive->Check();
			ASSERT_THAT(IsTrue(Holds(Thresholds[0]) && !Holds(Thresholds[1]), TEXT("below the first threshold")));
			WoundTo(Enemy, Thresholds[1].HealthFraction / 2.0);
			Passive->Check();
			ASSERT_THAT(IsTrue(Holds(Thresholds[0]) && Holds(Thresholds[1]), TEXT("below both")));
		}

		TEST_METHOD(TheKitMatchesItsCanon)
		{
			const FVeyraAbilitiesTuning& Abilities = UVeyraAbilitiesTuningSubsystem::Get();
			const auto KindOf = [&Abilities](const FVeyraContentId& Status) { return Abilities.Statuses.FindChecked(Status).Kind; };
			const FVeyraAreaAbilityTuning& Mass = Abilities.Area.FindChecked(Id(TEXT("torr_battering_mass")));
			ASSERT_THAT(IsTrue(Mass.Zones[0].Effects.Displacement.Num() == 1 && !Mass.Zones[0].Effects.Displacement[0].CollisionStatuses.IsEmpty(),
				TEXT("Battering Mass: a push that stuns on collision (§9)")));
			const FVeyraAreaAbilityTuning& Anchor = Abilities.Area.FindChecked(Id(TEXT("torr_anchor")));
			ASSERT_THAT(IsTrue(Anchor.Linger.Num() == 1 && Anchor.Cast.RecastWindow.Num() == 1, TEXT("Anchor: a field with a recast")));
			const FVeyraAreaAbilityTuning& Rip = Abilities.Area.FindChecked(Anchor.Cast.RecastWindow[0].Ability);
			ASSERT_THAT(IsTrue(Rip.Origin == EVeyraAreaOrigin::CastersLingeringArea && Rip.OriginAbility == TArray<FVeyraContentId>{ Id(TEXT("torr_anchor")) },
				TEXT("which rips the anchor up where it stands")));
			ASSERT_THAT(IsTrue(Anchor.Linger[0].EnemyStatuses.ContainsByPredicate([&KindOf](const FVeyraContentId& Each) { return KindOf(Each) == EVeyraStatusKind::Grounded; })));
			const FVeyraSelfBuffAbilityTuning& Over = Abilities.SelfBuff.FindChecked(Id(TEXT("torr_overcapacity")));
			ASSERT_THAT(IsTrue(Over.Statuses.ContainsByPredicate([&KindOf](const FVeyraContentId& Each) { return KindOf(Each) == EVeyraStatusKind::BodyScale; })
				&& Over.Statuses.ContainsByPredicate([&KindOf](const FVeyraContentId& Each) { return KindOf(Each) == EVeyraStatusKind::DisplacementImmunity; })
				&& !Over.TemporaryHealth.IsEmpty() && Over.Variants.Num() == 2 && Over.EndPayload.Num() == 1 && Over.EndPayload[0].Displacement.Num() == 1,
				TEXT("OVERCAPACITY: larger, immovable, Temporary Health, larger Q and E, and a venting knockback")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
