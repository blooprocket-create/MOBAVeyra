// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraPlayerState.h"

#include "AbilitySystemComponent.h"
#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "VeyraCombatVerbs.h"

AVeyraPlayerState::AVeyraPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	DamageAbsorption = CreateDefaultSubobject<UVeyraDamageAbsorptionComponent>(TEXT("DamageAbsorption"));

	// Attribute Sets created as default subobjects of the owner register with its Ability System
	// Component when the component initializes.
	VitalsSet = CreateDefaultSubobject<UVeyraVitalsSet>(TEXT("VitalsSet"));
	OffenceSet = CreateDefaultSubobject<UVeyraOffenceSet>(TEXT("OffenceSet"));
	DefenceSet = CreateDefaultSubobject<UVeyraDefenceSet>(TEXT("DefenceSet"));
}

UAbilitySystemComponent* AVeyraPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AVeyraPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// The pawn becomes the avatar when it is possessed (M3); until then the PlayerState is both.
	AbilitySystem->InitAbilityActorInfo(this, GetPawn() ? static_cast<AActor*>(GetPawn()) : this);
	VeyraCombat::ConfigureCombatant(*AbilitySystem, *DamageAbsorption);
}
