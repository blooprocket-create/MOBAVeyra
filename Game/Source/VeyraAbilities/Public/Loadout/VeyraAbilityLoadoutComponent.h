// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Content/VeyraContentId.h"
#include "GameplayAbilitySpecHandle.h"
#include "VeyraAbilityTypes.h"

#include "VeyraAbilityLoadoutComponent.generated.h"

class UAbilitySystemComponent;

/** One slot of a loadout: the ability's content ID, and on the server its granted spec. */
USTRUCT()
struct FVeyraLoadoutEntry
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraAbilitySlot Slot = EVeyraAbilitySlot::Q;

	UPROPERTY()
	FVeyraContentId Ability;

	/** Server only. */
	UPROPERTY(NotReplicated)
	FGameplayAbilitySpecHandle Handle;
};

/**
 * Which ability sits in each of a combatant's slots. It lives beside the Ability System Component
 * (a Vanguard's PlayerState), grants each ability from its content ID and tells an ability which
 * content it is running. The owner receives the slots for its HUD.
 */
UCLASS(ClassGroup = Abilities)
class VEYRAABILITIES_API UVeyraAbilityLoadoutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVeyraAbilityLoadoutComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Server only: grants Ability in Slot, replacing whatever was there. Its archetype comes from the
	 * Abilities tuning. Returns false if no archetype defines Ability.
	 */
	bool Grant(UAbilitySystemComponent& AbilitySystem, EVeyraAbilitySlot Slot, const FVeyraContentId& Ability);

	const FVeyraLoadoutEntry* FindSlot(EVeyraAbilitySlot Slot) const;
	const FVeyraLoadoutEntry* FindHandle(FGameplayAbilitySpecHandle Handle) const;

private:
	UPROPERTY(Replicated)
	TArray<FVeyraLoadoutEntry> Entries;
};
