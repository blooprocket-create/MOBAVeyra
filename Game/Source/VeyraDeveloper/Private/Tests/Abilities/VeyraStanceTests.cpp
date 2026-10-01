// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	/** Fixture values for the stance tests. */
	namespace StanceFixture
	{
		constexpr double VeilCooldown = 10.0;
		constexpr double BladeCooldown = 4.0;
		constexpr double WindowSeconds = 5.0;
		constexpr double Speed = 0.1;
		constexpr double LongSeconds = 60.0;
		/** Enough XP for a few levels, so W takes a point too. */
		constexpr double ManyLevels = 5000.0;

		inline FVeyraSelfBuffAbilityTuning Buff(double Cooldown)
		{
			FVeyraSelfBuffAbilityTuning Ability;
			Ability.Cast = InstantCast(0.0, Cooldown, 0.0);
			Ability.Statuses = { ArchetypeTestId(TEXT("test_stance_haste")) };
			return Ability;
		}

		inline FVeyraStanceSlotTuning Held(EVeyraAbilitySlot Slot, const TCHAR* Ability)
		{
			FVeyraStanceSlotTuning Entry;
			Entry.Slot = Slot;
			Entry.Ability = ArchetypeTestId(Ability);
			return Entry;
		}
	}

	// Veyra.Abilities.Stance.*: an ability that swaps its caster's own abilities for another set, each
	// set with its own cooldowns and each slot with its rank, while a follow-up waits for its own
	// ability (ADR-031 §3).
	TEST_CLASS(Stance, "Veyra.Abilities")
	{
		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;
		UVeyraAbilityLoadoutComponent* Loadout = nullptr;
		UVeyraCooldownComponent* Cooldowns = nullptr;

		BEFORE_EACH()
		{
			using namespace StanceFixture;
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_stance_haste")), StatusOf(EVeyraStatusKind::MoveSpeed, Speed, LongSeconds));
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_veil_q")), Buff(VeilCooldown));
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_blade_q")), Buff(BladeCooldown));
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_blade_w")), Buff(BladeCooldown));
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_veil_w_again")), Buff(0.0));
			FVeyraSelfBuffAbilityTuning OpensAFollowUp = Buff(VeilCooldown);
			FVeyraRecastTuning& Window = OpensAFollowUp.Cast.RecastWindow.AddDefaulted_GetRef();
			Window.Ability = ArchetypeTestId(TEXT("test_veil_w_again"));
			Window.WindowSeconds = WindowSeconds;
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_veil_w")), OpensAFollowUp);
			FVeyraStanceAbilityTuning Switch;
			Switch.Cast = InstantCast(0.0, 0.0, 0.0);
			Switch.Slots = { Held(EVeyraAbilitySlot::Q, TEXT("test_blade_q")), Held(EVeyraAbilitySlot::W, TEXT("test_blade_w")) };
			Tuning.Stance.Add(ArchetypeTestId(TEXT("test_switch")), Switch);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, { 5, 3, 1 }).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, { 5, 3, 1 }), TEXT(" | "))));

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			APlayerState* PlayerState = Caster->GetPlayerState();
			Loadout = PlayerState->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
			Cooldowns = PlayerState->FindComponentByClass<UVeyraCooldownComponent>();
			// R learnt from the start, as Angeru's rank shape does; points for Q and W.
			FVeyraRankShape Shape;
			Shape.BasicAbilityMaxRank = 5;
			Shape.bUltimateInnate = true;
			UVeyraProgressionComponent* Progression = PlayerState->FindComponentByClass<UVeyraProgressionComponent>();
			Progression->Initialize(FVeyraStatGrowth(), 0.0, &Shape);
			Progression->AddExperience(ManyLevels);
			UAbilitySystemComponent& Abilities = *Caster->GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(Loadout->Grant(Abilities, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_veil_q")))));
			ASSERT_THAT(IsTrue(Loadout->Grant(Abilities, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_veil_w")))));
			ASSERT_THAT(IsTrue(Loadout->Grant(Abilities, EVeyraAbilitySlot::R, ArchetypeTestId(TEXT("test_switch")))));
			ASSERT_THAT(IsTrue(Progression->AllocateRank(EVeyraAbilitySlot::Q) == EVeyraRankRefusal::None));
			ASSERT_THAT(IsTrue(Progression->AllocateRank(EVeyraAbilitySlot::W) == EVeyraRankRefusal::None));
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		EVeyraCastRejection Cast(EVeyraAbilitySlot Slot) const
		{
			return FArchetypeTestWorld::CastAt(*Caster, Slot, Caster->GetActorLocation());
		}

		FVeyraContentId In(EVeyraAbilitySlot Slot) const
		{
			const FVeyraLoadoutEntry* Entry = Loadout->FindSlot(Slot);
			return Entry ? Entry->Ability : FVeyraContentId();
		}

		double CooldownOf(const TCHAR* Ability) const
		{
			return Cooldowns->GetRemainingSecondsNow(ArchetypeTestId(Ability));
		}

		TEST_METHOD(ItSwapsTheSlotsItNamesAndBack)
		{
			ASSERT_THAT(IsTrue(Cast(EVeyraAbilitySlot::R) == EVeyraCastRejection::None, TEXT("learnt from the start, with no point spent")));
			ASSERT_THAT(IsTrue(In(EVeyraAbilitySlot::Q) == ArchetypeTestId(TEXT("test_blade_q")) && In(EVeyraAbilitySlot::W) == ArchetypeTestId(TEXT("test_blade_w"))));
			ASSERT_THAT(IsTrue(In(EVeyraAbilitySlot::R) == ArchetypeTestId(TEXT("test_switch")), TEXT("the stance keeps its own slot")));
			ASSERT_THAT(IsTrue(Cast(EVeyraAbilitySlot::R) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(In(EVeyraAbilitySlot::Q) == ArchetypeTestId(TEXT("test_veil_q")) && In(EVeyraAbilitySlot::W) == ArchetypeTestId(TEXT("test_veil_w"))));
		}

		TEST_METHOD(EachSetKeepsItsOwnCooldownsAndEachSlotItsRank)
		{
			ASSERT_THAT(IsTrue(Cast(EVeyraAbilitySlot::Q) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Cast(EVeyraAbilitySlot::Q) == EVeyraCastRejection::OnCooldown));
			ASSERT_THAT(IsTrue(Cast(EVeyraAbilitySlot::R) == EVeyraCastRejection::None));
			// The other set's Q is ready, and ranked as the slot is.
			ASSERT_THAT(IsTrue(Cast(EVeyraAbilitySlot::Q) == EVeyraCastRejection::None, TEXT("its own cooldown, and the slot's rank")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(CooldownOf(TEXT("test_blade_q")), StanceFixture::BladeCooldown)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(CooldownOf(TEXT("test_veil_q")), StanceFixture::VeilCooldown), TEXT("the stowed one still cools down")));
			ASSERT_THAT(IsTrue(Cast(EVeyraAbilitySlot::R) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Cast(EVeyraAbilitySlot::Q) == EVeyraCastRejection::OnCooldown, TEXT("back, it is still cooling down")));
		}

		TEST_METHOD(AFollowUpWaitsInTheOtherStanceAndComesBackWithItsAbility)
		{
			ASSERT_THAT(IsTrue(Cast(EVeyraAbilitySlot::W) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(In(EVeyraAbilitySlot::W) == ArchetypeTestId(TEXT("test_veil_w_again")), TEXT("the follow-up opens")));
			ASSERT_THAT(IsTrue(Cast(EVeyraAbilitySlot::R) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(In(EVeyraAbilitySlot::W) == ArchetypeTestId(TEXT("test_blade_w")), TEXT("unseen in the other stance")));
			ASSERT_THAT(IsFalse(Loadout->IsOverridden(EVeyraAbilitySlot::W)));
			ASSERT_THAT(IsTrue(Cast(EVeyraAbilitySlot::R) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(In(EVeyraAbilitySlot::W) == ArchetypeTestId(TEXT("test_veil_w_again")), TEXT("back with its ability")));
			ASSERT_THAT(IsTrue(Cast(EVeyraAbilitySlot::W) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(In(EVeyraAbilitySlot::W) == ArchetypeTestId(TEXT("test_veil_w")), TEXT("used once")));
		}

		TEST_METHOD(ValidationRefusesASlotTwiceOrItsOwnSlot)
		{
			FVeyraAbilitiesTuning Broken = Tuning;
			FVeyraStanceAbilityTuning& Switch = Broken.Stance.FindChecked(ArchetypeTestId(TEXT("test_switch")));
			Switch.Slots.Add(StanceFixture::Held(EVeyraAbilitySlot::Q, TEXT("test_veil_w_again")));
			Switch.Slots.Add(StanceFixture::Held(EVeyraAbilitySlot::Item1, TEXT("test_switch")));
			const TArray<FString> Problems = VeyraAbilityRules::Validate(Broken, { 5, 3, 1 });
			const FString All = FString::Join(Problems, TEXT(" | "));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/stance/test_switch/slots/2/slot: is already held")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/stance/test_switch/slots/3/slot: must be Q, W, E or R")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/stance/test_switch/slots/3/ability")), All));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
