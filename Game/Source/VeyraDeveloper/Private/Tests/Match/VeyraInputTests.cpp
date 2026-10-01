// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Input/VeyraCameraSettings.h"
#include "Input/VeyraInputSettings.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "UObject/Package.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraMatchTests
{
	// Veyra.Match.Input.*: the runtime Enhanced Input objects follow the Veyra input settings
	// (Settings Bible §1.1), and the shipped defaults stay inside the server's order limit.
	TEST_CLASS(Input, "Veyra.Match")
	{
		static FKey KeyFor(const UInputMappingContext& Context, const UInputAction* Action)
		{
			const FEnhancedActionKeyMapping* Mapping = Context.GetMappings().FindByPredicate(
				[Action](const FEnhancedActionKeyMapping& Candidate) { return Candidate.Action == Action; });
			return Mapping ? Mapping->Key : EKeys::Invalid;
		}

		TEST_METHOD(EachBindingUsesItsSetting)
		{
			// Keys other than the shipped defaults, so the settings are what the objects follow.
			UVeyraInputSettings* Settings = NewObject<UVeyraInputSettings>(GetTransientPackage(), NAME_None, RF_Transient);
			Settings->MoveOrderKey = EKeys::LeftMouseButton;
			Settings->AttackMoveKey = EKeys::X;
			Settings->AbilityQKey = EKeys::One;
			Settings->AbilityWKey = EKeys::Two;
			Settings->AbilityEKey = EKeys::Three;
			Settings->AbilityRKey = EKeys::Four;
			Settings->Item1Key = EKeys::Z;
			Settings->Item2Key = EKeys::C;
			Settings->Item3Key = EKeys::V;
			Settings->Item4Key = EKeys::F1;
			Settings->Item5Key = EKeys::F2;
			Settings->Item6Key = EKeys::F3;
			Settings->Spell1Key = EKeys::F5;
			Settings->Spell2Key = EKeys::F6;
			Settings->RecallKey = EKeys::F4;
			Settings->VisionToolKey = EKeys::F7;
			Settings->VoteYesKey = EKeys::F8;
			Settings->VoteNoKey = EKeys::F9;

			const FVeyraInputObjects Objects = VeyraInput::Build(*Settings, *GetTransientPackage());
			ASSERT_THAT(IsNotNull(Objects.MappingContext.Get()));
			// Move, attack-move, Recall, the vision tool and the two vote answers, then one per ability slot,
			// item slot and Flux Spell slot.
			constexpr int32 OrderBindings = 6;
			ASSERT_THAT(AreEqual(Objects.MappingContext->GetMappings().Num(),
				OrderBindings + static_cast<int32>(UE_ARRAY_COUNT(VeyraAbilitySlots::All) + UE_ARRAY_COUNT(VeyraAbilitySlots::Items) + UE_ARRAY_COUNT(VeyraAbilitySlots::Spells))));
			ASSERT_THAT(IsTrue(KeyFor(*Objects.MappingContext, Objects.MoveOrder) == EKeys::LeftMouseButton));
			ASSERT_THAT(IsTrue(KeyFor(*Objects.MappingContext, Objects.AttackMove) == EKeys::X));
			ASSERT_THAT(IsTrue(KeyFor(*Objects.MappingContext, Objects.Recall) == EKeys::F4));
			ASSERT_THAT(IsTrue(KeyFor(*Objects.MappingContext, Objects.VisionTool) == EKeys::F7));
			ASSERT_THAT(IsTrue(KeyFor(*Objects.MappingContext, Objects.VoteYes) == EKeys::F8 && KeyFor(*Objects.MappingContext, Objects.VoteNo) == EKeys::F9));
			ASSERT_THAT(IsTrue(Objects.GetAbilityAction(EVeyraAbilitySlot::VisionTool) == Objects.VisionTool));
			for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::All)
			{
				ASSERT_THAT(IsTrue(KeyFor(*Objects.MappingContext, Objects.GetAbilityAction(Slot)) == Settings->GetAbilityKey(Slot)));
			}
			for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::Items)
			{
				ASSERT_THAT(IsTrue(KeyFor(*Objects.MappingContext, Objects.GetAbilityAction(Slot)) == Settings->GetAbilityKey(Slot)));
			}
			for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::Spells)
			{
				ASSERT_THAT(IsTrue(KeyFor(*Objects.MappingContext, Objects.GetAbilityAction(Slot)) == Settings->GetAbilityKey(Slot)));
			}
		}

		TEST_METHOD(AnAbilitysKeyReportsItsRelease)
		{
			// No press-only trigger, so the action completes when the key comes up (ADR-040 §1).
			const FVeyraInputObjects Objects = VeyraInput::Build(*GetDefault<UVeyraInputSettings>(), *GetTransientPackage());
			for (const EVeyraAbilitySlot Slot : { EVeyraAbilitySlot::Q, EVeyraAbilitySlot::R, EVeyraAbilitySlot::Item1, EVeyraAbilitySlot::Spell2, EVeyraAbilitySlot::VisionTool })
			{
				const UInputAction* Action = Objects.GetAbilityAction(Slot);
				ASSERT_THAT(IsTrue(Action && Action->Triggers.IsEmpty()));
			}
			ASSERT_THAT(IsFalse(Objects.Recall->Triggers.IsEmpty(), TEXT("an order still fires once per press")));
			ASSERT_THAT(IsTrue(GetDefault<UVeyraInputSettings>()->SelectKey.IsValid()));
		}

		TEST_METHOD(BuildingTwiceUnderOneOuterGivesSeparateObjects)
		{
			const UVeyraInputSettings& Settings = *GetDefault<UVeyraInputSettings>();
			UObject* Outer = NewObject<UPackage>(GetTransientPackage(), NAME_None, RF_Transient);
			const FVeyraInputObjects First = VeyraInput::Build(Settings, *Outer);
			const FVeyraInputObjects Second = VeyraInput::Build(Settings, *Outer);
			ASSERT_THAT(IsTrue(First.MappingContext != Second.MappingContext));
			ASSERT_THAT(IsTrue(First.MoveOrder != Second.MoveOrder));
			ASSERT_THAT(IsTrue(First.AbilityQ != Second.AbilityQ));
		}

		TEST_METHOD(TheShippedDefaultsAreUsable)
		{
			const UVeyraInputSettings& Settings = *GetDefault<UVeyraInputSettings>();
			TArray<FKey> Keys = { Settings.MoveOrderKey, Settings.AttackMoveKey, Settings.RecallKey, Settings.RankUpModifierKey, Settings.ShowCastRangeKey, Settings.VisionToolKey };
			for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::All)
			{
				Keys.Add(Settings.GetAbilityKey(Slot));
			}
			for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::Items)
			{
				Keys.Add(Settings.GetAbilityKey(Slot));
			}
			for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::Spells)
			{
				Keys.Add(Settings.GetAbilityKey(Slot));
			}
			for (int32 Index = 0; Index < Keys.Num(); ++Index)
			{
				ASSERT_THAT(IsTrue(Keys[Index].IsValid()));
				ASSERT_THAT(IsFalse(Keys.Find(Keys[Index]) != Index, TEXT("two actions share a default key")));
			}
			// The items around the vision tool's 4 (ADR-016 §6).
			ASSERT_THAT(IsTrue(Settings.VisionToolKey == EKeys::Four && Settings.Item4Key == EKeys::Five && Settings.Item6Key == EKeys::Seven));
			ASSERT_THAT(IsTrue(Settings.HeldMoveOrderIntervalSeconds > 0.0f));
			// A held move order repeats no faster than the server accepts orders.
			const double HeldOrdersPerSecond = 1.0 / Settings.HeldMoveOrderIntervalSeconds;
			ASSERT_THAT(IsTrue(HeldOrdersPerSecond <= UVeyraMatchTuningSubsystem::Get().Orders.MaxPerSecond));
		}
	};

	// Veyra.Match.Camera.*: the shipped default view looks down at the Vanguard from a distance.
	TEST_CLASS(Camera, "Veyra.Match")
	{
		TEST_METHOD(TheShippedDefaultViewIsUsable)
		{
			const UVeyraCameraSettings& View = *GetDefault<UVeyraCameraSettings>();
			ASSERT_THAT(IsTrue(View.Distance > 0.0f));
			// Straight down or level would lose the top-down view.
			constexpr float StraightDownDegrees = -90.0f;
			ASSERT_THAT(IsTrue(View.PitchDegrees > StraightDownDegrees && View.PitchDegrees < 0.0f));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
