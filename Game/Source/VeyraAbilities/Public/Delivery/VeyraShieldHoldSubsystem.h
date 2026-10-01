// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "Delivery/VeyraAreaDelivery.h"
#include "Events/VeyraAbilityEvents.h"
#include "Statuses/VeyraStatusTypes.h"
#include "Subsystems/WorldSubsystem.h"

#include "VeyraShieldHoldSubsystem.generated.h"

class UAbilitySystemComponent;
struct FGameplayEffectRemovalInfo;

/**
 * Shields that hold statuses and burst as they end (ADR-032 §3), as Tempered Shell: while a watched shield
 * holds, its holder keeps the statuses given with it; as Combat removes the shield, because it emptied or
 * its time ran out, the statuses go and the burst's zones land around the holder, its provider's. A new
 * grant of the same shield from the same provider takes over the old one's watch quietly, and a holder
 * that has died keeps no burst. Server only.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraShieldHoldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/**
	 * Quietly stops watching Provider's shield of Id on Holder, as a new grant of it is about to take its
	 * place: neither its statuses go nor its burst lands.
	 */
	void Release(const UAbilitySystemComponent& Provider, const UAbilitySystemComponent& Holder, const FVeyraContentId& Id);

	/**
	 * Gives Holder Statuses from Provider, and watches Shield, the grant of Id Provider has just given it:
	 * as Combat removes it, the statuses go and EndZones land around the holder for Source's cast.
	 */
	void Hold(UAbilitySystemComponent& Provider, UAbilitySystemComponent& Holder, const FActiveGameplayEffectHandle& Shield, const FVeyraContentId& Id,
		TConstArrayView<FVeyraStatusSpec> Statuses, TArray<FVeyraPreparedZone> EndZones, const FVeyraAbilityHitSource& Source);

	/** How many shields it watches. */
	int32 GetHoldCount() const { return Holds.Num(); }

private:
	void OnShieldRemoved(const FGameplayEffectRemovalInfo& Removal, int32 Key);

	struct FHold
	{
		int32 Key = 0;
		TWeakObjectPtr<UAbilitySystemComponent> Provider;
		TWeakObjectPtr<UAbilitySystemComponent> Holder;
		FActiveGameplayEffectHandle Shield;
		FVeyraContentId Id;
		TArray<FVeyraContentId> Statuses;
		TArray<FVeyraPreparedZone> EndZones;
		FVeyraAbilityHitSource Source;
		FDelegateHandle Removed;
	};

	/** Stops watching Hold's shield; its delegate lets go. */
	static void Unbind(const FHold& Hold);

	TArray<FHold> Holds;
	int32 NextKey = 1;
};
