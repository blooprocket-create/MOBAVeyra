// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Delivery/VeyraAreaDelivery.h"
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

/** What a lingering area does to the enemy units inside it beside its statuses, prepared at Commit (ADR-026 §4). */
struct FVeyraLingerEffects
{
	/** Dealt at each pulse after it lands; empty when its pulses give statuses only. */
	TArray<FVeyraPreparedZone> Pulse;

	/** Dealt as it ends, measured from its centre; empty when its end does nothing. */
	TArray<FVeyraPreparedZone> End;

	/** How long before its end the presentation marks it. */
	double EndWarningSeconds = 0.0;

	/** The cast they belong to, for the hits' announcements. */
	int32 CastId = 0;
};

/**
 * A delivered area that lasts (ADR-018 §5), the smallest of ADR-003's world volumes. As it lands and
 * at every pulse after, it gives the units inside it their side's statuses: its caster, the allied
 * Vanguards, the enemies. Each pulse after it lands may also hit the enemies inside, and so may its
 * end, which every machine is warned of ahead (ADR-026 §4). The server pulses it on a world timer, so
 * a pause holds it; every machine sees its shape and when it ends. It lasts if its caster dies
 * (Combat Bible §9, Guaranteed Resolution).
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
	 * Statuses now and every PulseSeconds until DurationSeconds have passed, dealing Effects' pulse at
	 * each pulse after now and their end as it ends. Called once, after spawning.
	 */
	void Arm(UAbilitySystemComponent& Caster, const FVeyraEffectFrame& Placement, const FVeyraShape& Shape, FVeyraLingerStatuses Statuses,
		FVeyraLingerEffects Effects, double DurationSeconds, double PulseSeconds, const FVeyraContentId& Ability);

	FVeyraPlacedShape GetPlacedShape() const { return FVeyraPlacedShape{ Shape, GetActorLocation(), Direction }; }

	/** When it ends, in the server's world time. */
	double GetEndsAt() const { return EndsAt; }

	/** How long before its end the presentation marks it; 0 when its end does nothing (ADR-026 §4). */
	double GetEndWarningSeconds() const { return EndWarningSeconds; }

	/** Whether its end hits and is near at Now, in the server's world time, so the presentation marks it. */
	bool IsEndNear(double Now) const { return EndWarningSeconds > 0.0 && Now >= EndsAt - EndWarningSeconds; }

	/** Server only: whose area it is. */
	const UAbilitySystemComponent* GetCaster() const { return Caster.Get(); }

	/** Its caster's side, so Vision shows it to that side and to those who see it (ADR-016 §3). */
	virtual EVeyraTeam GetVeyraTeam() const override { return Team; }

	const FVeyraContentId& GetAbility() const { return Ability; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Gives the units inside it their side's statuses. */
	void GiveStatuses();

	/** A pulse after it lands: its statuses and its pulse effects. */
	void Pulse();

	/** Its end effects, then its end. */
	void End();

	/** Hits the enemies inside with Zones, measured from its centre: a pulse's as a tick, which Spell Shields let pass. */
	void Deal(TConstArrayView<FVeyraPreparedZone> Zones, bool bTick);

	UPROPERTY(Replicated)
	FVeyraShape Shape;

	UPROPERTY(Replicated)
	FVector Direction = FVector::ForwardVector;

	UPROPERTY(Replicated)
	double EndsAt = 0.0;

	UPROPERTY(Replicated)
	double EndWarningSeconds = 0.0;

	UPROPERTY(Replicated)
	EVeyraTeam Team = EVeyraTeam::None;

	UPROPERTY(Replicated)
	FVeyraContentId Ability;

	/** Server only. */
	TWeakObjectPtr<UAbilitySystemComponent> Caster;
	FVeyraLingerStatuses Statuses;
	FVeyraLingerEffects Effects;
	FTimerHandle PulseTimer;
	FTimerHandle EndTimer;
};
