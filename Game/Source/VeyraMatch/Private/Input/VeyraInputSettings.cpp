// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Input/VeyraInputSettings.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"

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
}

FVeyraInputObjects Build(const UVeyraInputSettings& Settings, UObject& Outer)
{
	FVeyraInputObjects Objects;
	Objects.MappingContext = NewInputObject<UInputMappingContext>(Outer, TEXT("VeyraMappingContext"));

	// Move repeats while held; the controller paces it.
	Objects.MoveOrder = NewInputObject<UInputAction>(Outer, TEXT("VeyraMoveOrder"));
	Objects.MoveOrder->ValueType = EInputActionValueType::Boolean;

	Objects.AbilityQ = NewInputObject<UInputAction>(Outer, TEXT("VeyraAbilityQ"));
	Objects.AbilityQ->ValueType = EInputActionValueType::Boolean;
	Objects.AbilityQ->Triggers.Add(NewObject<UInputTriggerPressed>(Objects.AbilityQ));

	Objects.MappingContext->MapKey(Objects.MoveOrder, Settings.MoveOrderKey);
	Objects.MappingContext->MapKey(Objects.AbilityQ, Settings.AbilityQKey);
	return Objects;
}
}
