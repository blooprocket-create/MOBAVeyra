// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Fluxborn/VeyraFluxborn.h"

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
#include "Fluxborn/VeyraFluxbornController.h"
#include "Life/VeyraLifeComponent.h"
#include "Movement/VeyraMovementComponent.h"
#include "Movement/VeyraUnitCollision.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraWorldLog.h"

AVeyraFluxborn::AVeyraFluxborn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UVeyraMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	bReplicates = true;
	// Always relevant, so replays record it; Vision's fog gate decides which clients receive it (ADR-016 §3).
	bAlwaysRelevant = true;
	// Its server controller moves it; clients only draw it.
	AutoPossessAI = EAutoPossessAI::Spawned;
	AIControllerClass = AVeyraFluxbornController::StaticClass();
	bUseControllerRotationYaw = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	// Crowd control that displaces it runs even between its controller's orders.
	Movement->bRunPhysicsWithNoController = true;
	Movement->bUseRVOAvoidance = true;

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

void AVeyraFluxborn::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	AbilitySystem->InitAbilityActorInfo(this, this);
	VeyraCombat::ConfigureCombatant(*AbilitySystem, *DamageAbsorption, *Statuses);
	if (HasAuthority())
	{
		GetVeyraMovement()->BindCombatant(AbilitySystem);
	}
}

void AVeyraFluxborn::BeginPlay()
{
	Super::BeginPlay();
	ApplyBody();
	if (!HasAuthority())
	{
		return;
	}
	const FVeyraWorldTuning& Tuning = UVeyraWorldTuningSubsystem::Get();
	// It replicates at a whole fraction of the server's tick (ADR-011 §7).
	const UNetDriver* Driver = GetNetDriver();
	const int32 TickRate = Driver ? Driver->GetNetServerMaxTickRate() : 0;
	if (TickRate > 0 && Tuning.Replication.FluxbornUpdateEveryServerTicks > 0)
	{
		SetNetUpdateFrequency(static_cast<float>(TickRate) / static_cast<float>(Tuning.Replication.FluxbornUpdateEveryServerTicks));
	}
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->AvoidanceConsiderationRadius = static_cast<float>(Tuning.Fluxborn.AvoidanceConsiderationRadius);
	Movement->AvoidanceWeight = static_cast<float>(Tuning.Fluxborn.AvoidanceWeight);
}

void AVeyraFluxborn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraFluxborn, Team, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraFluxborn, Kind, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraFluxborn, Lane, Params);
}

void AVeyraFluxborn::Configure(const FVeyraContentId& InKind, EVeyraTeam InTeam, EVeyraLane InLane, TArray<FVector2D> InWaypoints)
{
	Kind = InKind;
	Team = InTeam;
	Lane = InLane;
	Waypoints = MoveTemp(InWaypoints);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraFluxborn, Kind, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraFluxborn, Team, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraFluxborn, Lane, this);
	ApplyBody();
	// It collides by side: allied units pass through it, enemies are blocked (ADR-062 §1). It steers around
	// everyone, its own side's Vanguards too, so a wave makes way for them (§3).
	VeyraUnitCollision::ApplySide(*GetCapsuleComponent(), Team);
	VeyraUnitCollision::ApplyAvoidanceGroups(*GetCharacterMovement(), Team, VeyraUnitCollision::EAvoidanceRole::Fluxborn);
}

bool AVeyraFluxborn::InitializeStats(double HealthMultiplier, double DamageMultiplier)
{
	const FVeyraFluxbornDefinition* Definition = GetDefinition();
	if (!Definition)
	{
		UE_LOG(LogVeyraWorld, Error, TEXT("%s is an unknown kind of Fluxborn (%s); World.json defines none."), *GetName(), *Kind.ToString());
		return false;
	}
	const bool bStats = VeyraCombat::InitializeStats(*AbilitySystem, Definition->Stats) && BasicAttack->SetProfile(Definition->BasicAttack);
	return bStats && ApplyStrength(HealthMultiplier, DamageMultiplier);
}

bool AVeyraFluxborn::ApplyStrength(double HealthMultiplier, double DamageMultiplier)
{
	const FVeyraFluxbornDefinition* Definition = GetDefinition();
	return Definition && VeyraCombat::SetUnitScaling(*AbilitySystem, Definition->Stats.MaxHealth, HealthMultiplier, DamageMultiplier);
}

const FVeyraFluxbornDefinition* AVeyraFluxborn::GetDefinition() const
{
	return UVeyraWorldTuningSubsystem::Get().FindFluxborn(Kind);
}

bool AVeyraFluxborn::IsAlive() const
{
	return Life && Life->IsAlive();
}

UVeyraMovementComponent* AVeyraFluxborn::GetVeyraMovement() const
{
	return CastChecked<UVeyraMovementComponent>(GetCharacterMovement());
}

void AVeyraFluxborn::OnRep_Kind()
{
	ApplyBody();
}

void AVeyraFluxborn::ApplyBody()
{
	if (const FVeyraFluxbornDefinition* Definition = GetDefinition())
	{
		GetCapsuleComponent()->SetCapsuleSize(static_cast<float>(Definition->CapsuleRadius), static_cast<float>(Definition->CapsuleHalfHeight));
	}
}
