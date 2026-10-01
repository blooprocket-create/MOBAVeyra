// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "Entities/VeyraMarkerTypes.h"
#include "GameFramework/Pawn.h"
#include "Teams/VeyraTeam.h"
#include "TimerManager.h"
#include "Units/VeyraUnit.h"

#include "VeyraPlacedMarker.generated.h"

class APlayerState;
class UAbilitySystemComponent;
class UBoxComponent;
class UCapsuleComponent;
class UVeyraAttributionComponent;
class UVeyraDamageAbsorptionComponent;
class UVeyraDefenceSet;
class UVeyraLifeComponent;
class UVeyraStatusComponent;
class UVeyraVitalsSet;
struct FVeyraDeathEvent;

/**
 * A placed marker (ADR-003; ADR-030 §5): something a Vanguard's ability leaves on the battleground, with
 * no combat behaviour of its own, such as Tavi's illusion. It belongs to its owner and its owner's side;
 * what it causes is its owner's (Combat Bible §32), and destroying it pays nothing and is not a kill of
 * its owner. It inherits nothing. It stands for its lifetime and ends early when its owner dies, when
 * enemies destroy it (one point of Health a hit, as a ward), or when its owner's ability recalls it. A
 * marker that takes no hits is Untargetable. Vision gates it as a unit.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class VEYRACOMBAT_API AVeyraPlacedMarker : public APawn, public IAbilitySystemInterface, public IVeyraTeamMember, public IVeyraUnit
{
	GENERATED_BODY()

public:
	AVeyraPlacedMarker(const FObjectInitializer& ObjectInitializer);

	/**
	 * Server: places a marker of Spec for Owner, a participant's Ability System Component, at Where. Its
	 * side is set before it joins the world, so Vision gates it from the start. Null if it could not.
	 */
	static AVeyraPlacedMarker* Place(UWorld& World, UAbilitySystemComponent& Owner, const FVeyraMarkerSpec& Spec, const FTransform& Where);

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystem; }
	virtual EVeyraTeam GetVeyraTeam() const override { return Team; }
	virtual EVeyraUnitKind GetVeyraUnitKind() const override { return EVeyraUnitKind::Marker; }

	/** Server: ends it now, for Reason, and announces it. */
	void EndMarker(EVeyraMarkerEndReason Reason, UAbilitySystemComponent* Destroyer = nullptr);

	/** Server: Owner's marker of Id that still stands, if any; an owner has at most one of each. */
	static AVeyraPlacedMarker* FindStanding(const UAbilitySystemComponent& Owner, const FVeyraContentId& Id);

	/** Server: moves it to Where on every machine, as its owner's swap does (ADR-031 §5). */
	void Relocate(const FVector& Where);

	/** Server: its owner's Ability System Component. */
	UAbilitySystemComponent* GetOwnerAbilities() const { return OwnerAbilities.Get(); }

	const FVeyraContentId& GetMarkerId() const { return Spec.Id; }

	/** Whether enemies may target and hit it. */
	bool IsTargetable() const { return bTargetable; }

	/** The participant it presents itself as to enemies, or null: its owner's PlayerState, on every machine. */
	APlayerState* GetPresentedAs() const { return PresentedAs; }

	/** Whether it is a wall (ADR-032 §4), on every machine. */
	bool IsWall() const { return WallSize.X > 0.0f && WallSize.Y > 0.0f; }

	/** A wall's length, across the way it faces, and its thickness; zero for a marker that is no wall. */
	FVector2f GetWallSize() const { return WallSize; }

private:
	/** Server, as it is placed: its Health, its lifetime and its watch on deaths. */
	void Start();

	/** Sizes its body from BodySize, on every machine. */
	UFUNCTION()
	void ApplyBody();

	/** Puts it where Spot says, on a client. */
	UFUNCTION()
	void ApplySpot();

	/** A wall's body blocks as terrain does, sized from WallSize, on every machine (ADR-032 §4). */
	UFUNCTION()
	void ApplyWall();

	void OnDeath(const FVeyraDeathEvent& Death);

	UPROPERTY(Replicated)
	EVeyraTeam Team = EVeyraTeam::None;

	UPROPERTY(Replicated)
	bool bTargetable = false;

	UPROPERTY(Replicated)
	TObjectPtr<APlayerState> PresentedAs;

	/** Its body's size, its owner's when it presents as its owner. */
	UPROPERTY(ReplicatedUsing = ApplyBody)
	FVector2f BodySize = FVector2f::ZeroVector;

	/** Where it stands: it does not move but by Relocate, so this replicates instead of its movement. */
	UPROPERTY(ReplicatedUsing = ApplySpot)
	FVector_NetQuantize Spot = FVector::ZeroVector;

	/** A wall's length and thickness, so every machine's movement meets it; zero for no wall. */
	UPROPERTY(ReplicatedUsing = ApplyWall)
	FVector2f WallSize = FVector2f::ZeroVector;

	UPROPERTY()
	TObjectPtr<UCapsuleComponent> Body;

	/** A wall's body: it blocks nothing until ApplyWall shapes it. */
	UPROPERTY()
	TObjectPtr<UBoxComponent> WallBody;

	/** Replicated in Minimal mode: a marker has no effects of its own to show. */
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY()
	TObjectPtr<UVeyraDamageAbsorptionComponent> DamageAbsorption;

	UPROPERTY()
	TObjectPtr<UVeyraStatusComponent> Statuses;

	UPROPERTY()
	TObjectPtr<UVeyraLifeComponent> Life;

	/** Who hit it, so its end names its destroyer. */
	UPROPERTY()
	TObjectPtr<UVeyraAttributionComponent> Attribution;

	UPROPERTY()
	TObjectPtr<UVeyraVitalsSet> VitalsSet;

	/** Read by the damage execution; a marker's hits ignore it. */
	UPROPERTY()
	TObjectPtr<UVeyraDefenceSet> DefenceSet;

	TWeakObjectPtr<UAbilitySystemComponent> OwnerAbilities;
	FVeyraMarkerSpec Spec;
	FTimerHandle LifetimeTimer;
	FDelegateHandle DeathHandle;
	bool bEnded = false;
};
