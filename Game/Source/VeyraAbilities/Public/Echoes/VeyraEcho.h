// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "Content/VeyraContentId.h"
#include "Entities/VeyraOwnedUnit.h"
#include "GameFramework/Character.h"
#include "Teams/VeyraTeam.h"
#include "Units/VeyraUnit.h"

#include "VeyraEcho.generated.h"

class APlayerState;
class UAbilitySystemComponent;
class UVeyraAttributionComponent;
class UVeyraBasicAttackComponent;
class UVeyraDamageAbsorptionComponent;
class UVeyraDefenceSet;
class UVeyraLifeComponent;
class UVeyraMobilitySet;
class UVeyraOffenceSet;
class UVeyraResourceSet;
class UVeyraStatusComponent;
class UVeyraVitalsSet;

/**
 * A Vanguard's Echo (Item Bible §9, §11; ADR-050 §2): a projection of its holder on its holder's side, whose damage
 * is a share of its holder's and whose kills are its holder's (Combat Bible §32), so it sets off none of its holder's
 * Attunements. Its Health is sealed: it is the Echo's Integrity, which only UVeyraEchoSubsystem sets. Like a
 * companion it walks on the Veyra movement component and owns its Ability System Component (Minimal replication)
 * and the combat components a unit needs; nothing moves it unless Match hands it a controller (ADR-050 §6).
 */
UCLASS(NotBlueprintable, NotPlaceable)
class VEYRAABILITIES_API AVeyraEcho : public ACharacter, public IAbilitySystemInterface, public IVeyraTeamMember, public IVeyraUnit, public IVeyraOwnedUnit
{
	GENERATED_BODY()

public:
	AVeyraEcho(const FObjectInitializer& ObjectInitializer);

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystem; }
	virtual EVeyraTeam GetVeyraTeam() const override { return Team; }
	virtual EVeyraUnitKind GetVeyraUnitKind() const override { return EVeyraUnitKind::Echo; }
	virtual UAbilitySystemComponent* GetOwnerAbilities() const override { return HolderAbilities.Get(); }

	/**
	 * Server, before FinishSpawning: its holder, a participant's Ability System Component, whose side it takes; the
	 * ability that formed it; and where its holder stood as it was cast, the anchor of its tether.
	 */
	void Configure(UAbilitySystemComponent& InHolder, const FVeyraContentId& InAbility, const FVector& InAnchor);

	/**
	 * Server, once spawned: it takes its holder's offence as it stands now, dealing DamageCoefficient of its holder's
	 * outgoing damage (ADR-050 §2), its holder's basic attack and Move Speed, and a sealed Health of Integrity, or of its
	 * holder's Max Health when it has no Integrity to lose. False if refused.
	 */
	bool TakeHolderSnapshot(double DamageCoefficient, TOptional<double> Integrity);

	/** Server: its offence is its holder's as it stands now, at the same coefficient, as it repeats an ability. */
	bool RefreshOffence();

	/** Server: its Integrity, as its sealed Health, and the tether radius that Integrity allows. */
	void SetIntegrity(double Integrity, double InRadius);

	/** Server: it ends, withdrawn from the battleground without a death and out of sight (ADR-050 §2). */
	void Withdraw();

	/** Whether it has ended, on every machine. */
	bool IsWithdrawn() const { return bWithdrawn; }

	/** The ability that formed it. */
	const FVeyraContentId& GetAbility() const { return Ability; }

	/** Its holder's PlayerState, on every machine; null until it replicates. */
	APlayerState* GetHolderState() const { return HolderState; }

	/** Where its holder stood as it was cast, on every machine. */
	const FVector& GetAnchor() const { return Anchor; }

	/** Its tether radius now; 0 for an Echo without a tether. */
	double GetRadius() const { return Radius; }

private:
	UFUNCTION()
	void OnRep_Withdrawn();

	void ApplyWithdrawn();

	UPROPERTY(VisibleAnywhere, Category = "Veyra|Echo")
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

	UPROPERTY(Replicated)
	EVeyraTeam Team = EVeyraTeam::None;

	UPROPERTY(Replicated)
	TObjectPtr<APlayerState> HolderState;

	UPROPERTY(Replicated)
	FVeyraContentId Ability;

	UPROPERTY(Replicated)
	FVector Anchor = FVector::ZeroVector;

	UPROPERTY(Replicated)
	double Radius = 0.0;

	UPROPERTY(ReplicatedUsing = OnRep_Withdrawn)
	bool bWithdrawn = false;

	/** Server: the holder whose projection it is. */
	TWeakObjectPtr<UAbilitySystemComponent> HolderAbilities;

	/** Server: the share of its holder's outgoing damage it deals. */
	double DamageCoefficient = 0.0;
};
