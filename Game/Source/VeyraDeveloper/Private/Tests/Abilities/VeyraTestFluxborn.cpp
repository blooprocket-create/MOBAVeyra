// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tests/Abilities/VeyraTestFluxborn.h"

#include "AbilitySystemComponent.h"
#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attribution/VeyraAttributionComponent.h"
#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraMobilitySet.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraResourceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Life/VeyraLifeComponent.h"
#include "Movement/VeyraMovementComponent.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Statuses/VeyraStatusComponent.h"
#include "VeyraCombatVerbs.h"

AVeyraTestFluxborn::AVeyraTestFluxborn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UVeyraMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;
	// Nothing controls it, so the server runs its movement anyway, as forced moves need.
	GetCharacterMovement()->bRunPhysicsWithNoController = true;

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	DamageAbsorption = CreateDefaultSubobject<UVeyraDamageAbsorptionComponent>(TEXT("DamageAbsorption"));
	Statuses = CreateDefaultSubobject<UVeyraStatusComponent>(TEXT("Statuses"));
	Life = CreateDefaultSubobject<UVeyraLifeComponent>(TEXT("Life"));
	Attribution = CreateDefaultSubobject<UVeyraAttributionComponent>(TEXT("Attribution"));
	VitalsSet = CreateDefaultSubobject<UVeyraVitalsSet>(TEXT("VitalsSet"));
	OffenceSet = CreateDefaultSubobject<UVeyraOffenceSet>(TEXT("OffenceSet"));
	DefenceSet = CreateDefaultSubobject<UVeyraDefenceSet>(TEXT("DefenceSet"));
	MobilitySet = CreateDefaultSubobject<UVeyraMobilitySet>(TEXT("MobilitySet"));
	ResourceSet = CreateDefaultSubobject<UVeyraResourceSet>(TEXT("ResourceSet"));
}

void AVeyraTestFluxborn::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	AbilitySystem->InitAbilityActorInfo(this, this);
	VeyraCombat::ConfigureCombatant(*AbilitySystem, *DamageAbsorption, *Statuses);
	if (HasAuthority())
	{
		GetVeyraMovement()->BindCombatant(AbilitySystem);
	}
}

void AVeyraTestFluxborn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraTestFluxborn, Team, Params);
}

void AVeyraTestFluxborn::SetVeyraTeam(EVeyraTeam NewTeam)
{
	Team = NewTeam;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraTestFluxborn, Team, this);
}

UVeyraMovementComponent* AVeyraTestFluxborn::GetVeyraMovement() const
{
	return CastChecked<UVeyraMovementComponent>(GetCharacterMovement());
}

AVeyraTestStructure::AVeyraTestStructure(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

AVeyraTestWildlife::AVeyraTestWildlife(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

AVeyraTestWard::AVeyraTestWard(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

AVeyraTestObjective::AVeyraTestObjective(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

AVeyraTestOwnedUnit::AVeyraTestOwnedUnit(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}
