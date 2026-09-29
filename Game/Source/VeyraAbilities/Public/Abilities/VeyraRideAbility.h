// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Abilities/VeyraGameplayAbility.h"
#include "Engine/TimerHandle.h"

#include "VeyraRideAbility.generated.h"

class UVeyraMovementComponent;
struct FVeyraDeathEvent;
struct FVeyraRideEnd;

/**
 * The archetype for an ability that puts its caster in a ride state (Combat Bible §56; ADR-018 §7), as
 * Raska's Kickstart and NO BRAKES. The ride sets the caster's speed and limits its turns (Combat's
 * movement owns both); its mounted actions hold their slots and its statuses hold on the rider while
 * it lasts. It ends with its time, when a recast that fires at expiry fires, with the rider's death,
 * or with a dash that leaves it; as it ends, its vehicle goes on without its rider. Each ability of
 * this kind is an entry in Abilities.json's ride map.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraRideAbility : public UVeyraGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual bool Defines(const FVeyraContentId& Ability) const override;
	virtual double GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual double GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual EVeyraCastRejection CheckTarget(const AActor& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const override;
	virtual const FVeyraCastTuning* GetCastTuning(const FVeyraContentId& Ability) const override;
	virtual FVeyraChannelPlan Deliver(const FVeyraCast& Cast) override;
	virtual bool IsOffensive(const FVeyraContentId& Ability) const override;

private:
	/** Its time is up: a recast that fires at expiry fires now; otherwise the ride simply ends. */
	void Expire();
	void OnRideEnded(const FVeyraRideEnd& End);
	void OnDeath(const FVeyraDeathEvent& Death);

	/** Sends the vehicle on without its rider, from End's place along its heading (§56, "The separated vehicle"). */
	void LaunchVehicle(UAbilitySystemComponent& Rider, const FVeyraContentId& Vehicle, const FVeyraRideEnd& End) const;

	/** The group its mounted actions hold their slots under. */
	FName MountedGroup() const;

	void StopWatching();

	TWeakObjectPtr<UAbilitySystemComponent> Rider;
	FVeyraContentId RideAbility;
	int32 RideRank = 0;
	int32 RideCastId = 0;
	TWeakObjectPtr<UVeyraMovementComponent> Watched;
	FDelegateHandle RideEndedHandle;
	FDelegateHandle DeathHandle;
	FTimerHandle ExpiryTimer;
};
