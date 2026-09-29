// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Pawn.h"
#include "Teams/VeyraTeam.h"
#include "Units/VeyraUnit.h"

#include "VeyraFluxWell.generated.h"

class UAbilitySystemComponent;
class UCapsuleComponent;
class UVeyraAttributionComponent;
class UVeyraDamageAbsorptionComponent;
class UVeyraDefenceSet;
class UVeyraLifeComponent;
class UVeyraOffenceSet;
class UVeyraStatusComponent;
class UVeyraVitalsSet;

/** Where a Flux Well stands in its cycle (Battleground Bible §6). */
UENUM()
enum class EVeyraFluxWellState : uint8
{
	/** Before its opening time: it cannot be damaged or drained. */
	Closed,
	/** Open: damage and presence drain it, and its last hit secures it. */
	Open,
	/** Secured, waiting out its cycle before it opens again. */
	Respawning,
};

/**
 * A Flux Well, North or South (Battleground Bible §6; ADR-014 §4): a neutral objective, on no side
 * and not a structure, so Vanguard attacks and abilities both drain it and both sides can take it. It
 * is a pawn with its own Ability System Component and a body that blocks movement; it neither moves
 * nor attacks. UVeyraFluxWellSubsystem spawns it, opens and closes it, and drains it by presence.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class VEYRAWORLD_API AVeyraFluxWell : public APawn, public IAbilitySystemInterface, public IVeyraTeamMember, public IVeyraUnit
{
	GENERATED_BODY()

public:
	AVeyraFluxWell(const FObjectInitializer& ObjectInitializer);

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystem; }
	virtual EVeyraTeam GetVeyraTeam() const override { return EVeyraTeam::None; }
	virtual EVeyraUnitKind GetVeyraUnitKind() const override { return EVeyraUnitKind::Objective; }

	/** Server, before FinishSpawning: which of World.json's sites it stands at. */
	void Configure(int32 InSite);

	/** Server, once spawned: its Health and resistances from World.json. Returns false if refused. */
	bool InitializeStats();

	int32 GetSite() const { return Site; }
	EVeyraFluxWellState GetState() const { return State; }

	/** When it next opens, in the server's world time; 0 while open. */
	double GetOpensAt() const { return OpensAt; }

	/** Server: enters State, opening at OpensAt; closed and respawning, it cannot be damaged. */
	void SetState(EVeyraFluxWellState NewState, double NewOpensAt);

	/** Whether its Health stands above 0. */
	bool IsStanding() const;

private:
	void ApplyBody();

	UPROPERTY(Replicated)
	int32 Site = INDEX_NONE;

	UPROPERTY(Replicated)
	EVeyraFluxWellState State = EVeyraFluxWellState::Closed;

	UPROPERTY(Replicated)
	double OpensAt = 0.0;

	/** Server: whether it holds invulnerability now. */
	bool bInvulnerable = false;

	UPROPERTY()
	TObjectPtr<UCapsuleComponent> Capsule;

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
};
