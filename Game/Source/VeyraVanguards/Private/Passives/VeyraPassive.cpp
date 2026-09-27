// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Passives/VeyraPassive.h"

#include "AbilitySystemComponent.h"

void UVeyraPassive::Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId)
{
	OwnerAbilitySystem = &Owner;
	PassiveId = InPassiveId;
}

void UVeyraPassive::Stop()
{
	OwnerAbilitySystem.Reset();
}

UWorld* UVeyraPassive::GetWorld() const
{
	// The class default object has no world; an instance lives in its participant's.
	const UObject* Outer = GetOuter();
	return Outer && !HasAnyFlags(RF_ClassDefaultObject) ? Outer->GetWorld() : nullptr;
}
