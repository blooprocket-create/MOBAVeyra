// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Delivery/VeyraAreaDelivery.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/Actor.h"
#include "Shapes/VeyraShapes.h"
#include "Teams/VeyraTeam.h"

#include "VeyraDelayedArea.generated.h"

class UAbilitySystemComponent;

/**
 * An area that hits after a telegraphed delay (ADR-009 §4). The server keeps its effects, prepared at
 * Commit (Combat Bible §50), and hits on a world timer, so a pause holds it; every machine sees its
 * shapes, placement and moment for the telegraph. It still hits if its caster has died (§9,
 * Guaranteed Resolution).
 */
UCLASS(NotPlaceable)
class VEYRAABILITIES_API AVeyraDelayedArea : public AActor
{
	GENERATED_BODY()

public:
	AVeyraDelayedArea();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server only: arms the area, placed at the actor's location, to hit DelaySeconds from now. Called once, after spawning. */
	void Arm(UAbilitySystemComponent& Caster, const FVeyraEffectFrame& Placement, TArray<FVeyraPreparedZone> Zones, double DelaySeconds,
		const FVeyraContentId& Ability, int32 CastId);

	const TArray<FVeyraShape>& GetShapes() const { return Shapes; }
	const FVector& GetDirection() const { return Direction; }

	/** When it hits, in the server's world time. */
	double GetResolvesAt() const { return ResolvesAt; }

	EVeyraTeam GetVeyraTeam() const { return Team; }
	const FVeyraContentId& GetAbility() const { return Ability; }
	int32 GetCastId() const { return CastId; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Resolve();

	/** The zones' shapes, innermost first. */
	UPROPERTY(Replicated)
	TArray<FVeyraShape> Shapes;

	UPROPERTY(Replicated)
	FVector Direction = FVector::ForwardVector;

	UPROPERTY(Replicated)
	double ResolvesAt = 0.0;

	UPROPERTY(Replicated)
	EVeyraTeam Team = EVeyraTeam::None;

	UPROPERTY(Replicated)
	FVeyraContentId Ability;

	UPROPERTY(Replicated)
	int32 CastId = 0;

	/** Server only. */
	TWeakObjectPtr<UAbilitySystemComponent> Caster;
	TArray<FVeyraPreparedZone> Zones;
	bool bOriginIsCaster = false;
	FTimerHandle ResolveTimer;
};
