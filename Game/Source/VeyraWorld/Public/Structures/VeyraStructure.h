// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "Battleground/VeyraBattlegroundTypes.h"
#include "GameFramework/Pawn.h"
#include "Misc/Optional.h"
#include "Teams/VeyraTeam.h"
#include "Units/VeyraUnit.h"

#include "VeyraStructure.generated.h"

class UAbilitySystemComponent;
class UCapsuleComponent;
class UVeyraAttributionComponent;
class UVeyraDamageAbsorptionComponent;
class UVeyraDefenceSet;
class UVeyraLifeComponent;
class UVeyraOffenceSet;
class UVeyraStatusComponent;
class UVeyraVitalsSet;
struct FVeyraStructurePlacement;
struct FVeyraStructureTuning;

/**
 * A lane Spire, base-defense tower, inhibitor or Prime Well (Battleground Bible §5, §10, §18; ADR-011
 * §4). It is a pawn, so unit gathering, the grey-box bodies and the HUD bars include it; it owns its
 * Ability System Component (Minimal replication, ADR-006 §4) and the combat components a unit needs,
 * and Combat §33's structure rules apply to it. UVeyraBattlegroundSubsystem spawns it on the server
 * from the layout and runs its rules; it never moves.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class VEYRAWORLD_API AVeyraStructure : public APawn, public IAbilitySystemInterface, public IVeyraTeamMember, public IVeyraUnit
{
	GENERATED_BODY()

public:
	AVeyraStructure(const FObjectInitializer& ObjectInitializer);

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystem; }
	virtual EVeyraTeam GetVeyraTeam() const override { return Team; }
	virtual EVeyraUnitKind GetVeyraUnitKind() const override { return EVeyraUnitKind::Structure; }

	/** Server, before FinishSpawning: what the structure is, whose, and its place in its lane's order. */
	void Configure(const FVeyraStructurePlacement& Placement);

	/** Server, once spawned: its kind's stats from World.json, at full Health. Returns false if refused. */
	bool InitializeStats();

	EVeyraStructureKind GetStructureKind() const { return Kind; }
	TOptional<EVeyraLane> GetLane() const;

	/** The order its lane's structures fall in, from 0 for the outer Spire; the index among its kind in the base. */
	int32 GetOrder() const { return Order; }

	/** Whether its death is final: destroyed, and not rebuilt yet. */
	bool IsDestroyed() const;

	/** Whether its prerequisites leave it invulnerable now (Battleground Bible §18), as every machine sees it. */
	bool IsInvulnerable() const { return bInvulnerable; }

	/** Server: makes it invulnerable or not, holding one invulnerability grant while it is. */
	void SetInvulnerable(bool bNewInvulnerable);

	/** When a destroyed inhibitor rebuilds, in the server's world time; 0 when none is due. */
	double GetRebuildsAt() const { return RebuildsAt; }

	/** Server: when it will rebuild; 0 when it will not. */
	void SetRebuildsAt(double At);

	/** Server: back at full Health, as a reconstructed inhibitor (Battleground Bible §10). */
	bool Rebuild();

	/** Its stats, from World.json by its kind. */
	const FVeyraStructureTuning& GetTuning() const;

private:
	/** Shapes the capsule from its kind's tuning, on every machine once the kind is known. */
	void ApplyBody();

	UPROPERTY(Replicated)
	EVeyraTeam Team = EVeyraTeam::None;

	UPROPERTY(Replicated)
	EVeyraStructureKind Kind = EVeyraStructureKind::LaneSpire;

	UPROPERTY(Replicated)
	bool bHasLane = false;

	UPROPERTY(Replicated)
	EVeyraLane Lane = EVeyraLane::Mid;

	UPROPERTY(Replicated)
	int32 Order = 0;

	UPROPERTY(Replicated)
	bool bInvulnerable = false;

	UPROPERTY(Replicated)
	double RebuildsAt = 0.0;

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
