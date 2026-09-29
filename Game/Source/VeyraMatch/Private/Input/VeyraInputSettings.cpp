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
	case EVeyraAbilitySlot::Item1:
		return Item1Key;
	case EVeyraAbilitySlot::Item2:
		return Item2Key;
	case EVeyraAbilitySlot::Item3:
		return Item3Key;
	case EVeyraAbilitySlot::Item4:
		return Item4Key;
	case EVeyraAbilitySlot::Item5:
		return Item5Key;
	case EVeyraAbilitySlot::Item6:
		return Item6Key;
	case EVeyraAbilitySlot::Spell1:
		return Spell1Key;
	case EVeyraAbilitySlot::Spell2:
		return Spell2Key;
	case EVeyraAbilitySlot::VisionTool:
		return VisionToolKey;
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
	case EVeyraAbilitySlot::VisionTool:
		return VisionTool;
	default:
		break;
	}
	if (VeyraAbilitySlots::IsSpellSlot(Slot))
	{
		const int32 SpellIndex = VeyraAbilitySlots::SpellIndexOf(Slot);
		return SpellSlots.IsValidIndex(SpellIndex) ? SpellSlots[SpellIndex].Get() : nullptr;
	}
	const int32 Index = VeyraAbilitySlots::ItemIndexOf(Slot);
	return ItemSlots.IsValidIndex(Index) ? ItemSlots[Index].Get() : nullptr;
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

	Objects.AttackMove = NewCastAction(Outer, TEXT("VeyraAttackMove"));
	Objects.MappingContext->MapKey(Objects.AttackMove, Settings.AttackMoveKey);

	Objects.AbilityQ = NewCastAction(Outer, TEXT("VeyraAbilityQ"));
	Objects.AbilityW = NewCastAction(Outer, TEXT("VeyraAbilityW"));
	Objects.AbilityE = NewCastAction(Outer, TEXT("VeyraAbilityE"));
	Objects.AbilityR = NewCastAction(Outer, TEXT("VeyraAbilityR"));
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(VeyraAbilitySlots::Items)); ++Index)
	{
		Objects.ItemSlots.Add(NewCastAction(Outer, TEXT("VeyraItemSlot")));
	}
	for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::All)
	{
		Objects.MappingContext->MapKey(Objects.GetAbilityAction(Slot), Settings.GetAbilityKey(Slot));
	}
	for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::Items)
	{
		Objects.MappingContext->MapKey(Objects.GetAbilityAction(Slot), Settings.GetAbilityKey(Slot));
	}
	for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::Spells)
	{
		Objects.SpellSlots.Add(NewCastAction(Outer, TEXT("VeyraSpellSlot")));
		Objects.MappingContext->MapKey(Objects.GetAbilityAction(Slot), Settings.GetAbilityKey(Slot));
	}
	Objects.VisionTool = NewCastAction(Outer, TEXT("VeyraVisionTool"));
	Objects.MappingContext->MapKey(Objects.VisionTool, Settings.VisionToolKey);
	Objects.Recall = NewCastAction(Outer, TEXT("VeyraRecall"));
	Objects.MappingContext->MapKey(Objects.Recall, Settings.RecallKey);
	Objects.VoteYes = NewCastAction(Outer, TEXT("VeyraVoteYes"));
	Objects.MappingContext->MapKey(Objects.VoteYes, Settings.VoteYesKey);
	Objects.VoteNo = NewCastAction(Outer, TEXT("VeyraVoteNo"));
	Objects.MappingContext->MapKey(Objects.VoteNo, Settings.VoteNoKey);
	return Objects;
}
}
