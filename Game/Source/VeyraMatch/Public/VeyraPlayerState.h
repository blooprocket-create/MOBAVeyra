// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"

#include "VeyraPlayerState.generated.h"

class UAbilitySystemComponent;
class UVeyraDamageAbsorptionComponent;
class UVeyraDefenceSet;
class UVeyraOffenceSet;
class UVeyraVitalsSet;

/**
 * One participant's persistent match state. It owns the Vanguard's Ability System Component and
 * Attribute Sets, so cooldowns, effects and attribution survive death, respawn and reconnect; the
 * pawn is only the avatar (ADR-006 §4). AI-controlled Vanguards get one too.
 */
UCLASS()
class VEYRAMATCH_API AVeyraPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AVeyraPlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void PostInitializeComponents() override;

private:
	/** Replicated in Mixed mode: full effect data to the owning client, the minimum to everyone else. */
	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UVeyraDamageAbsorptionComponent> DamageAbsorption;

	UPROPERTY()
	TObjectPtr<UVeyraVitalsSet> VitalsSet;

	UPROPERTY()
	TObjectPtr<UVeyraOffenceSet> OffenceSet;

	UPROPERTY()
	TObjectPtr<UVeyraDefenceSet> DefenceSet;
};
