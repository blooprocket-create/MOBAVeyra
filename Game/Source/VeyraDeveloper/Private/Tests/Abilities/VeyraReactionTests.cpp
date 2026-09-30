// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "AbilitySystemComponent.h"
#include "CQTest.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Events/VeyraAbilityEvents.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	// Veyra.Abilities.Reactions.*: effects that react to the statuses their target holds, and stacks
	// that turn into another status at their most (ADR-026 §1–§2).
	TEST_CLASS(Reactions, "Veyra.Abilities")
	{
		// Fixture values, independent of the committed Abilities.json.
		static constexpr double LongSeconds = 60.0;
		static constexpr double Blow = 50.0;
		static constexpr double PerMark = 10.0;
		static constexpr int32 MostMarks = 3;
		static constexpr double Near = 200.0;

		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;

		BEFORE_EACH()
		{
			FVeyraStatusTuning Mark = StatusOf(EVeyraStatusKind::Counter, 0.0, LongSeconds);
			Mark.Stacking = EVeyraStackingPolicy::Stacking;
			Mark.MaxStacks = MostMarks;
			Mark.AtMaxStacks = { ArchetypeTestId(TEXT("test_primed")) };
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_mark")), Mark);
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_primed")), StatusOf(EVeyraStatusKind::Counter, 0.0, LongSeconds));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_root")), StatusOf(EVeyraStatusKind::Root, 0.0, LongSeconds));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_stun")), StatusOf(EVeyraStatusKind::Stun, 0.0, LongSeconds));
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);
			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		UAbilitySystemComponent& Self() const
		{
			return *Caster->GetAbilitySystemComponent();
		}

		void Hit(AActor& Target, const FVeyraEffectBundleTuning& Effects) const
		{
			VeyraEffectDelivery::Apply(Self(), Target, VeyraEffectDelivery::Prepare(Self(), Effects, 1), FVeyraEffectFrame(), FVeyraAbilityHitSource());
		}

		void Mark(AVeyraVanguardCharacter& Target, int32 Times)
		{
			const TOptional<FVeyraStatusSpec> Spec = UVeyraAbilitiesTuningSubsystem::FindStatus(ArchetypeTestId(TEXT("test_mark")));
			ASSERT_THAT(IsTrue(Spec.IsSet()));
			for (int32 Index = 0; Index < Times; ++Index)
			{
				ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Self(), *Target.GetAbilitySystemComponent(), Spec.GetValue())));
			}
		}

		static FVeyraReactionTuning Reaction(const TCHAR* Status, EVeyraReactionConsume Consume)
		{
			FVeyraReactionTuning Result;
			Result.Status = ArchetypeTestId(Status);
			Result.Consume = Consume;
			return Result;
		}

		TEST_METHOD(AReactionAddsDamagePerStackHeldAndConsumesThem)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			FVeyraEffectBundleTuning Rupture;
			Rupture.Damage.Add(FVeyraDamageTuning{ EVeyraDamageType::TrueDamage, { Blow }, 0.0, 0.0 });
			FVeyraReactionTuning& Burst = Rupture.Reactions.Add_GetRef(Reaction(TEXT("test_mark"), EVeyraReactionConsume::Consume));
			Burst.Scaling = EVeyraReactionScaling::PerStack;
			Burst.Damage.Add(FVeyraDamageTuning{ EVeyraDamageType::TrueDamage, { PerMark }, 0.0, 0.0 });

			Mark(Enemy, 2);
			Hit(Enemy, Rupture);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Enemy), Blow + 2 * PerMark, 1e-3), FString::SanitizeFloat(World.HealthLost(Enemy))));
			ASSERT_THAT(IsFalse(World.Has(Enemy, TEXT("test_mark")), TEXT("consumed")));
			Hit(Enemy, Rupture);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Enemy), 2 * Blow + 2 * PerMark, 1e-3), TEXT("nothing left to react to")));
		}

		TEST_METHOD(AReactionTakesThePlaceOfTheBundlesStatus)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Primed = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			AVeyraVanguardCharacter& Plain = World.Spawn(EVeyraTeam::B, FVector(0.0, Near, 0.0));
			FVeyraEffectBundleTuning Cure;
			Cure.Statuses = { ArchetypeTestId(TEXT("test_root")) };
			FVeyraReactionTuning& Harden = Cure.Reactions.Add_GetRef(Reaction(TEXT("test_primed"), EVeyraReactionConsume::Consume));
			Harden.Statuses = { ArchetypeTestId(TEXT("test_stun")) };
			Harden.Replaces = { ArchetypeTestId(TEXT("test_root")) };

			Mark(Primed, MostMarks);
			ASSERT_THAT(IsTrue(World.Has(Primed, TEXT("test_primed"))));
			Hit(Primed, Cure);
			Hit(Plain, Cure);
			ASSERT_THAT(IsTrue(World.Has(Primed, TEXT("test_stun")) && !World.Has(Primed, TEXT("test_root")), TEXT("a stun in place of the root (ADR-026 §1)")));
			ASSERT_THAT(IsFalse(World.Has(Primed, TEXT("test_primed")), TEXT("and the reaction spent what it reacted to")));
			ASSERT_THAT(IsTrue(World.Has(Plain, TEXT("test_root")) && !World.Has(Plain, TEXT("test_stun"))));
		}

		TEST_METHOD(AStatusAtItsMostStacksBecomesAnother)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			Mark(Enemy, MostMarks - 1);
			ASSERT_THAT(IsTrue(World.Has(Enemy, TEXT("test_mark")) && !World.Has(Enemy, TEXT("test_primed"))));
			Mark(Enemy, 1);
			ASSERT_THAT(IsTrue(World.Has(Enemy, TEXT("test_primed")) && !World.Has(Enemy, TEXT("test_mark")), TEXT("at its most it becomes the other (ADR-026 §2)")));
		}

		TEST_METHOD(ValidationChecksReactionsAndConversions)
		{
			FVeyraAbilitiesTuning Broken = Tuning;
			Broken.Statuses[ArchetypeTestId(TEXT("test_primed"))].AtMaxStacks = { ArchetypeTestId(TEXT("test_mark")) };
			FVeyraAreaAbilityTuning Area;
			Area.Cast.CooldownSecondsByRank = { 1.0 };
			Area.Cast.ResourceCostByRank = { 0.0 };
			FVeyraAreaZoneTuning& Zone = Area.Zones.AddDefaulted_GetRef();
			Zone.Shape.Kind = EVeyraShapeKind::Circle;
			Zone.Shape.Radius = Near;
			Zone.Effects.Reactions.Add(Reaction(TEXT("no_such_status"), EVeyraReactionConsume::Keep));
			FVeyraReactionTuning& Stray = Zone.Effects.Reactions.Add_GetRef(Reaction(TEXT("test_mark"), EVeyraReactionConsume::Consume));
			Stray.Replaces = { ArchetypeTestId(TEXT("test_root")) };
			Broken.Area.Add(ArchetypeTestId(TEXT("test_broken")), Area);
			constexpr int32 RankCounts[] = { 5, 3 };
			const FString All = FString::Join(VeyraAbilityRules::Validate(Broken, RankCounts), TEXT(" | "));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/statuses/test_primed/atMaxStacks")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/reactions/0/status")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/reactions/0:")), TEXT("a reaction that does nothing")));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/reactions/1/replaces")), TEXT("it replaces a status the effects do not give")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
