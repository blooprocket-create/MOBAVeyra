// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Abilities/VeyraGameplayAbility.h"
#include "Delivery/VeyraAreaDelivery.h"
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
 * or with a dash that leaves it; as it ends, its vehicle goes on without its rider, and its crash
 * zones erupt where its rider is, on every end but death (ADR-035 §3). Each ability of this kind is an
 * entry in Abilities.json's ride map.
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

	/** Its body strikes the enemies and helps the allies it meets, each once a ride (ADR-035 §6). */
	void PulseContact();

	/** It lays its trail's area each time its rider has come its spacing (ADR-035 §6). */
	void PulseTrail();

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
	/** Its time ran out mid-dash: the dash ends first, and the ride with it if the dash has not ended it. */
	FDelegateHandle ExpiryDashHandle;
	FTimerHandle ExpiryTimer;
	/** Its crash, prepared at Commit from the rider's rank and power (Combat Bible §50). */
	TArray<FVeyraPreparedZone> CrashZones;

	/** Its contact's effects, prepared at Commit, and the units it has met this ride. */
	TOptional<FVeyraPreparedEffects> ContactEffects;
	TOptional<FVeyraPreparedAllyEffects> ContactHelp;
	TArray<TWeakObjectPtr<AActor>> Met;
	FTimerHandle ContactTimer;

	/** Its trail: where its rider was at the last look, and how far it has come since its last area. */
	FVector TrailFrom = FVector::ZeroVector;
	double TrailTravelled = 0.0;
	FTimerHandle TrailTimer;
};
