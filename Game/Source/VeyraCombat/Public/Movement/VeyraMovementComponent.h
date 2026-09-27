// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Delegates/IDelegateInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/WeakObjectPtr.h"

#include "VeyraMovementComponent.generated.h"

class UAbilitySystemComponent;
class UVeyraStatusComponent;

/**
 * A combatant body's movement (ADR-009 §2). On the server it walks at the effective Movement Speed
 * of the combatant it follows (VeyraMovementRules::EffectiveSpeed), and reports when something
 * other than the unit's own orders owns its movement, so its controller can hold them. Clients only
 * show the replicated result (ADR-006 §7).
 */
UCLASS(ClassGroup = Combat)
class VEYRACOMBAT_API UVeyraMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	/** Server only: follows Combatant's speed and statuses; nullptr lets go. */
	void BindCombatant(UAbilitySystemComponent* Combatant);

	virtual float GetMaxSpeed() const override;

	/** Whether the unit cannot follow its orders now: it is stunned. */
	bool IsMovementLocked() const { return bMovementLocked; }

	/** Server only: raised when IsMovementLocked changes, with its new value. */
	TMulticastDelegate<void(bool)> OnMovementLockChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void RefreshMovementLock();

	TWeakObjectPtr<UAbilitySystemComponent> FollowedCombatant;
	TWeakObjectPtr<UVeyraStatusComponent> FollowedStatuses;
	FDelegateHandle StatusesChangedHandle;
	bool bMovementLocked = false;
};
