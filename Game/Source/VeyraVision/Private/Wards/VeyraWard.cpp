// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Wards/VeyraWard.h"

#include "AbilitySystemComponent.h"
#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Attribution/VeyraAttributionComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerState.h"
#include "Life/VeyraLifeComponent.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Tuning/VeyraVisionTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraVisionLog.h"

AVeyraWard::AVeyraWard(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	// Always relevant, so replays record it; Vision's fog gate decides which clients receive it (ADR-016 §3).
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;

	// It stops no one: its body is on a channel every other body ignores. It still blocks the cursor's
	// unit trace, so a player who sees it can click it (ADR-016 §6).
	Body = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Body"));
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionObjectType(GetBodyChannel());
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Body->SetCanEverAffectNavigation(false);
	Body->SetGenerateOverlapEvents(false);
	RootComponent = Body;

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	DamageAbsorption = CreateDefaultSubobject<UVeyraDamageAbsorptionComponent>(TEXT("DamageAbsorption"));
	Statuses = CreateDefaultSubobject<UVeyraStatusComponent>(TEXT("Statuses"));
	Life = CreateDefaultSubobject<UVeyraLifeComponent>(TEXT("Life"));
	Attribution = CreateDefaultSubobject<UVeyraAttributionComponent>(TEXT("Attribution"));
	VitalsSet = CreateDefaultSubobject<UVeyraVitalsSet>(TEXT("VitalsSet"));
	DefenceSet = CreateDefaultSubobject<UVeyraDefenceSet>(TEXT("DefenceSet"));
}

ECollisionChannel AVeyraWard::GetBodyChannel()
{
	// DefaultEngine.ini names it VeyraMarker, and makes every body ignore it by default.
	return ECC_GameTraceChannel1;
}

void AVeyraWard::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	AbilitySystem->InitAbilityActorInfo(this, this);
	VeyraCombat::ConfigureCombatant(*AbilitySystem, *DamageAbsorption, *Statuses);
}

void AVeyraWard::BeginPlay()
{
	Super::BeginPlay();
	ApplyBody();
}

void AVeyraWard::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraWard, Team, Params);
}

bool AVeyraWard::Place(EVeyraTeam InTeam, APlayerState& InPlacer)
{
	const FVeyraPersistentWardTuning& Tuning = UVeyraVisionTuningSubsystem::Get().PersistentWard;
	if (!HasAuthority() || Placer.IsValid() || !VeyraCombat::InitializeVitals(*AbilitySystem, static_cast<double>(Tuning.HitsToDestroy)))
	{
		UE_LOG(LogVeyraVision, Error, TEXT("%s could not be placed: only the server places a ward, once, and it must take its Health."), *GetName());
		return false;
	}
	Team = InTeam;
	Placer = &InPlacer;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraWard, Team, this);
	// Its lifetime runs on world time, so a pause holds it.
	SetLifeSpan(static_cast<float>(Tuning.LifetimeSeconds));
	ApplyBody();
	return true;
}

bool AVeyraWard::IsAlive() const
{
	return Life && Life->IsAlive();
}

void AVeyraWard::ApplyBody()
{
	const FVeyraPersistentWardTuning& Tuning = UVeyraVisionTuningSubsystem::Get().PersistentWard;
	Body->SetCapsuleSize(static_cast<float>(Tuning.BodyRadius), static_cast<float>(Tuning.BodyHalfHeight));
}
