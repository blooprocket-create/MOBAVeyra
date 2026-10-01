// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Input/VeyraCastInput.h"

FVeyraCastOutcome FVeyraCastInput::Press(EVeyraAbilitySlot Slot, EVeyraCastMode Mode, bool bPreview)
{
	if (bPreview)
	{
		// Show Cast Range shows the ability's indicator and never casts (Settings Bible §1.7).
		Indicator = FVeyraCastIndicator{ Slot, Mode, /*bPreviewOnly*/ true };
		return { EVeyraCastStep::Show, Slot };
	}
	if (Mode == EVeyraCastMode::Quick)
	{
		// Whatever showed gives way to the cast.
		Indicator.Reset();
		return { EVeyraCastStep::CastNow, Slot };
	}
	if (Indicator && Indicator->Slot == Slot && !Indicator->bPreviewOnly)
	{
		// A second press of the waiting ability's key keeps it waiting (ADR-040 §8.3).
		return {};
	}
	// Another ability's key moves the indicator to it, casting nothing.
	Indicator = FVeyraCastIndicator{ Slot, Mode, /*bPreviewOnly*/ false };
	return { EVeyraCastStep::Show, Slot };
}

FVeyraCastOutcome FVeyraCastInput::Release(EVeyraAbilitySlot Slot)
{
	if (Indicator && Indicator->Slot == Slot && !Indicator->bPreviewOnly && Indicator->Mode == EVeyraCastMode::QuickWithIndicator)
	{
		return Take(EVeyraCastStep::CastNow);
	}
	return {};
}

FVeyraCastOutcome FVeyraCastInput::Confirm()
{
	return Indicator && !Indicator->bPreviewOnly ? Take(EVeyraCastStep::CastNow) : FVeyraCastOutcome();
}

FVeyraCastOutcome FVeyraCastInput::Cancel()
{
	return Indicator ? Take(EVeyraCastStep::Hide) : FVeyraCastOutcome();
}

FVeyraCastOutcome FVeyraCastInput::EndPreview()
{
	return Indicator && Indicator->bPreviewOnly ? Take(EVeyraCastStep::Hide) : FVeyraCastOutcome();
}

FVeyraCastOutcome FVeyraCastInput::Take(EVeyraCastStep Step)
{
	const FVeyraCastOutcome Outcome{ Step, Indicator->Slot };
	Indicator.Reset();
	return Outcome;
}

namespace VeyraCastModes
{
FString NameOf(EVeyraCastMode Mode)
{
	switch (Mode)
	{
	case EVeyraCastMode::Quick:
		return TEXT("Quick");
	case EVeyraCastMode::QuickWithIndicator:
		return TEXT("QuickWithIndicator");
	case EVeyraCastMode::Normal:
		return TEXT("Normal");
	}
	return FString();
}

TOptional<EVeyraCastMode> Parse(const FString& Name)
{
	for (const EVeyraCastMode Mode : { EVeyraCastMode::Quick, EVeyraCastMode::QuickWithIndicator, EVeyraCastMode::Normal })
	{
		if (Name == NameOf(Mode))
		{
			return Mode;
		}
	}
	return {};
}
}
