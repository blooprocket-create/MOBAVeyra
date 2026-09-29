// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Pawn.h"
#include "Teams/VeyraTeam.h"
#include "Units/VeyraUnit.h"

#include "VeyraWard.generated.h"

class APlayerState;
class UAbilitySystemComponent;
class UCapsuleComponent;
class UVeyraAttributionComponent;
class UVeyraDamageAbsorptionComponent;
class UVeyraDefenceSet;
class UVeyraLifeComponent;
class UVeyraStatusComponent;
class UVeyraVitalsSet;

/**
 * A placed ward (Vision Bible §4; ADR-016 §6): a marker on the battleground (ADR-003) that gives its
 * side sight, lasts its lifetime, and falls to a number of basic attacks from enemy Vanguards. It is
 * a unit of the Ward kind, so Combat counts hits on it and every area passes it by. Vision places it,
 * keeps it from the enemy's clients unless True Sight covers it, and pays its destroyer.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class VEYRAVISION_API AVeyraWard : public APawn, public IAbilitySystemInterface, public IVeyraTeamMember, public IVeyraUnit
{
	GENERATED_BODY()

public:
	AVeyraWard(const FObjectInitializer& ObjectInitializer);

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystem; }
	virtual EVeyraTeam GetVeyraTeam() const override { return Team; }
	virtual EVeyraUnitKind GetVeyraUnitKind() const override { return EVeyraUnitKind::Ward; }

	/**
	 * Server, once, as it is placed: its side, the participant who placed it, and its Health, a
	 * point per hit it takes (Vision.json persistentWard). False if it could not take them.
	 */
	bool Place(EVeyraTeam InTeam, APlayerState& InPlacer);

	/** Server: the participant who placed it, while that participant is in the match. */
	APlayerState* GetPlacer() const { return Placer.Get(); }

	bool IsAlive() const;

	/** The collision channel a ward's body is on, which every other body ignores (ADR-016 §6). */
	static ECollisionChannel GetBodyChannel();

private:
	/** Sizes the body from Vision.json, on every machine. */
	void ApplyBody();

	UPROPERTY(Replicated)
	EVeyraTeam Team = EVeyraTeam::None;

	UPROPERTY()
	TObjectPtr<UCapsuleComponent> Body;

	/** Replicated in Minimal mode: a ward has no effects of its own to show. */
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY()
	TObjectPtr<UVeyraDamageAbsorptionComponent> DamageAbsorption;

	UPROPERTY()
	TObjectPtr<UVeyraStatusComponent> Statuses;

	UPROPERTY()
	TObjectPtr<UVeyraLifeComponent> Life;

	/** Who hit it, so its killing blow is credited to its destroyer (Vision Bible §8). */
	UPROPERTY()
	TObjectPtr<UVeyraAttributionComponent> Attribution;

	UPROPERTY()
	TObjectPtr<UVeyraVitalsSet> VitalsSet;

	/** Read by the damage execution; a ward's hits ignore it. */
	UPROPERTY()
	TObjectPtr<UVeyraDefenceSet> DefenceSet;

	TWeakObjectPtr<APlayerState> Placer;
};
