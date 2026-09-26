// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraPlayerState.h"

#include "AbilitySystemComponent.h"
#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraMobilitySet.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraResourceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Life/VeyraLifeComponent.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "VeyraCombatVerbs.h"
#include "VeyraMatchLog.h"

AVeyraPlayerState::AVeyraPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	DamageAbsorption = CreateDefaultSubobject<UVeyraDamageAbsorptionComponent>(TEXT("DamageAbsorption"));
	Life = CreateDefaultSubobject<UVeyraLifeComponent>(TEXT("Life"));

	// Attribute Sets created as default subobjects of the owner register with its Ability System
	// Component when the component initializes.
	VitalsSet = CreateDefaultSubobject<UVeyraVitalsSet>(TEXT("VitalsSet"));
	OffenceSet = CreateDefaultSubobject<UVeyraOffenceSet>(TEXT("OffenceSet"));
	DefenceSet = CreateDefaultSubobject<UVeyraDefenceSet>(TEXT("DefenceSet"));
	MobilitySet = CreateDefaultSubobject<UVeyraMobilitySet>(TEXT("MobilitySet"));
	ResourceSet = CreateDefaultSubobject<UVeyraResourceSet>(TEXT("ResourceSet"));
}

UAbilitySystemComponent* AVeyraPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AVeyraPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Until a Vanguard exists the PlayerState is its own avatar; the Vanguard takes over when it is
	// given this PlayerState (AVeyraVanguardCharacter::OnPlayerStateChanged).
	AbilitySystem->InitAbilityActorInfo(this, GetPawn() ? static_cast<AActor*>(GetPawn()) : this);
	VeyraCombat::ConfigureCombatant(*AbilitySystem, *DamageAbsorption);
}

void AVeyraPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraPlayerState, Team, Params);
}

void AVeyraPlayerState::SetVeyraTeam(EVeyraTeam NewTeam)
{
	Team = NewTeam;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraPlayerState, Team, this);
}

void AVeyraPlayerState::OnDeactivated()
{
	SetIsInactive(true);
	UE_LOG(LogVeyraMatch, Log, TEXT("%s disconnected; the Vanguard stays in the match."), *GetPlayerName());
}
