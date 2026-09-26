// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "Teams/VeyraTeam.h"

#include "VeyraPlayerState.generated.h"

class AVeyraVanguardController;
class UAbilitySystemComponent;
class UVeyraDamageAbsorptionComponent;
class UVeyraDefenceSet;
class UVeyraLifeComponent;
class UVeyraMobilitySet;
class UVeyraOffenceSet;
class UVeyraResourceSet;
class UVeyraVitalsSet;

/**
 * One participant's persistent match state. It owns the Vanguard's Ability System Component and
 * Attribute Sets, so cooldowns, effects and attribution survive death, respawn and reconnect; the
 * pawn is only the avatar (ADR-006 §4). AI-controlled Vanguards get one too.
 */
UCLASS()
class VEYRAMATCH_API AVeyraPlayerState : public APlayerState, public IAbilitySystemInterface, public IVeyraTeamMember
{
	GENERATED_BODY()

public:
	AVeyraPlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual EVeyraTeam GetVeyraTeam() const override { return Team; }
	virtual void PostInitializeComponents() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server only: the GameMode assigns each participant a side once. */
	void SetVeyraTeam(EVeyraTeam NewTeam);

	/** Server only: the controller that moves this participant's Vanguard. It outlives each pawn. */
	AVeyraVanguardController* GetVanguardController() const { return VanguardController; }
	void SetVanguardController(AVeyraVanguardController* Controller) { VanguardController = Controller; }

	/** Server only: whether the base stats have been set from data. They are set once per match. */
	bool HasInitializedStats() const { return bStatsInitialized; }
	void MarkStatsInitialized() { bStatsInitialized = true; }

protected:
	/**
	 * The engine destroys a departing player's PlayerState. Veyra keeps it: the Vanguard stays in the
	 * world with its Ability System Component (Match Flow Bible §4). Reconnecting to it arrives with
	 * session identity (M4).
	 */
	virtual void OnDeactivated() override;

private:
	/** Replicated in Mixed mode: full effect data to the owning client, the minimum to everyone else. */
	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UVeyraDamageAbsorptionComponent> DamageAbsorption;

	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UVeyraLifeComponent> Life;

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

	UPROPERTY(Transient)
	TObjectPtr<AVeyraVanguardController> VanguardController;

	bool bStatsInitialized = false;
};
