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
			Settings->AbilityQKey = EKeys::One;
			Settings->AbilityWKey = EKeys::Two;
			Settings->AbilityEKey = EKeys::Three;
			Settings->AbilityRKey = EKeys::Four;

			const FVeyraInputObjects Objects = VeyraInput::Build(*Settings, *GetTransientPackage());
			ASSERT_THAT(IsNotNull(Objects.MappingContext.Get()));
			ASSERT_THAT(AreEqual(Objects.MappingContext->GetMappings().Num(), 1 + static_cast<int32>(UE_ARRAY_COUNT(VeyraAbilitySlots::All))));
			ASSERT_THAT(IsTrue(KeyFor(*Objects.MappingContext, Objects.MoveOrder) == EKeys::LeftMouseButton));
			for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::All)
			{
				ASSERT_THAT(IsTrue(KeyFor(*Objects.MappingContext, Objects.GetAbilityAction(Slot)) == Settings->GetAbilityKey(Slot)));
			}
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
			TArray<FKey> Keys = { Settings.MoveOrderKey };
			for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::All)
			{
				Keys.Add(Settings.GetAbilityKey(Slot));
			}
			for (int32 Index = 0; Index < Keys.Num(); ++Index)
			{
				ASSERT_THAT(IsTrue(Keys[Index].IsValid()));
				ASSERT_THAT(IsFalse(Keys.Find(Keys[Index]) != Index, TEXT("two actions share a default key")));
			}
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
