// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "Battleground/VeyraBattlegroundTypes.h"
#include "Content/VeyraContentId.h"
#include "GameFramework/Character.h"
#include "Teams/VeyraTeam.h"
#include "Units/VeyraUnit.h"

#include "VeyraFluxborn.generated.h"

class UAbilitySystemComponent;
class UVeyraAttributionComponent;
class UVeyraBasicAttackComponent;
class UVeyraDamageAbsorptionComponent;
class UVeyraDefenceSet;
class UVeyraLifeComponent;
class UVeyraMobilitySet;
class UVeyraMovementComponent;
class UVeyraOffenceSet;
class UVeyraResourceSet;
class UVeyraStatusComponent;
class UVeyraVitalsSet;
struct FVeyraFluxbornDefinition;

/**
 * A lane Fluxborn (Battleground Bible §4; ADR-011 §7): a Strider, Spark or Breaker, whose kind's
 * stats, body and basic attack come from World.json. It walks on CharacterMovement through
 * UVeyraMovementComponent, so slows and displacement work on it; it owns its Ability System
 * Component (Minimal replication) and the combat components a unit needs; and it attacks with the
 * basic attack component Vanguards use. A server-only AVeyraFluxbornController moves it along its
 * lane. Team Flux strengthens its Max Health and damage live. UVeyraBattlegroundSubsystem spawns it.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class VEYRAWORLD_API AVeyraFluxborn : public ACharacter, public IAbilitySystemInterface, public IVeyraTeamMember, public IVeyraUnit
{
	GENERATED_BODY()

public:
	AVeyraFluxborn(const FObjectInitializer& ObjectInitializer);

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystem; }
	virtual EVeyraTeam GetVeyraTeam() const override { return Team; }
	virtual EVeyraUnitKind GetVeyraUnitKind() const override { return EVeyraUnitKind::Fluxborn; }

	/**
	 * Server, before FinishSpawning: its kind, side and lane, and the path it walks toward the enemy
	 * base, on the floor.
	 */
	void Configure(const FVeyraContentId& InKind, EVeyraTeam InTeam, EVeyraLane InLane, TArray<FVector2D> InWaypoints);

	/**
	 * Server, once spawned: its kind's stats and basic attack from World.json, strengthened by its
	 * team's Team Flux (the multipliers). Returns false if refused.
	 */
	bool InitializeStats(double HealthMultiplier, double DamageMultiplier);

	/** Server: its team's Team Flux changed; it keeps its Health's percentage (Battleground Bible §4). */
	bool ApplyStrength(double HealthMultiplier, double DamageMultiplier);

	const FVeyraContentId& GetKind() const { return Kind; }
	EVeyraLane GetLane() const { return Lane; }

	/** Its kind's definition in World.json, or null for an unknown kind. */
	const FVeyraFluxbornDefinition* GetDefinition() const;

	/** Server: the path it walks, from its own base toward the enemy's. */
	const TArray<FVector2D>& GetWaypoints() const { return Waypoints; }

	bool IsAlive() const;
	UVeyraMovementComponent* GetVeyraMovement() const;
	UVeyraBasicAttackComponent* GetBasicAttack() const { return BasicAttack; }

private:
	/** Shapes the capsule from its kind, on every machine once the kind is known. */
	void ApplyBody();

	UFUNCTION()
	void OnRep_Kind();

	UPROPERTY(Replicated)
	EVeyraTeam Team = EVeyraTeam::None;

	UPROPERTY(ReplicatedUsing = OnRep_Kind)
	FVeyraContentId Kind;

	UPROPERTY(Replicated)
	EVeyraLane Lane = EVeyraLane::Mid;

	/** Server only. */
	TArray<FVector2D> Waypoints;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY()
	TObjectPtr<UVeyraDamageAbsorptionComponent> DamageAbsorption;

	UPROPERTY()
	TObjectPtr<UVeyraStatusComponent> Statuses;

	UPROPERTY()
	TObjectPtr<UVeyraLifeComponent> Life;

	UPROPERTY()
	TObjectPtr<UVeyraAttributionComponent> Attribution;

	UPROPERTY()
	TObjectPtr<UVeyraBasicAttackComponent> BasicAttack;

	UPROPERTY()
	TObjectPtr<UVeyraVitalsSet> VitalsSet;

	UPROPERTY()
	TObjectPtr<UVeyraOffenceSet> OffenceSet;

	UPROPERTY()
	TObjectPtr<UVeyraDefenceSet> DefenceSet;

	UPROPERTY()
	TObjectPtr<UVeyraMobilitySet> MobilitySet;

	UPROPERTY()
	TObjectPtr<UVeyraResourceSet> ResourceSet;
};
