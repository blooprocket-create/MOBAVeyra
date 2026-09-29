// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Engine/World.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	// Veyra.Abilities.HuntingOptions.*: damage multiplied against a kind of unit, a channel its caster
	// may move through, statuses on the caster at Commit, and Health restored from hits up to a cap for
	// the cast (ADR-018 §6), as Gorraveth's kit uses them.
	TEST_CLASS(HuntingOptions, "Veyra.Abilities")
	{
		// Fixture values, independent of the committed Abilities.json.
		static constexpr double LongSeconds = 60.0;
		static constexpr double Reach = 300.0;
		static constexpr double Blow = 100.0;
		static constexpr double Multiplier = 1.5;
		static constexpr double ChannelSeconds = 0.8;
		static constexpr double PerHit = 0.04;
		static constexpr double Cap = 0.06;
		static constexpr double Wound = 200.0;
		static constexpr float WorldStep = 0.05f;
		static constexpr double Tolerance = 1e-3;

		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;

		BEFORE_EACH()
		{
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_drag")), StatusOf(EVeyraStatusKind::Slow, 0.3, ChannelSeconds));

			FVeyraAreaAbilityTuning Rake;
			Rake.Cast = InstantCast(0.0, LongSeconds, 0.0);
			Rake.Origin = EVeyraAreaOrigin::Caster;
			Rake.ChannelTicks = 2;
			Rake.ChannelSeconds = ChannelSeconds;
			Rake.ChannelMovement = EVeyraCastMovement::Free;
			Rake.CasterStatuses.Add(ArchetypeTestId(TEXT("test_drag")));
			FVeyraAreaZoneTuning& Zone = Rake.Zones.AddDefaulted_GetRef();
			Zone.Shape = CircleOf(Reach);
			FVeyraDamageTuning& Damage = Zone.Effects.Damage.AddDefaulted_GetRef();
			Damage.Type = EVeyraDamageType::TrueDamage;
			Damage.AmountByRank = { Blow };
			FVeyraUnitKindMultiplierTuning& Harder = Zone.Effects.UnitKindMultipliers.AddDefaulted_GetRef();
			Harder.Kind = EVeyraUnitKind::Fluxborn;
			Harder.Multiplier = Multiplier;
			FVeyraHealOnHitTuning& Heal = Rake.HealOnHit.AddDefaulted_GetRef();
			Heal.UnitKinds = { EVeyraUnitKind::Fluxborn };
			Heal.MaxHealthRatioPerHit = PerHit;
			Heal.CapMaxHealthRatio = Cap;
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_rake")), Rake);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_rake")))));
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		void AdvanceWorld(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, WorldStep);
				++GFrameCounter;
				World.GetTimerManager().Tick(WorldStep);
			}
		}

		EVeyraCastRejection Rake() const
		{
			return FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::W, FVector::ZeroVector);
		}

		TEST_METHOD(ItHitsTheNamedKindHarder)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Reach / 2.0, 0.0, 0.0));
			AVeyraTestFluxborn& Minion = World.SpawnFluxborn(EVeyraTeam::B, FVector(0.0, Reach / 2.0, 0.0));
			ASSERT_THAT(IsTrue(Rake() == EVeyraCastRejection::None));
			AdvanceWorld(ChannelSeconds / 2.0 + WorldStep * 3.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FArchetypeTestWorld::HealthLost(Enemy), Blow, Tolerance)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FArchetypeTestWorld::HealthLost(Minion), Blow * Multiplier, Tolerance),
				FString::Printf(TEXT("the Fluxborn lost %g"), FArchetypeTestWorld::HealthLost(Minion))));
		}

		TEST_METHOD(ItsCasterMovesThroughTheChannelUnderItsStatusesAndItSweepsWhereItStands)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Distant = World.Spawn(EVeyraTeam::B, FVector(Reach * 3.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(Rake() == EVeyraCastRejection::None));
			ASSERT_THAT(IsFalse(Caster->GetVeyraMovement()->IsMovementLocked(), TEXT("free to move while it channels")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(*Caster, TEXT("test_drag"))));
			// It walks up to the distant enemy before its first sweep.
			Caster->SetActorLocation(FVector(Reach * 3.0 - Reach / 2.0, 0.0, Caster->GetActorLocation().Z));
			AdvanceWorld(ChannelSeconds / 2.0 + WorldStep * 3.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FArchetypeTestWorld::HealthLost(Distant), Blow, Tolerance), TEXT("swept where it stood")));
		}

		TEST_METHOD(HealthFromHitsStopsAtTheCastsCap)
		{
			FArchetypeTestWorld World{ Spawner };
			for (int32 Index = 0; Index < 3; ++Index)
			{
				World.SpawnFluxborn(EVeyraTeam::B, FVector(Reach / 2.0, (Index - 1) * Reach / 4.0, 0.0));
			}
			UAbilitySystemComponent& Self = *Caster->GetAbilitySystemComponent();
			FVeyraRawDamageEvent Hurt;
			Hurt.Components.Add({ EVeyraDamageType::TrueDamage, Wound });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*World.Spawn(EVeyraTeam::B, FVector(-Reach * 3.0, 0.0, 0.0)).GetAbilitySystemComponent(), Self, Hurt)));
			ASSERT_THAT(IsTrue(Rake() == EVeyraCastRejection::None));
			AdvanceWorld(ChannelSeconds + WorldStep);
			// Three hits a sweep, two sweeps, but no more than the cap for the whole cast.
			const double MaxHealth = VeyraCombatTests::ExampleStats().MaxHealth;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FArchetypeTestWorld::HealthLost(*Caster), Wound - MaxHealth * Cap, Tolerance),
				FString::Printf(TEXT("lost %g"), FArchetypeTestWorld::HealthLost(*Caster))));
		}

		TEST_METHOD(AStatusSparesAUnitTheDisplacement)
		{
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_aloft")), StatusOf(EVeyraStatusKind::Knockup, 0.0, LongSeconds));
			FVeyraEffectBundleTuning& Effects = Tuning.Area.FindChecked(ArchetypeTestId(TEXT("test_rake"))).Zones[0].Effects;
			FVeyraDisplacementTuning& Push = Effects.Displacement.AddDefaulted_GetRef();
			Push.Direction = EVeyraDisplacementDirection::AwayFromOrigin;
			Push.Distance = Reach;
			Push.Speed = Reach * 4.0;
			Effects.DisplacementUnlessStatuses.Add(ArchetypeTestId(TEXT("test_aloft")));
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Aloft = World.Spawn(EVeyraTeam::B, FVector(Reach / 2.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Grounded = World.Spawn(EVeyraTeam::B, FVector(0.0, Reach / 2.0, 0.0));
			const FVeyraStatusSpec Knocked = UVeyraAbilitiesTuningSubsystem::FindStatus(ArchetypeTestId(TEXT("test_aloft"))).GetValue();
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Aloft.GetAbilitySystemComponent(), *Aloft.GetAbilitySystemComponent(), Knocked)));
			ASSERT_THAT(IsTrue(Rake() == EVeyraCastRejection::None));
			AdvanceWorld(ChannelSeconds / 2.0 + WorldStep * 3.0);
			ASSERT_THAT(IsTrue(Grounded.GetVeyraMovement()->IsDisplaced() && !Aloft.GetVeyraMovement()->IsDisplaced(), TEXT("one displacement per target per cast")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(Aloft) > 0.0, TEXT("it still takes the damage")));
		}

		TEST_METHOD(ValidationKeepsTheOptionsInShape)
		{
			constexpr int32 RankCounts[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, RankCounts).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, RankCounts), TEXT(" | "))));
			FVeyraAbilitiesTuning Broken = Tuning;
			FVeyraAreaAbilityTuning& Rake = Broken.Area.FindChecked(ArchetypeTestId(TEXT("test_rake")));
			Rake.Zones[0].Effects.UnitKindMultipliers[0].Multiplier = 0.5;
			Rake.HealOnHit[0].CapMaxHealthRatio = PerHit / 2.0;
			Rake.ChannelTicks = 1;
			const FString All = FString::Join(VeyraAbilityRules::Validate(Broken, RankCounts), TEXT(" | "));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/unitKindMultipliers/0:")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/area/test_rake/healOnHit/0:")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/area/test_rake/channelMovement:")), All));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
