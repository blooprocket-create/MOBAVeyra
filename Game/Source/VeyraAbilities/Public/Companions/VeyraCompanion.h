// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "Content/VeyraContentId.h"
#include "Entities/VeyraOwnedUnit.h"
#include "GameFramework/Character.h"
#include "Teams/VeyraTeam.h"
#include "Units/VeyraUnit.h"

#include "VeyraCompanion.generated.h"

class APlayerState;
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
struct FVeyraCompanionTuning;

/** What a companion is doing (ADR-034 §4). */
UENUM()
enum class EVeyraCompanionMode : uint8
{
	/** Keeping near its owner, fighting what its owner fights. */
	Follow,
	/** Standing at a point an ability sent it to, fighting what comes near. */
	Hold,
	/** Summoned to keep near an ally it helps now and then, fighting nothing (ADR-035 §5). */
	Escort,
	/** Summoned to hunt one enemy while it stays within its leash of its owner (ADR-035 §5). */
	Hunt,
	/**
	 * Deployed at a point (ADR-037 §1): it never walks, fights the enemies within its basic attack's reach without
	 * chasing, and stands whatever its owner's distance.
	 */
	Anchored,
};

/**
 * A Vanguard's companion (ADR-003's combat entity; ADR-034 §3), as Nix: an owned unit with its own Health,
 * basic attack and behaviour, on its owner's side, whose damage and kills are its owner's (Combat Bible
 * §32). Like a Fluxborn it walks on the Veyra movement component, so crowd control and forced moves work
 * on it, and owns its Ability System Component (Minimal replication) and the combat components a unit
 * needs. It inherits only what its definition declares. UVeyraCompanionSubsystem forms, banishes and
 * reforms it; a server-only AVeyraCompanionController moves and fights with it.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class VEYRAABILITIES_API AVeyraCompanion : public ACharacter, public IAbilitySystemInterface, public IVeyraTeamMember, public IVeyraUnit, public IVeyraOwnedUnit
{
	GENERATED_BODY()

public:
	AVeyraCompanion(const FObjectInitializer& ObjectInitializer);

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystem; }
	virtual EVeyraTeam GetVeyraTeam() const override { return Team; }
	virtual EVeyraUnitKind GetVeyraUnitKind() const override { return EVeyraUnitKind::Companion; }
	virtual UAbilitySystemComponent* GetOwnerAbilities() const override { return OwnerAbilities.Get(); }

	/**
	 * The body a companion of Owner stands beside and follows: Owner's avatar once it is a pawn. A
	 * participant's abilities answer for its PlayerState until its Vanguard spawns; that has no body.
	 */
	static APawn* BodyOf(const UAbilitySystemComponent& Owner);

	/** Server, before FinishSpawning: its definition and its owner, a participant's Ability System Component, whose side it takes. */
	void Configure(const FVeyraContentId& InDefinition, UAbilitySystemComponent& InOwner);

	/** Server, once spawned: its stats at its owner's Level and its basic attack. Returns false if refused. */
	bool InitializeStats(int32 OwnerLevel);

	/** Server: its stats grow to its owner's Level, never back; it keeps what Health it lacks. */
	bool GrowTo(int32 OwnerLevel);

	/** Server: what it holds of its owner's Magic Power, by its share (ADR-034 §3). */
	bool Inherit(double OwnerMagicPower);

	/** Server: it leaves the battleground, dead, hidden and without collision, until Reform. */
	void Banish();

	/** Server: it comes back at Where, alive at full Health, following its owner. */
	bool Reform(const FVector& Where);

	/** Whether it is banished, on every machine. */
	bool IsBanished() const { return bBanished; }

	/** Server: it goes to Where and holds there until HoldsUntil, world time; OpenedBy is the ability that sent it, whose follow-up the hold's end closes. */
	void HoldAt(const FVector& Where, double HoldsUntil, const FVeyraContentId& OpenedBy);

	/** Server: it gives up its hold and follows its owner again; the follow-up of the ability that sent it closes. */
	void EndHold();

	/** Server: a summoned companion escorts the ally or hunts the enemy Unit, as Mode says (ADR-035 §5). */
	void Bind(EVeyraCompanionMode InMode, AActor& Unit);

	/**
	 * Server: it stands deployed at Where, facing Facing, from now on (ADR-037 §1): it goes there at once, keeping its
	 * Health, and gives up whatever it did before.
	 */
	void Anchor(const FVector& Where, const FVector& Facing);

	/** Server: the way it faced as it was last anchored, sent moving, or turned by a posture change. */
	const FVector& GetAnchorFacing() const { return AnchorFacing; }

	/** Server: the posture it stands in, by its definition's order; 0 for a companion with none (ADR-037 §2). */
	int32 GetPosture() const { return Posture; }

	/**
	 * Server: it takes its next posture, the first after the last, its statuses replacing the last one's; standing,
	 * it turns to Facing if that names a way. False for a companion with fewer than two postures.
	 */
	bool NextPosture(const FVector& Facing);

	/** Server: whether its posture holds its fire (ADR-037 §2). Moving, it fires. */
	bool HoldsFire() const;

	/**
	 * Server: it walks with Ally, facing Facing whichever way it walks, holding its moving statuses instead of its
	 * posture's (ADR-037 §3), until Anchor sets it down again.
	 */
	void StartMoving(AActor& Ally, const FVector& Facing);

	/** Server: whether an order moves it now. */
	bool IsMoving() const { return bMoving; }

	/** Server: it gives itself any of the statuses it holds now that it lacks: its moving ones, or its posture's. */
	void KeepHeldStatuses();

	/** Server: it turns to face its anchor facing. */
	void FaceAnchor();

	/** Server: the ally it escorts or the enemy it hunts, if it was summoned for one. */
	AActor* GetBoundTo() const { return BoundTo.Get(); }

	EVeyraCompanionMode GetMode() const { return Mode; }
	const FVector& GetHoldPoint() const { return HoldPoint; }
	double GetHoldsUntil() const { return HoldsUntil; }

	/** Server: whether presentation draws a chain to its owner (ADR-034 §7). */
	void SetChained(bool bInChained);

	/** Whether a chain joins it to its owner, on every machine. */
	bool IsChained() const { return bChained; }

	/** Its owner's PlayerState, on every machine, so presentation knows whose it is. */
	APlayerState* GetOwnerState() const { return OwnerState; }

	const FVeyraContentId& GetDefinitionId() const { return Definition; }

	/** Its definition in Abilities.json, or null for an unknown one. */
	const FVeyraCompanionTuning* GetDefinition() const;

	/** The owner Level its stats have grown to. */
	int32 GetGrownLevel() const { return GrownLevel; }

	bool IsAlive() const;
	UVeyraMovementComponent* GetVeyraMovement() const;
	UVeyraBasicAttackComponent* GetBasicAttack() const { return BasicAttack; }

