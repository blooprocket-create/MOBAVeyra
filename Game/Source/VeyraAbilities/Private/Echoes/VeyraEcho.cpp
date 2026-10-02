// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Echoes/VeyraEcho.h"

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
#include "GameFramework/PlayerState.h"
#include "Life/VeyraLifeComponent.h"
#include "Movement/VeyraMovementComponent.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Statuses/VeyraStatusComponent.h"
#include "VeyraAbilitiesLog.h"
#include "VeyraCombatVerbs.h"

AVeyraEcho::AVeyraEcho(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UVeyraMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	bReplicates = true;
	// Always relevant, so replays record it; Vision's fog gate decides which clients receive it (ADR-016 §3).
	bAlwaysRelevant = true;
	// Only a controller Match hands it moves it (ADR-050 §6); clients only draw it.
	AutoPossessAI = EAutoPossessAI::Disabled;
	bUseControllerRotationYaw = false;

	// A projection passes through units, as a companion does, and never traps one; terrain still stops it.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
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

void AVeyraEcho::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	AbilitySystem->InitAbilityActorInfo(this, this);
	VeyraCombat::ConfigureCombatant(*AbilitySystem, *DamageAbsorption, *Statuses);
	if (HasAuthority())
	{
		CastChecked<UVeyraMovementComponent>(GetCharacterMovement())->BindCombatant(AbilitySystem);
		// Its Health is its Integrity, which only its keeper sets (ADR-050 §3).
		VeyraCombat::SealHealth(*AbilitySystem);
	}
}

void AVeyraEcho::BeginPlay()
{
	Super::BeginPlay();
	ApplyWithdrawn();
}

void AVeyraEcho::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraEcho, Team, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraEcho, HolderState, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraEcho, Ability, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraEcho, Anchor, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraEcho, Radius, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraEcho, ImmuneUntil, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraEcho, ControlAt, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraEcho, RepeatsLeft, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraEcho, bWithdrawn, Params);
}

void AVeyraEcho::Configure(UAbilitySystemComponent& InHolder, const FVeyraContentId& InAbility, const FVector& InAnchor)
{
	const AActor* Participant = InHolder.GetOwner();
	HolderAbilities = &InHolder;
	Ability = InAbility;
	Anchor = InAnchor;
	// Its side before it joins the world, so the fog gate knows it from the start (ADR-016 §3).
	Team = VeyraTeams::TeamOf(Participant);
	HolderState = const_cast<APlayerState*>(Cast<APlayerState>(Participant));
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraEcho, Team, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraEcho, HolderState, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraEcho, Ability, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraEcho, Anchor, this);
}

bool AVeyraEcho::TakeHolderSnapshot(double InDamageCoefficient, TOptional<double> Integrity)
{
	const UAbilitySystemComponent* Holder = HolderAbilities.Get();
	const AActor* Participant = Holder ? Holder->GetOwner() : nullptr;
	const UVeyraBasicAttackComponent* HolderAttack = Participant ? Participant->FindComponentByClass<UVeyraBasicAttackComponent>() : nullptr;
	if (!Holder)
	{
		UE_LOG(LogVeyraAbilities, Error, TEXT("%s has no holder to project."), *GetName());
		return false;
	}
	FVeyraStatBlock Stats;
	Stats.MaxHealth = Integrity.Get(Holder->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()));
	Stats.MoveSpeed = Holder->GetNumericAttribute(UVeyraMobilitySet::GetMoveSpeedAttribute());
	Stats.AttackSpeed = Holder->GetNumericAttribute(UVeyraOffenceSet::GetAttackSpeedAttribute());
	DamageCoefficient = InDamageCoefficient;
	// Its holder's basic attack, when its holder has one.
	const bool bAttacks = !HolderAttack || !HolderAttack->HasProfile() || BasicAttack->SetProfile(HolderAttack->GetProfile());
	return VeyraCombat::InitializeStats(*AbilitySystem, Stats) && bAttacks && RefreshOffence();
}

bool AVeyraEcho::RefreshOffence()
{
	const UAbilitySystemComponent* Holder = HolderAbilities.Get();
	return Holder && VeyraCombat::CopyOffence(*Holder, *AbilitySystem, DamageCoefficient);
}

void AVeyraEcho::SetIntegrity(double Integrity, double InRadius)
{
	VeyraCombat::SetSealedHealth(*AbilitySystem, FMath::Max(Integrity, 0.0));
	if (Radius != InRadius)
	{
		Radius = InRadius;
		MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraEcho, Radius, this);
	}
}

void AVeyraEcho::SetRepeatsLeft(int32 InRepeatsLeft)
{
	if (RepeatsLeft != InRepeatsLeft)
	{
		RepeatsLeft = InRepeatsLeft;
		MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraEcho, RepeatsLeft, this);
	}
}

void AVeyraEcho::SetProjection(double InImmuneUntil, double InControlAt)
{
	ImmuneUntil = InImmuneUntil;
	ControlAt = InControlAt;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraEcho, ImmuneUntil, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraEcho, ControlAt, this);
}

void AVeyraEcho::Withdraw()
{
	if (bWithdrawn)
	{
		return;
	}
	// It leaves at nobody's hand, so nothing is credited or rewarded; what it set going keeps it as its source.
	VeyraCombat::Withdraw(*AbilitySystem);
	bWithdrawn = true;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraEcho, bWithdrawn, this);
	ApplyWithdrawn();
}

void AVeyraEcho::OnRep_Withdrawn()
{
	ApplyWithdrawn();
}

void AVeyraEcho::ApplyWithdrawn()
{
	SetActorHiddenInGame(bWithdrawn);
	SetActorEnableCollision(!bWithdrawn);
}
