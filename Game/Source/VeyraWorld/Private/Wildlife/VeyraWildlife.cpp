// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Wildlife/VeyraWildlife.h"

#include "AbilitySystemComponent.h"
#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attribution/VeyraAttributionComponent.h"
#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraMobilitySet.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraResourceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Components/CapsuleComponent.h"
#include "Engine/NetDriver.h"
#include "Life/VeyraLifeComponent.h"
#include "Movement/VeyraMovementComponent.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraWorldLog.h"
#include "Wildlife/VeyraWildlifeController.h"

AVeyraWildlife::AVeyraWildlife(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UVeyraMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	bReplicates = true;
	// Every machine sees every creature until Vision gates them, as it sees Fluxborn (ADR-011 §7).
	bAlwaysRelevant = true;
	// Its server controller keeps it to its camp; clients only draw it.
	AutoPossessAI = EAutoPossessAI::Spawned;
	AIControllerClass = AVeyraWildlifeController::StaticClass();
	bUseControllerRotationYaw = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	// Crowd control that displaces it runs even between its controller's orders.
	Movement->bRunPhysicsWithNoController = true;

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	DamageAbsorption = CreateDefaultSubobject<UVeyraDamageAbsorptionComponent>(TEXT("DamageAbsorption"));
	Statuses = CreateDefaultSubobject<UVeyraStatusComponent>(TEXT("Statuses"));
	Life = CreateDefaultSubobject<UVeyraLifeComponent>(TEXT("Life"));
	Attribution = CreateDefaultSubobject<UVeyraAttributionComponent>(TEXT("Attribution"));
	BasicAttack = CreateDefaultSubobject<UVeyraBasicAttackComponent>(TEXT("BasicAttack"));
	VitalsSet = CreateDefaultSubobject<UVeyraVitalsSet>(TEXT("VitalsSet"));
	OffenceSet = CreateDefaultSubobject<UVeyraOffenceSet>(TEXT("OffenceSet"));
	DefenceSet = CreateDefaultSubobject<UVeyraDefenceSet>(TEXT("DefenceSet"));
	MobilitySet = CreateDefaultSubobject<UVeyraMobilitySet>(TEXT("MobilitySet"));
	ResourceSet = CreateDefaultSubobject<UVeyraResourceSet>(TEXT("ResourceSet"));
}

void AVeyraWildlife::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	AbilitySystem->InitAbilityActorInfo(this, this);
	VeyraCombat::ConfigureCombatant(*AbilitySystem, *DamageAbsorption, *Statuses);
	if (HasAuthority())
	{
		GetVeyraMovement()->BindCombatant(AbilitySystem);
	}
}

void AVeyraWildlife::BeginPlay()
{
	Super::BeginPlay();
	ApplyBody();
	if (!HasAuthority())
	{
		return;
	}
	// It replicates at the Fluxborn's whole fraction of the server's tick (ADR-011 §7).
	const FVeyraWorldTuning& Tuning = UVeyraWorldTuningSubsystem::Get();
	const UNetDriver* Driver = GetNetDriver();
	const int32 TickRate = Driver ? Driver->GetNetServerMaxTickRate() : 0;
	if (TickRate > 0 && Tuning.Replication.FluxbornUpdateEveryServerTicks > 0)
	{
		SetNetUpdateFrequency(static_cast<float>(TickRate) / static_cast<float>(Tuning.Replication.FluxbornUpdateEveryServerTicks));
	}
}

void AVeyraWildlife::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraWildlife, Species, Params);
}

void AVeyraWildlife::Configure(const FVeyraContentId& InSpecies, int32 InCamp, const FVector& InHome, double InLeashRadius)
{
	Species = InSpecies;
	Camp = InCamp;
	Home = InHome;
	LeashRadius = InLeashRadius;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraWildlife, Species, this);
	ApplyBody();
}

bool AVeyraWildlife::InitializeStats()
{
	const FVeyraWildlifeSpecies* Definition = GetDefinition();
	if (!Definition)
	{
		UE_LOG(LogVeyraWorld, Error, TEXT("%s is an unknown species of wildlife (%s); World.json defines none."), *GetName(), *Species.ToString());
		return false;
	}
	return VeyraCombat::InitializeStats(*AbilitySystem, Definition->Stats) && BasicAttack->SetProfile(Definition->BasicAttack);
}

const FVeyraWildlifeSpecies* AVeyraWildlife::GetDefinition() const
{
	return UVeyraWorldTuningSubsystem::Get().FindSpecies(Species);
}

bool AVeyraWildlife::IsAlive() const
{
	return Life && Life->IsAlive();
}

UVeyraMovementComponent* AVeyraWildlife::GetVeyraMovement() const
{
	return CastChecked<UVeyraMovementComponent>(GetCharacterMovement());
}

void AVeyraWildlife::OnRep_Species()
{
	ApplyBody();
}

void AVeyraWildlife::ApplyBody()
{
	if (const FVeyraWildlifeSpecies* Definition = GetDefinition())
	{
		GetCapsuleComponent()->SetCapsuleSize(static_cast<float>(Definition->CapsuleRadius), static_cast<float>(Definition->CapsuleHalfHeight));
	}
}
