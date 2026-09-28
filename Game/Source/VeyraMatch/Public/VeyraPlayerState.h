// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "Content/VeyraContentId.h"
#include "GameFramework/PlayerState.h"
#include "Teams/VeyraTeam.h"
#include "Units/VeyraUnit.h"

#include "VeyraPlayerState.generated.h"

class AVeyraVanguardController;
class UAbilitySystemComponent;
class UVeyraAbilityLoadoutComponent;
class UVeyraAttributionComponent;
class UVeyraBasicAttackComponent;
class UVeyraCastStateComponent;
class UVeyraCombatStateComponent;
class UVeyraCooldownComponent;
class UVeyraDamageAbsorptionComponent;
class UVeyraDefenceSet;
class UVeyraLifeComponent;
class UVeyraMobilitySet;
class UVeyraOffenceSet;
class UVeyraGoldComponent;
class UVeyraInventoryComponent;
class UVeyraProgressionComponent;
class UVeyraRegenerationComponent;
class UVeyraResourceSet;
class UVeyraPassive;
class UVeyraStatusComponent;
class UVeyraVitalsSet;

/**
 * One participant's persistent match state. It owns the Vanguard's Ability System Component and
 * Attribute Sets, so cooldowns, effects and attribution survive death, respawn and reconnect; the
 * pawn is only the avatar (ADR-006 §4). AI-controlled Vanguards get one too.
 */
UCLASS()
class VEYRAMATCH_API AVeyraPlayerState : public APlayerState, public IAbilitySystemInterface, public IVeyraTeamMember, public IVeyraUnit
{
	GENERATED_BODY()

public:
	AVeyraPlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual EVeyraTeam GetVeyraTeam() const override { return Team; }
	virtual EVeyraUnitKind GetVeyraUnitKind() const override { return EVeyraUnitKind::Vanguard; }
	virtual void PostInitializeComponents() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server only: the GameMode assigns each participant a side once. */
	void SetVeyraTeam(EVeyraTeam NewTeam);

	/** Server only: the controller that moves this participant's Vanguard. It outlives each pawn. */
	AVeyraVanguardController* GetVanguardController() const { return VanguardController; }
	void SetVanguardController(AVeyraVanguardController* Controller) { VanguardController = Controller; }

	/** Server only: whether the participant has been prepared as its Vanguard. That happens once per match. */
	bool HasInitializedStats() const { return bStatsInitialized; }
	void MarkStatsInitialized() { bStatsInitialized = true; }

	/** The Vanguard this participant plays, on every machine, which gives its body its shape (ADR-008 §2). */
	const FVeyraContentId& GetVanguardId() const { return VanguardId; }

	/** Server only: set once, before the participant's first Vanguard spawns. */
	void SetVanguardId(const FVeyraContentId& InVanguardId);

	/** Server only: keeps the participant's started passive alive for the match. */
	void SetPassive(UVeyraPassive* InPassive) { Passive = InPassive; }
	UVeyraPassive* GetPassive() const { return Passive; }

	/**
	 * Server only, development builds: the Vanguard this participant asked to play with the
	 * -VeyraVanguard= option (ADR-008 §8). Invalid when it asked for none.
	 */
	const FVeyraContentId& GetRequestedVanguardId() const { return RequestedVanguardId; }
	void SetRequestedVanguardId(const FVeyraContentId& InVanguardId) { RequestedVanguardId = InVanguardId; }

	/**
	 * Server only: the backend account this participant joined as, from the match's roster
	 * (ADR-007). Empty for bots and on developer servers without an assignment. Not replicated.
	 */
	const FString& GetAccountId() const { return AccountId; }
	void SetAccountId(const FString& InAccountId) { AccountId = InAccountId; }

	/**
	 * When the participant's dead Vanguard returns, in the server's world time, on every machine, so
	 * the HUD can count down (Economy & Progression Bible §14). Meaningful only while it is dead.
	 */
	double GetRespawnAt() const { return RespawnAt; }

	/** Server only: set as the Vanguard dies. */
	void SetRespawnAt(double InRespawnAt);

protected:
	/**
	 * The engine destroys a departing player's PlayerState. Veyra keeps it: the Vanguard stays in the
	 * world with its Ability System Component (Match Flow Bible §4). A returning player's account is
	 * known from the roster (ADR-007); giving it this PlayerState back arrives with reconnect.
	 */
	virtual void OnDeactivated() override;

private:
	/** Replicated in Mixed mode: full effect data to the owning client, the minimum to everyone else. */
	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UVeyraDamageAbsorptionComponent> DamageAbsorption;

	/** Crowd control, buffs and debuffs. They end at death, unlike the participant's progression. */
	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UVeyraStatusComponent> Statuses;

	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UVeyraCombatStateComponent> CombatState;

	/** Who contributed toward this participant's death, for assists. Server only. */
	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UVeyraAttributionComponent> Attribution;

	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UVeyraLifeComponent> Life;

	UPROPERTY(VisibleAnywhere, Category = "Abilities")
	TObjectPtr<UVeyraAbilityLoadoutComponent> Loadout;

	/** Here rather than on the pawn, so cooldowns keep running through death (Combat Bible §44). */
	UPROPERTY(VisibleAnywhere, Category = "Abilities")
	TObjectPtr<UVeyraCooldownComponent> Cooldowns;

	/** The cast that holds the Vanguard now, for telegraphs. */
	UPROPERTY(VisibleAnywhere, Category = "Abilities")
	TObjectPtr<UVeyraCastStateComponent> CastState;

	/** Basic attacks, with the hit chain and a waiting empowerment, which death clears (Combat Bible §44). */
	UPROPERTY(VisibleAnywhere, Category = "Abilities")
	TObjectPtr<UVeyraBasicAttackComponent> BasicAttack;

	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UVeyraRegenerationComponent> Regeneration;

	/** Level, XP, skill points and ranks survive death with the rest of the participant. */
	UPROPERTY(VisibleAnywhere, Category = "Progression")
	TObjectPtr<UVeyraProgressionComponent> Progression;

	/** Gold survives death too (Economy & Progression Bible §14). */
	UPROPERTY(VisibleAnywhere, Category = "Progression")
	TObjectPtr<UVeyraGoldComponent> Gold;

	/** So do items, and purchases waiting for the fountain (§10–§11). */
	UPROPERTY(VisibleAnywhere, Category = "Items")
	TObjectPtr<UVeyraInventoryComponent> Inventory;

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

	UFUNCTION()
	void OnRep_VanguardId();

	UPROPERTY(ReplicatedUsing = OnRep_VanguardId)
	FVeyraContentId VanguardId;

	UPROPERTY(Replicated)
	double RespawnAt = 0.0;

	/** Server only. */
	UPROPERTY(Transient)
	TObjectPtr<UVeyraPassive> Passive;

	FVeyraContentId RequestedVanguardId;

	UPROPERTY(Transient)
	TObjectPtr<AVeyraVanguardController> VanguardController;

	bool bStatsInitialized = false;

	FString AccountId;
};
