// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	// Veyra.Abilities.KitOptions.*: a buff's variants and an area that spends its caster's statuses
	// (ADR-018 §1, §6), as The Last Volley and Break the Line.
	TEST_CLASS(KitOptions, "Veyra.Abilities")
	{
		// Fixture values, independent of the committed Abilities.json.
		static constexpr double VariantSeconds = 2.0;
		static constexpr double LongSeconds = 60.0;
		static constexpr double Reach = 300.0;
		static constexpr float WorldStep = 0.1f;

		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;
		UVeyraAbilityLoadoutComponent* Loadout = nullptr;

		BEFORE_EACH()
		{
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_rally")), StatusOf(EVeyraStatusKind::AttackSpeed, 0.2, LongSeconds));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_stack")), StatusOf(EVeyraStatusKind::AttackSpeed, 0.1, LongSeconds));
			Tuning.Statuses.FindChecked(ArchetypeTestId(TEXT("test_stack"))).Stacking = EVeyraStackingPolicy::Stacking;
			Tuning.Statuses.FindChecked(ArchetypeTestId(TEXT("test_stack"))).MaxStacks = 5;

			// An ultimate whose W holds a stronger form while it lasts.
			FVeyraSelfBuffAbilityTuning Ultimate;
			Ultimate.Cast = InstantCast(0.0, LongSeconds, 0.0);
			Ultimate.Statuses.Add(ArchetypeTestId(TEXT("test_rally")));
			FVeyraVariantTuning& Variant = Ultimate.Variants.AddDefaulted_GetRef();
			Variant.Slot = EVeyraAbilitySlot::W;
			Variant.Ability = ArchetypeTestId(TEXT("test_strong_stance"));
			Variant.Cooldown = EVeyraVariantCooldown::Own;
			Variant.DurationSeconds = VariantSeconds;
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_ultimate")), Ultimate);

			FVeyraSelfBuffAbilityTuning Stance;
			Stance.Cast = InstantCast(0.0, LongSeconds, 0.0);
			Stance.Statuses.Add(ArchetypeTestId(TEXT("test_rally")));
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_stance")), Stance);
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_strong_stance")), Stance);

			// A stance in two forms, the stronger an ultimate's variant, each ended by its recast.
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_planted")), StatusOf(EVeyraStatusKind::Planted, 0.0, LongSeconds));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_reach")), StatusOf(EVeyraStatusKind::AttackRange, Reach / 2.0, LongSeconds));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_far_reach")), StatusOf(EVeyraStatusKind::AttackRange, Reach, LongSeconds));
			FVeyraSelfBuffAbilityTuning Dig;
			Dig.Cast = InstantCast(0.0, LongSeconds, 0.0);
			Dig.Recast = EVeyraRecast::EndsEarly;
			Dig.Statuses = { ArchetypeTestId(TEXT("test_planted")), ArchetypeTestId(TEXT("test_reach")) };
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_dig")), Dig);
			Dig.Statuses = { ArchetypeTestId(TEXT("test_planted")), ArchetypeTestId(TEXT("test_far_reach")) };
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_dig_far")), Dig);

			// A cone that spends the caster's stacks.
			FVeyraAreaAbilityTuning Break;
			Break.Cast = InstantCast(0.0, LongSeconds, 0.0);
			Break.Origin = EVeyraAreaOrigin::Caster;
			Break.Zones.AddDefaulted_GetRef().Shape = CircleOf(Reach);
			Break.ConsumesCasterStatuses.Add(ArchetypeTestId(TEXT("test_stack")));
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_break")), Break);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Loadout = Caster->GetPlayerState()->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		/** Moves world time on by Seconds in small steps, timers included. */
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

		bool Holds(EVeyraAbilitySlot Slot, const TCHAR* Ability) const
		{
			const FVeyraLoadoutEntry* Entry = Loadout->FindSlot(Slot);
			return Entry && Entry->Ability == ArchetypeTestId(Ability);
		}

		TEST_METHOD(ABuffsVariantHoldsItsSlotWhileItLasts)
		{
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_ultimate")))));
			ASSERT_THAT(IsTrue(World.Equip(*Caster, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_stance")))));
			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::Q, FVector::ZeroVector) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Holds(EVeyraAbilitySlot::W, TEXT("test_strong_stance"))));
			AdvanceWorld(VariantSeconds + WorldStep);
			ASSERT_THAT(IsTrue(Holds(EVeyraAbilitySlot::W, TEXT("test_stance")), TEXT("it ends with its time")));
		}

		bool Has(const TCHAR* Status) const
		{
			return FArchetypeTestWorld::Has(*Caster, Status);
		}

		TEST_METHOD(AStancesFormsNeverStackAndAVariantFormEndsWithItsVariant)
		{
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_dig")))));
			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::W, FVector::ZeroVector) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Has(TEXT("test_planted")) && Has(TEXT("test_reach"))));
			// An ultimate's variant: W holds the stronger form for a while, as The Last Volley does.
			FVeyraOverrideSpec Variant;
			Variant.Ability = ArchetypeTestId(TEXT("test_dig_far"));
			Variant.DurationSeconds = VariantSeconds;
			Variant.Use = EVeyraOverrideUse::WhileActive;
			ASSERT_THAT(IsTrue(Loadout->Override(*Caster->GetAbilitySystemComponent(), EVeyraAbilitySlot::W, Variant)));
			ASSERT_THAT(IsTrue(Holds(EVeyraAbilitySlot::W, TEXT("test_dig_far"))));
			// Its stronger form, pressed while the stance stands, ends the stance: every form of it.
			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::W, FVector::ZeroVector) == EVeyraCastRejection::None));
			ASSERT_THAT(IsFalse(Has(TEXT("test_planted")) || Has(TEXT("test_reach")) || Has(TEXT("test_far_reach")), TEXT("no form's bonus outlasts the stance")));
			// Taken again in the stronger form, it ends as the variant does.
			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::W, FVector::ZeroVector) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Has(TEXT("test_planted")) && Has(TEXT("test_far_reach")) && !Has(TEXT("test_reach"))));
			AdvanceWorld(VariantSeconds + WorldStep);
			ASSERT_THAT(IsTrue(Holds(EVeyraAbilitySlot::W, TEXT("test_dig"))));
			ASSERT_THAT(IsFalse(Has(TEXT("test_planted")) || Has(TEXT("test_far_reach")), TEXT("the stronger form lasts no longer than its variant")));
		}

		TEST_METHOD(AnAreaSpendsItsCastersStatusesWholly)
		{
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_break")))));
			UAbilitySystemComponent& Abilities = *Caster->GetAbilitySystemComponent();
			const FVeyraStatusSpec Stack = UVeyraAbilitiesTuningSubsystem::FindStatus(ArchetypeTestId(TEXT("test_stack"))).GetValue();
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Abilities, Abilities, Stack) && VeyraCombat::ApplyStatus(Abilities, Abilities, Stack)));
			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::E, FVector(Reach, 0.0, 0.0)) == EVeyraCastRejection::None));
			ASSERT_THAT(IsFalse(World.Has(*Caster, TEXT("test_stack"))));
		}

		TEST_METHOD(ValidationChecksVariantsAndSpentStatuses)
		{
			constexpr int32 RankCounts[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, RankCounts).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, RankCounts), TEXT(" | "))));
			FVeyraAbilitiesTuning Broken = Tuning;
			FVeyraVariantTuning& Variant = Broken.SelfBuff.FindChecked(ArchetypeTestId(TEXT("test_ultimate"))).Variants[0];
			Variant.Ability = ArchetypeTestId(TEXT("no_such_ability"));
			Variant.DurationSeconds = 0.0;
			Broken.Area.FindChecked(ArchetypeTestId(TEXT("test_break"))).ConsumesCasterStatuses.Add(ArchetypeTestId(TEXT("no_such_status")));
			const FString All = FString::Join(VeyraAbilityRules::Validate(Broken, RankCounts), TEXT(" | "));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/selfBuff/test_ultimate/variants/0/ability:")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/selfBuff/test_ultimate/variants/0/durationSeconds:")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/area/test_break/consumesCasterStatuses/1:")), All));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
