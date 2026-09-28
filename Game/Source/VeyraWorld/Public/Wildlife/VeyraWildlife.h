// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "Content/VeyraContentId.h"
#include "GameFramework/Character.h"
#include "Teams/VeyraTeam.h"
#include "Units/VeyraUnit.h"

#include "VeyraWildlife.generated.h"

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
struct FVeyraWildlifeSpecies;

/**
 * A creature of a jungle camp (Battleground Bible §8; ADR-014 §2): native fauna of a species whose
 * stats, body and basic attack come from World.json. It is neutral, on no side, so Vanguards of both
 * sides fight it and it fights them back, while lane Fluxborn and towers leave it alone (ADR-014 §1).
 * Like a Fluxborn it walks on CharacterMovement through UVeyraMovementComponent, owns its Ability
 * System Component (Minimal replication) and the combat components a unit needs, and attacks with the
 * basic attack component Vanguards use. A server-only AVeyraWildlifeController keeps it to its camp.
 * UVeyraJungleSubsystem spawns it.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class VEYRAWORLD_API AVeyraWildlife : public ACharacter, public IAbilitySystemInterface, public IVeyraTeamMember, public IVeyraUnit
{
	GENERATED_BODY()

public:
	AVeyraWildlife(const FObjectInitializer& ObjectInitializer);

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystem; }
	virtual EVeyraTeam GetVeyraTeam() const override { return EVeyraTeam::None; }
	virtual EVeyraUnitKind GetVeyraUnitKind() const override { return EVeyraUnitKind::Wildlife; }

	/** Server, before FinishSpawning: its species, its camp, and the spot it keeps to and the leash around it. */
	void Configure(const FVeyraContentId& InSpecies, int32 InCamp, const FVector& InHome, double InLeashRadius);

	/** Server, once spawned: its species' stats and basic attack from World.json. Returns false if refused. */
	bool InitializeStats();

	const FVeyraContentId& GetSpecies() const { return Species; }
	int32 GetCamp() const { return Camp; }

	/** Server: the spot it stands at, and fights no farther from than its leash. */
	const FVector& GetHome() const { return Home; }
	double GetLeashRadius() const { return LeashRadius; }

	/** Its species' definition in World.json, or null for an unknown species. */
	const FVeyraWildlifeSpecies* GetDefinition() const;

	bool IsAlive() const;
	UVeyraMovementComponent* GetVeyraMovement() const;
	UVeyraBasicAttackComponent* GetBasicAttack() const { return BasicAttack; }

private:
	/** Shapes the capsule from its species, on every machine once the species is known. */
	void ApplyBody();

	UFUNCTION()
	void OnRep_Species();

	UPROPERTY(ReplicatedUsing = OnRep_Species)
	FVeyraContentId Species;

	/** Server only. */
	int32 Camp = INDEX_NONE;
	FVector Home = FVector::ZeroVector;
	double LeashRadius = 0.0;

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
