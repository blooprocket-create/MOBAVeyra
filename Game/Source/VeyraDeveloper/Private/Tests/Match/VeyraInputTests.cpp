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
			Settings->AbilityQKey = EKeys::W;

			const FVeyraInputObjects Objects = VeyraInput::Build(*Settings, *GetTransientPackage());
			ASSERT_THAT(IsNotNull(Objects.MappingContext.Get()));
			ASSERT_THAT(AreEqual(Objects.MappingContext->GetMappings().Num(), 2));
			ASSERT_THAT(IsTrue(KeyFor(*Objects.MappingContext, Objects.MoveOrder) == EKeys::LeftMouseButton));
			ASSERT_THAT(IsTrue(KeyFor(*Objects.MappingContext, Objects.AbilityQ) == EKeys::W));
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
			ASSERT_THAT(IsTrue(Settings.MoveOrderKey.IsValid()));
			ASSERT_THAT(IsTrue(Settings.AbilityQKey.IsValid()));
			ASSERT_THAT(IsTrue(Settings.MoveOrderKey != Settings.AbilityQKey));
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