private:
	/** Shapes the capsule from its definition, on every machine once the definition is known. */
	void ApplyBody();

	/** Hides it and its collision while banished, on every machine. */
	void ApplyBanished();

	/** The status IDs it holds now: its moving ones, or its posture's. */
	TConstArrayView<FVeyraContentId> HeldStatuses() const;

	/** Server: it gives up the statuses it holds now, those it gave itself. */
	void DropHeldStatuses();

	UFUNCTION()
	void OnRep_Definition();

	UFUNCTION()
	void OnRep_Banished();

	UPROPERTY(Replicated)
	EVeyraTeam Team = EVeyraTeam::None;

	UPROPERTY(ReplicatedUsing = OnRep_Definition)
	FVeyraContentId Definition;

	UPROPERTY(Replicated)
	TObjectPtr<APlayerState> OwnerState;

	UPROPERTY(ReplicatedUsing = OnRep_Banished)
	bool bBanished = false;

	UPROPERTY(Replicated)
	bool bChained = false;

	/** Server only. */
	TWeakObjectPtr<UAbilitySystemComponent> OwnerAbilities;
	EVeyraCompanionMode Mode = EVeyraCompanionMode::Follow;
	TWeakObjectPtr<AActor> BoundTo;
	/** Where it holds, or stands anchored. */
	FVector HoldPoint = FVector::ZeroVector;
	FVector AnchorFacing = FVector::ForwardVector;
	int32 Posture = 0;
	bool bMoving = false;
	double HoldsUntil = 0.0;
	FVeyraContentId HoldOpenedBy;
	int32 GrownLevel = 1;
	/** What it holds of its owner's Magic Power, once it holds any. */
	TOptional<double> InheritedMagicPower;

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
