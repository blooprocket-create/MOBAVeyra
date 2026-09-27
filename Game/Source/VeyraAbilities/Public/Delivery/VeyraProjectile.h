// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "GameFramework/Actor.h"
#include "Teams/VeyraTeam.h"
#include "Templates/Function.h"
#include "Tuning/VeyraAbilitiesTuning.h"

#include "VeyraProjectile.generated.h"

class UAbilitySystemComponent;

/** How a projectile finds what it hits (ADR-009 §4). */
UENUM()
enum class EVeyraProjectileFlight : uint8
{
	/** Straight along its direction, until its range, terrain, or a unit it stops at ends it. */
	Line,
	/** After one unit, which it reaches unless that unit dies or leaves first; terrain does not stop it (ADR-008 §9). */
	Homing,
};

/**
 * A projectile in flight (ADR-009 §4). It has no Ability System Component of its own: it carries its
 * caster's effects, prepared at Commit (Combat Bible §50), and only the server moves it, tick by tick,
 * so a pause holds it. Clients receive its launch data only, and presentation draws its path from the
 * server's clock, which a pause also stops. It still hits if its caster has died (§9, Guaranteed
 * Resolution).
 */
UCLASS(NotPlaceable)
class VEYRAABILITIES_API AVeyraProjectile : public AActor, public IVeyraTeamMember
{
	GENERATED_BODY()

public:
	AVeyraProjectile();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual EVeyraTeam GetVeyraTeam() const override { return Team; }

	/**
	 * Server only: sends it from the actor's location along Direction, stopped as Collision says.
	 * Effects land on each unit it hits; PassThroughEffects on each unit a FirstEnemyVanguard
	 * projectile passes through. Called once, after spawning.
	 */
	void LaunchLine(UAbilitySystemComponent& Caster, const FVector& Direction, const FVeyraProjectileTuning& Tuning, EVeyraSkillshotCollision Collision,
		FVeyraPreparedEffects Effects, FVeyraPreparedEffects PassThroughEffects, const FVeyraContentId& Ability, int32 CastId);

	/**
	 * Server only: sends it from the actor's location after Target, at Speed. When it lands, Effects
	 * apply to the target and then OnLanded runs, as a basic attack resolves its hit. Called once,
	 * after spawning.
	 */
	void LaunchHoming(UAbilitySystemComponent& Caster, AActor& Target, double Speed, double Radius, FVeyraPreparedEffects Effects,
		const FVeyraContentId& Ability, int32 CastId, TFunction<void(AActor&)> OnLanded = nullptr);

	/**
	 * Server only: flies Seconds further, hitting what it meets on the way, and is destroyed once it
	 * ends. Its tick calls this; tests call it directly.
	 */
	void AdvanceBy(double Seconds);

	/**
	 * Where a line projectile is at ServerTime, in the server's world time, from its launch data alone,
	 * for presentation on any machine; a homing one's launch point. It may already have ended sooner.
	 */
	FVector GetLineLocationAt(double ServerTime) const;

	EVeyraProjectileFlight GetFlight() const { return Flight; }
	const FVector& GetLaunchedFrom() const { return LaunchedFrom; }
	double GetLaunchedAt() const { return LaunchedAt; }
	const FVector& GetDirection() const { return Direction; }
	double GetSpeed() const { return Speed; }
	double GetRadius() const { return Radius; }
	double GetRange() const { return Range; }
	AActor* GetHomingTarget() const { return HomingTarget; }
	const FVeyraContentId& GetAbility() const { return Ability; }
	int32 GetCastId() const { return CastId; }

private:
	void Launch(UAbilitySystemComponent& InCaster, const FVeyraContentId& InAbility, int32 InCastId);
	void AdvanceLine(UAbilitySystemComponent& Source, double Distance);
	void AdvanceHoming(UAbilitySystemComponent& Source, double Distance);

	/** Where displacements are measured from: the caster while it lives, the launch point after. */
	FVeyraEffectFrame CasterFrame() const;

	/** Its path, for pushing units out of it. */
	FVeyraEffectFrame PathFrame() const;

	void End();

	UPROPERTY(Replicated)
	EVeyraProjectileFlight Flight = EVeyraProjectileFlight::Line;

	UPROPERTY(Replicated)
	FVector LaunchedFrom = FVector::ZeroVector;

	/** In the server's world time. */
	UPROPERTY(Replicated)
	double LaunchedAt = 0.0;

	/** On the ground. A homing projectile's is where it set off toward. */
	UPROPERTY(Replicated)
	FVector Direction = FVector::ForwardVector;

	/** Units per second. */
	UPROPERTY(Replicated)
	double Speed = 0.0;

	UPROPERTY(Replicated)
	double Radius = 0.0;

	/** How far a line projectile flies; 0 for a homing one, which flies until it lands. */
	UPROPERTY(Replicated)
	double Range = 0.0;

	UPROPERTY(Replicated)
	TObjectPtr<AActor> HomingTarget;

	UPROPERTY(Replicated)
	EVeyraTeam Team = EVeyraTeam::None;

	UPROPERTY(Replicated)
	FVeyraContentId Ability;

	UPROPERTY(Replicated)
	int32 CastId = 0;

	/** Server only. */
	TWeakObjectPtr<UAbilitySystemComponent> Caster;
	FVeyraPreparedEffects Effects;
	FVeyraPreparedEffects PassThroughEffects;
	TFunction<void(AActor&)> OnLanded;
	EVeyraSkillshotCollision Collision = EVeyraSkillshotCollision::FirstEnemy;
	double Travelled = 0.0;

	/** Units it has hit or passed through, which it never meets again. */
	TArray<TWeakObjectPtr<AActor>> Met;
};
