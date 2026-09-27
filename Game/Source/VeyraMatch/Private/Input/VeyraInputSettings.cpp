// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Input/VeyraInputSettings.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"

const FKey& UVeyraInputSettings::GetAbilityKey(EVeyraAbilitySlot Slot) const
{
	switch (Slot)
	{
	case EVeyraAbilitySlot::Q:
		return AbilityQKey;
	case EVeyraAbilitySlot::W:
		return AbilityWKey;
	case EVeyraAbilitySlot::E:
		return AbilityEKey;
	case EVeyraAbilitySlot::R:
		return AbilityRKey;
	}
	return EKeys::Invalid;
}

UInputAction* FVeyraInputObjects::GetAbilityAction(EVeyraAbilitySlot Slot) const
{
	switch (Slot)
	{
	case EVeyraAbilitySlot::Q:
		return AbilityQ;
	case EVeyraAbilitySlot::W:
		return AbilityW;
	case EVeyraAbilitySlot::E:
		return AbilityE;
	case EVeyraAbilitySlot::R:
		return AbilityR;
	}
	return nullptr;
}

namespace VeyraInput
{
namespace
{
	// Named for debugging, and unique so building twice under one outer never collides.
	template <typename ObjectType>
	ObjectType* NewInputObject(UObject& Outer, const TCHAR* BaseName)
	{
		return NewObject<ObjectType>(&Outer, MakeUniqueObjectName(&Outer, ObjectType::StaticClass(), BaseName), RF_Transient);
	}

	/** A cast action, which fires once per press. */
	UInputAction* NewCastAction(UObject& Outer, const TCHAR* BaseName)
	{
		UInputAction* Action = NewInputObject<UInputAction>(Outer, BaseName);
		Action->ValueType = EInputActionValueType::Boolean;
		Action->Triggers.Add(NewObject<UInputTriggerPressed>(Action));
		return Action;
	}
}

FVeyraInputObjects Build(const UVeyraInputSettings& Settings, UObject& Outer)
{
	FVeyraInputObjects Objects;
	Objects.MappingContext = NewInputObject<UInputMappingContext>(Outer, TEXT("VeyraMappingContext"));

	// Move repeats while held; the controller paces it.
	Objects.MoveOrder = NewInputObject<UInputAction>(Outer, TEXT("VeyraMoveOrder"));
	Objects.MoveOrder->ValueType = EInputActionValueType::Boolean;
	Objects.MappingContext->MapKey(Objects.MoveOrder, Settings.MoveOrderKey);

	Objects.AbilityQ = NewCastAction(Outer, TEXT("VeyraAbilityQ"));
	Objects.AbilityW = NewCastAction(Outer, TEXT("VeyraAbilityW"));
	Objects.AbilityE = NewCastAction(Outer, TEXT("VeyraAbilityE"));
	Objects.AbilityR = NewCastAction(Outer, TEXT("VeyraAbilityR"));
	for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::All)
	{
		Objects.MappingContext->MapKey(Objects.GetAbilityAction(Slot), Settings.GetAbilityKey(Slot));
	}
	return Objects;
}
}
