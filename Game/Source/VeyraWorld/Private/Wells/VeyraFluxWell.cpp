// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Wells/VeyraFluxWell.h"

#include "AbilitySystemComponent.h"
#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Attribution/VeyraAttributionComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/CollisionProfile.h"
#include "Life/VeyraLifeComponent.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraWorldLog.h"

AVeyraFluxWell::AVeyraFluxWell(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	// Every machine sees both Wells until Vision gates them, as it sees structures (ADR-011 §7).
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;

	// It blocks movement and carves navigation, as a structure does; the cursor finds it as it does any unit.
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	Capsule->SetCanEverAffectNavigation(true);
	Capsule->bDynamicObstacle = true;
	RootComponent = Capsule;

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
}

void AVeyraFluxWell::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	AbilitySystem->InitAbilityActorInfo(this, this);
	VeyraCombat::ConfigureCombatant(*AbilitySystem, *DamageAbsorption, *Statuses);
}

void AVeyraFluxWell::BeginPlay()
{
	Super::BeginPlay();
	ApplyBody();
}

void AVeyraFluxWell::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraFluxWell, Site, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraFluxWell, State, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraFluxWell, OpensAt, Params);
}

void AVeyraFluxWell::Configure(int32 InSite)
{
	Site = InSite;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraFluxWell, Site, this);
	ApplyBody();
}

bool AVeyraFluxWell::InitializeStats()
{
	const FVeyraFluxWellsTuning& Tuning = UVeyraWorldTuningSubsystem::Get().FluxWells;
	const bool bVitals = VeyraCombat::InitializeVitals(*AbilitySystem, Tuning.MaxHealth);
	const bool bResistances = VeyraCombat::InitializeResistances(*AbilitySystem, Tuning.Armor, Tuning.MagicResist);
	UE_CLOG(!bVitals || !bResistances, LogVeyraWorld, Error, TEXT("%s could not take its stats; see the errors above."), *GetName());
	return bVitals && bResistances;
}

void AVeyraFluxWell::SetState(EVeyraFluxWellState NewState, double NewOpensAt)
{
	if (!HasAuthority())
	{
		return;
	}
	State = NewState;
	OpensAt = NewOpensAt;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraFluxWell, State, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraFluxWell, OpensAt, this);
	// Only an open Well can be damaged.
	const bool bShouldBeInvulnerable = NewState != EVeyraFluxWellState::Open;
	if (bShouldBeInvulnerable != bInvulnerable)
	{
		if (bShouldBeInvulnerable)
		{
			VeyraCombat::GrantInvulnerability(*AbilitySystem);
		}
		else
		{
			VeyraCombat::RevokeInvulnerability(*AbilitySystem);
		}
		bInvulnerable = bShouldBeInvulnerable;
	}
}

bool AVeyraFluxWell::IsStanding() const
{
	return Life && Life->IsAlive();
}

void AVeyraFluxWell::ApplyBody()
{
	const FVeyraFluxWellsTuning& Tuning = UVeyraWorldTuningSubsystem::Get().FluxWells;
	Capsule->SetCapsuleSize(static_cast<float>(Tuning.CapsuleRadius), static_cast<float>(Tuning.CapsuleHalfHeight));
}
