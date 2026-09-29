// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "Teams/VeyraTeam.h"
#include "Units/VeyraUnit.h"

#include "VeyraTestFluxborn.generated.h"

class UAbilitySystemComponent;
class UVeyraAttributionComponent;
class UVeyraDamageAbsorptionComponent;
class UVeyraDefenceSet;
class UVeyraLifeComponent;
class UVeyraMobilitySet;
class UVeyraMovementComponent;
class UVeyraOffenceSet;
class UVeyraResourceSet;
class UVeyraStatusComponent;
class UVeyraVitalsSet;

// A unit that is not a Vanguard, for the tests until minions exist: it owns its Ability System
// Component and the combat components a unit needs, and the server moves it without a controller.
// UHT forbids preprocessor guards around UCLASSes, so this header is unconditional.
UCLASS(NotBlueprintable, NotPlaceable, Transient)
class AVeyraTestFluxborn : public ACharacter, public IAbilitySystemInterface, public IVeyraTeamMember, public IVeyraUnit
{
	GENERATED_BODY()

public:
	AVeyraTestFluxborn(const FObjectInitializer& ObjectInitializer);

	virtual void PostInitializeComponents() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystem; }
	virtual EVeyraTeam GetVeyraTeam() const override { return Team; }
	virtual EVeyraUnitKind GetVeyraUnitKind() const override { return EVeyraUnitKind::Fluxborn; }

	void SetVeyraTeam(EVeyraTeam NewTeam);
	UVeyraMovementComponent* GetVeyraMovement() const;

private:
	UPROPERTY(Replicated)
	EVeyraTeam Team = EVeyraTeam::None;

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

// A structure for the tests until structures exist: the test unit, reporting itself a structure, so
// Combat Bible §33's rules apply to it.
UCLASS(NotBlueprintable, NotPlaceable, Transient)
class AVeyraTestStructure : public AVeyraTestFluxborn
{
	GENERATED_BODY()

public:
	AVeyraTestStructure(const FObjectInitializer& ObjectInitializer);

	virtual EVeyraUnitKind GetVeyraUnitKind() const override { return EVeyraUnitKind::Structure; }
};
