// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"

#include "VeyraPassive.generated.h"

class UAbilitySystemComponent;

/**
 * A Vanguard's passive that the shared systems cannot express (ADR-008 §5). It belongs to the content
 * domain, and reacts only to the events and hooks the core systems publish; no core system names it.
 * Each passive archetype is a subclass whose data is a map in Vanguards.json. Server only.
 */
UCLASS(Abstract)
class VEYRAVANGUARDS_API UVeyraPassive : public UObject
{
	GENERATED_BODY()

public:
	/** Begins the passive for the participant whose Ability System Component is Owner, with the data PassiveId names. */
	virtual void Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId);

	/** Ends it, as when its participant leaves the match. */
	virtual void Stop();

	UAbilitySystemComponent* GetOwnerAbilitySystem() const { return OwnerAbilitySystem.Get(); }
	const FVeyraContentId& GetPassiveId() const { return PassiveId; }

	virtual UWorld* GetWorld() const override;

protected:
	TWeakObjectPtr<UAbilitySystemComponent> OwnerAbilitySystem;
	FVeyraContentId PassiveId;
};
