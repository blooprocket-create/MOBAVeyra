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
	// Move repeats while held; the controller paces it.
	Objects.MoveOrder = NewInputObject<UInputAction>(Outer, TEXT("VeyraMoveOrder"));
	Objects.MoveOrder->ValueType = EInputActionValueType::Boolean;
	Objects.AttackMove = NewCastAction(Outer, TEXT("VeyraAttackMove"));
	Objects.AbilityQ = NewCastAction(Outer, TEXT("VeyraAbilityQ"));
	Objects.AbilityW = NewCastAction(Outer, TEXT("VeyraAbilityW"));
	Objects.AbilityE = NewCastAction(Outer, TEXT("VeyraAbilityE"));
	Objects.AbilityR = NewCastAction(Outer, TEXT("VeyraAbilityR"));
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(VeyraAbilitySlots::Items)); ++Index)
	{
		Objects.ItemSlots.Add(NewCastAction(Outer, TEXT("VeyraItemSlot")));
	}
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(VeyraAbilitySlots::Spells)); ++Index)
	{
		Objects.SpellSlots.Add(NewCastAction(Outer, TEXT("VeyraSpellSlot")));
	}
	Objects.VisionTool = NewCastAction(Outer, TEXT("VeyraVisionTool"));
	Objects.Recall = NewCastAction(Outer, TEXT("VeyraRecall"));
	Objects.VoteYes = NewCastAction(Outer, TEXT("VeyraVoteYes"));
	Objects.VoteNo = NewCastAction(Outer, TEXT("VeyraVoteNo"));
	Objects.MappingContext = MapKeys(Settings, Objects, Outer);
	return Objects;
}

UInputMappingContext* MapKeys(const UVeyraInputSettings& Settings, const FVeyraInputObjects& Actions, UObject& Outer)
{
	UInputMappingContext* Context = NewInputObject<UInputMappingContext>(Outer, TEXT("VeyraMappingContext"));
	const auto Map = [Context](const UInputAction* Action, const FKey& Key) {
		// A binding the player cleared maps nothing.
		if (Action && Key.IsValid())
		{
			Context->MapKey(Action, Key);
		}
	};
	Map(Actions.MoveOrder, Settings.MoveOrderKey);
	Map(Actions.AttackMove, Settings.AttackMoveKey);
	for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::All)
	{
		Map(Actions.GetAbilityAction(Slot), Settings.GetAbilityKey(Slot));
	}
	for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::Items)
	{
		Map(Actions.GetAbilityAction(Slot), Settings.GetAbilityKey(Slot));
	}
	for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::Spells)
	{
		Map(Actions.GetAbilityAction(Slot), Settings.GetAbilityKey(Slot));
	}
	Map(Actions.VisionTool, Settings.VisionToolKey);
	Map(Actions.Recall, Settings.RecallKey);
	Map(Actions.VoteYes, Settings.VoteYesKey);
	Map(Actions.VoteNo, Settings.VoteNoKey);
	return Context;
}
}
