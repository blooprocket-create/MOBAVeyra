// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Structures/VeyraStructure.h"

#include "AbilitySystemComponent.h"
#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Attribution/VeyraAttributionComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/CollisionProfile.h"
#include "Layout/VeyraLayout.h"
#include "Life/VeyraLifeComponent.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Rules/VeyraStructureRules.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Structures/VeyraStructureAttackComponent.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraWorldLog.h"

AVeyraStructure::AVeyraStructure(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	// Every machine sees every structure, as League shows towers: the map is public (ADR-011 §7, ADR-016 §11).
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;

	// It blocks movement and carves navigation, so paths route around it; the cursor finds it as it does any unit.
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
	// Server logic only; kinds that do not shoot never start it.
	Attack = CreateDefaultSubobject<UVeyraStructureAttackComponent>(TEXT("Attack"));
}

void AVeyraStructure::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	AbilitySystem->InitAbilityActorInfo(this, this);
	VeyraCombat::ConfigureCombatant(*AbilitySystem, *DamageAbsorption, *Statuses);
}

void AVeyraStructure::BeginPlay()
{
	Super::BeginPlay();
	// A client learns the kind by replication, then shapes the body to match.
	ApplyBody();
}

bool AVeyraStructure::InitializeStats()
{
	const FVeyraStructureTuning& Tuning = GetTuning();
	const bool bVitals = VeyraCombat::InitializeVitals(*AbilitySystem, Tuning.MaxHealth);
	const bool bResistances = VeyraCombat::InitializeResistances(*AbilitySystem, Tuning.Armor, Tuning.MagicResist);
	UE_CLOG(!bVitals || !bResistances, LogVeyraWorld, Error, TEXT("%s could not take its stats; see the errors above."), *GetName());
	return bVitals && bResistances;
}

void AVeyraStructure::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraStructure, Team, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraStructure, Kind, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraStructure, bHasLane, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraStructure, Lane, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraStructure, Order, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraStructure, bInvulnerable, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraStructure, RebuildsAt, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraStructure, BackdoorProtection, Params);
}

void AVeyraStructure::Configure(const FVeyraStructurePlacement& Placement)
{
	Team = Placement.Team;
	Kind = Placement.Kind;
	bHasLane = Placement.Lane.IsSet();
	Lane = Placement.Lane.Get(EVeyraLane::Mid);
	Order = Placement.Order;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraStructure, Team, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraStructure, Kind, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraStructure, bHasLane, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraStructure, Lane, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraStructure, Order, this);
	ApplyBody();
}

TOptional<EVeyraLane> AVeyraStructure::GetLane() const
{
	return bHasLane ? TOptional<EVeyraLane>(Lane) : TOptional<EVeyraLane>();
}

bool AVeyraStructure::IsDestroyed() const
{
	return Life && !Life->IsAlive();
}

void AVeyraStructure::SetInvulnerable(bool bNewInvulnerable)
{
	if (!HasAuthority() || bNewInvulnerable == bInvulnerable)
	{
		return;
	}
	if (bNewInvulnerable)
	{
		VeyraCombat::GrantInvulnerability(*AbilitySystem);
	}
	else
	{
		VeyraCombat::RevokeInvulnerability(*AbilitySystem);
	}
	bInvulnerable = bNewInvulnerable;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraStructure, bInvulnerable, this);
}

void AVeyraStructure::SetBackdoorProtection(double Fraction)
{
	if (!HasAuthority() || Fraction == BackdoorProtection || !VeyraCombat::SetBaseDamageReduction(*AbilitySystem, Fraction))
	{
		return;
	}
	BackdoorProtection = Fraction;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraStructure, BackdoorProtection, this);
}

void AVeyraStructure::SetRebuildsAt(double At)
{
	RebuildsAt = At;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraStructure, RebuildsAt, this);
}

bool AVeyraStructure::Rebuild()
{
	SetRebuildsAt(0.0);
	return VeyraCombat::Revive(*AbilitySystem);
}

const FVeyraStructureTuning& AVeyraStructure::GetTuning() const
{
	const FVeyraStructuresTuning& Structures = UVeyraWorldTuningSubsystem::Get().Structures;
	switch (Kind)
	{
	case EVeyraStructureKind::LaneSpire:
		return Structures.LaneSpire;
	case EVeyraStructureKind::BaseTower:
		return Structures.BaseTower;
	case EVeyraStructureKind::Inhibitor:
		return Structures.Inhibitor;
	case EVeyraStructureKind::PrimeWell:
		return Structures.PrimeWell;
	}
	return Structures.LaneSpire;
}

UVeyraStructureAttackComponent* AVeyraStructure::GetAttack() const
{
	return VeyraStructureRules::Attacks(Kind) ? Attack.Get() : nullptr;
}

void AVeyraStructure::ApplyBody()
{
	const FVeyraStructureTuning& Tuning = GetTuning();
	Capsule->SetCapsuleSize(static_cast<float>(Tuning.CapsuleRadius), static_cast<float>(Tuning.CapsuleHalfHeight));
}
