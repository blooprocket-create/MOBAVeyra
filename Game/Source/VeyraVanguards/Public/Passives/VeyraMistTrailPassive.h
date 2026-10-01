// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Engine/TimerHandle.h"
#include "Passives/VeyraPassive.h"

#include "VeyraMistTrailPassive.generated.h"

struct FVeyraMistTrailTuning;

/**
 * A guide's trail into the fog (ADR-036 §5), as Sylra's Follow the Bell. It looks where its owner stands each
 * look. As she steps from no fog into Dense Fog, it lays its area every spacing along the last stretch of the
 * way she came, and on along her path for a while, as hers. Its area gives allies its follow mark; an allied
 * Vanguard holding the mark from her in the fog volume she entered gains her follow shield, once per trail. It
 * grants no vision. Its data is an entry in Vanguards.json's mistTrail map.
 */
UCLASS()
class VEYRAVANGUARDS_API UVeyraMistTrailPassive : public UVeyraPassive
{
	GENERATED_BODY()

public:
	virtual void Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId) override;
	virtual void Stop() override;

	/** Looks where its owner stands; its world timer calls it each look, and tests may. */
	void Look();

	/** How many trails it has left. */
	int32 GetTrailCount() const { return TrailCount; }

private:
	/** Lays its area at Where, facing Facing, as its owner's. */
	void Lay(UAbilitySystemComponent& Owner, const FVeyraMistTrailTuning& Tuning, const FVector& Where, const FVector& Facing);

	/** Lays its area along the way its owner came, back from Here, every spacing up to the approach's length. */
	void LayApproach(UAbilitySystemComponent& Owner, const FVeyraMistTrailTuning& Tuning, const FVector& Here);

	/** Shields each allied Vanguard holding its mark in the fog its owner entered, once per trail. */
	void ShieldFollowers(UAbilitySystemComponent& Owner, const FVeyraMistTrailTuning& Tuning);

	FTimerHandle LookTimer;
	/** Where its owner's body has been lately, oldest first, at most the approach's length and a spacing back. */
	TArray<FVector> Approach;
	bool bWasInFog = false;
	/** Its trail goes on with its owner until then, in world time. */
	double LayingUntil = 0.0;
	/** How far its owner has gone since its last area, and where it was at the last look. */
	double Travelled = 0.0;
	FVector LastSeen = FVector::ZeroVector;
	/** Where the current trail entered its fog, which names that fog's volume; unset before the first. */
	TOptional<FVector> EnteredAt;
	/** The allies the current trail has shielded. */
	TSet<TWeakObjectPtr<const UAbilitySystemComponent>> Shielded;
	int32 TrailCount = 0;
};
