// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/Actor.h"
#include "Shapes/VeyraShapes.h"
#include "Statuses/VeyraStatusTypes.h"
#include "Teams/VeyraTeam.h"

#include "VeyraLingeringArea.generated.h"

class UAbilitySystemComponent;

/** The statuses a lingering area gives each side inside it, from its caster's Level at Commit (ADR-018 §5). */
struct FVeyraLingerStatuses
{
	TArray<FVeyraStatusSpec> Caster;
	TArray<FVeyraStatusSpec> Allies;
	TArray<FVeyraStatusSpec> Enemies;
};

/**
 * A delivered area that lasts (ADR-018 §5), the smallest of ADR-003's world volumes. As it lands and
 * at every pulse after, it gives the units inside it their side's statuses: its caster, the allied
 * Vanguards, the enemies. The server pulses it on a world timer, so a pause holds it; every machine
 * sees its shape and when it ends. It lasts if its caster dies (Combat Bible §9, Guaranteed
 * Resolution).
 */
UCLASS(NotPlaceable)
class VEYRAABILITIES_API AVeyraLingeringArea : public AActor, public IVeyraTeamMember
{
	GENERATED_BODY()

public:
	AVeyraLingeringArea();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Server only: arms the area, at the actor's location facing Placement's direction, to give
	 * Statuses now and every PulseSeconds until DurationSeconds have passed. Called once, after spawning.
	 */
	void Arm(UAbilitySystemComponent& Caster, const FVeyraEffectFrame& Placement, const FVeyraShape& Shape, FVeyraLingerStatuses Statuses,
		double DurationSeconds, double PulseSeconds, const FVeyraContentId& Ability);

	FVeyraPlacedShape GetPlacedShape() const { return FVeyraPlacedShape{ Shape, GetActorLocation(), Direction }; }

	/** When it ends, in the server's world time. */
	double GetEndsAt() const { return EndsAt; }

	/** Its caster's side, so Vision shows it to that side and to those who see it (ADR-016 §3). */
	virtual EVeyraTeam GetVeyraTeam() const override { return Team; }

	const FVeyraContentId& GetAbility() const { return Ability; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Gives the units inside it their side's statuses. */
	void Pulse();

	UPROPERTY(Replicated)
	FVeyraShape Shape;

	UPROPERTY(Replicated)
	FVector Direction = FVector::ForwardVector;

	UPROPERTY(Replicated)
	double EndsAt = 0.0;

	UPROPERTY(Replicated)
	EVeyraTeam Team = EVeyraTeam::None;

	UPROPERTY(Replicated)
	FVeyraContentId Ability;

	/** Server only. */
	TWeakObjectPtr<UAbilitySystemComponent> Caster;
	FVeyraLingerStatuses Statuses;
	FTimerHandle PulseTimer;
	FTimerHandle EndTimer;
};
