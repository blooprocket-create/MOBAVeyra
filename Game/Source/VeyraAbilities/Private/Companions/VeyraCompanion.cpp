// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Companions/VeyraCompanion.h"

#include "AbilitySystemComponent.h"
#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attribution/VeyraAttributionComponent.h"
#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraMobilitySet.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraResourceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Companions/VeyraCompanionController.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerState.h"
#include "Life/VeyraLifeComponent.h"
#include "Movement/VeyraMovementComponent.h"
#include "Movement/VeyraUnitCollision.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Stats/VeyraEquipmentStats.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraAbilitiesLog.h"
#include "VeyraAbilitiesVerbs.h"
#include "VeyraCombatVerbs.h"

namespace
{
	/** Base, plus Growth for each of Levels. */
	FVeyraStatBlock Grown(const FVeyraStatBlock& Base, const FVeyraStatBlock& Growth, int32 Levels)
	{
		FVeyraStatBlock Stats = Base;
		Stats.MaxHealth += Growth.MaxHealth * Levels;
		Stats.HealthRegen += Growth.HealthRegen * Levels;
		Stats.MaxResource += Growth.MaxResource * Levels;
		Stats.ResourceRegen += Growth.ResourceRegen * Levels;
		Stats.Armor += Growth.Armor * Levels;
		Stats.MagicResist += Growth.MagicResist * Levels;
		Stats.PhysicalPower += Growth.PhysicalPower * Levels;
		Stats.MagicPower += Growth.MagicPower * Levels;
		Stats.AttackSpeed += Growth.AttackSpeed * Levels;
		Stats.MoveSpeed += Growth.MoveSpeed * Levels;
		return Stats;
	}
}

AVeyraCompanion::AVeyraCompanion(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UVeyraMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	bReplicates = true;
	// Always relevant, so replays record it; Vision's fog gate decides which clients receive it (ADR-016 §3).
	bAlwaysRelevant = true;
	// Its server controller moves it; clients only draw it.
	AutoPossessAI = EAutoPossessAI::Spawned;
	AIControllerClass = AVeyraCompanionController::StaticClass();
	bUseControllerRotationYaw = false;

	// It follows in its owner's footsteps, so a solid body would stand in the way of every turn back:
	// ghosted, it passes through units and never traps an ally (Combat Bible §24; ADR-034 §4). Terrain
	// still stops it, and it is gathered and hit as any unit is.
	VeyraUnitCollision::SetResponseToUnits(*GetCapsuleComponent(), ECR_Ignore);

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

void AVeyraCompanion::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	AbilitySystem->InitAbilityActorInfo(this, this);
	VeyraCombat::ConfigureCombatant(*AbilitySystem, *DamageAbsorption, *Statuses);
	if (HasAuthority())
	{
		GetVeyraMovement()->BindCombatant(AbilitySystem);
	}
}

void AVeyraCompanion::BeginPlay()
{
	Super::BeginPlay();
	ApplyBody();
	ApplyBanished();
}

void AVeyraCompanion::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraCompanion, Team, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraCompanion, Definition, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraCompanion, OwnerState, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraCompanion, bBanished, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraCompanion, bChained, Params);
}

APawn* AVeyraCompanion::BodyOf(const UAbilitySystemComponent& Owner)
{
	return Cast<APawn>(Owner.GetAvatarActor());
}

void AVeyraCompanion::Configure(const FVeyraContentId& InDefinition, UAbilitySystemComponent& InOwner)
{
	const AActor* Participant = InOwner.GetOwner();
	Definition = InDefinition;
	OwnerAbilities = &InOwner;
	// Its side before it joins the world, so the fog gate knows it from the start (ADR-016 §3).
	Team = VeyraTeams::TeamOf(Participant);
	OwnerState = const_cast<APlayerState*>(Cast<APlayerState>(Participant));
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraCompanion, Definition, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraCompanion, Team, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraCompanion, OwnerState, this);
	ApplyBody();
}

bool AVeyraCompanion::InitializeStats(int32 OwnerLevel)
{
	const FVeyraCompanionTuning* Tuning = GetDefinition();
	if (!Tuning)
	{
		UE_LOG(LogVeyraAbilities, Error, TEXT("%s is an unknown companion (%s); Abilities.json defines none."), *GetName(), *Definition.ToString());
		return false;
	}
	GrownLevel = FMath::Max(OwnerLevel, 1);
	return VeyraCombat::InitializeStats(*AbilitySystem, Grown(Tuning->Stats, Tuning->Growth, GrownLevel - 1)) && BasicAttack->SetProfile(Tuning->BasicAttack);
}

bool AVeyraCompanion::GrowTo(int32 OwnerLevel)
{
	const FVeyraCompanionTuning* Tuning = GetDefinition();
	if (!Tuning || OwnerLevel <= GrownLevel)
	{
		return Tuning != nullptr;
	}
	if (!VeyraCombat::GrowBaseStats(*AbilitySystem, Grown(FVeyraStatBlock(), Tuning->Growth, OwnerLevel - GrownLevel)))
	{
		return false;
	}
	GrownLevel = OwnerLevel;
	return true;
}

bool AVeyraCompanion::Inherit(double OwnerMagicPower)
{
	const FVeyraCompanionTuning* Tuning = GetDefinition();
	if (!Tuning)
	{
		return false;
	}
	// Nothing else of its owner's is its: no items, crits, on-hit effects, lifesteal or buffs (Combat Bible §32).
	FVeyraEquipmentStats Held;
	Held.MagicPower = Tuning->OwnerMagicPowerShare * FMath::Max(OwnerMagicPower, 0.0);
	if (InheritedMagicPower.IsSet() && FMath::IsNearlyEqual(InheritedMagicPower.GetValue(), Held.MagicPower))
	{
		return true;
	}
	// A companion carries no items, so the line Combat keeps for what a unit carries holds its inheritance.
	if (!VeyraCombat::SetEquipmentStats(*AbilitySystem, Held))
	{
		return false;
	}
	InheritedMagicPower = Held.MagicPower;
	return true;
}

void AVeyraCompanion::Banish()
{
	check(HasAuthority());
	EndHold();
	SetChained(false);
	// One killed is dead already; one whose owner fell leaves at nobody's hand (ADR-034 §3).
	if (IsAlive())
	{
		VeyraCombat::Withdraw(*AbilitySystem);
	}
	// A windup under way leaves with it: a banished companion strikes nothing.
	BasicAttack->CancelAttack();
	if (AController* Brain = GetController())
	{
		Brain->StopMovement();
	}
	GetCharacterMovement()->StopMovementImmediately();
	bBanished = true;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraCompanion, bBanished, this);
	ApplyBanished();
}

bool AVeyraCompanion::Reform(const FVector& Where)
{
	check(HasAuthority());
	if (!bBanished)
	{
		return false;
	}
	// Collision returns first, so the body finds its room where it reforms.
	bBanished = false;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraCompanion, bBanished, this);
	ApplyBanished();
	if (!TeleportTo(Where, GetActorRotation(), /*bIsATest*/ false, /*bNoCheck*/ false))
	{
		TeleportTo(Where, GetActorRotation(), /*bIsATest*/ false, /*bNoCheck*/ true);
	}
	Mode = EVeyraCompanionMode::Follow;
	return VeyraCombat::Revive(*AbilitySystem);
}

void AVeyraCompanion::HoldAt(const FVector& Where, double InHoldsUntil, const FVeyraContentId& OpenedBy)
{
	Mode = EVeyraCompanionMode::Hold;
	HoldPoint = Where;
	HoldsUntil = InHoldsUntil;
	HoldOpenedBy = OpenedBy;
}

void AVeyraCompanion::EndHold()
{
	if (Mode != EVeyraCompanionMode::Hold)
	{
		return;
	}
	Mode = EVeyraCompanionMode::Follow;
	const FVeyraContentId OpenedBy = HoldOpenedBy;
	HoldOpenedBy = FVeyraContentId();
	// The recall it offered has nothing left to recall (ADR-034 §5).
	if (UAbilitySystemComponent* Keeper = OwnerAbilities.Get(); Keeper && OpenedBy.IsValid())
	{
		VeyraAbilities::EndFollowUp(*Keeper, OpenedBy);
	}
}

void AVeyraCompanion::Bind(EVeyraCompanionMode InMode, AActor& Unit)
{
	// Escort an ally or hunt an enemy, whichever it was summoned for (ADR-035 §5).
	if (InMode != EVeyraCompanionMode::Escort && InMode != EVeyraCompanionMode::Hunt)
	{
		return;
	}
	Mode = InMode;
	BoundTo = &Unit;
}

void AVeyraCompanion::Anchor(const FVector& Where, const FVector& Facing)
{
	check(HasAuthority());
	EndHold();
	BoundTo.Reset();
	// Set down from a move, it holds its posture's statuses again (ADR-037 §3).
	if (bMoving)
	{
		DropHeldStatuses();
		bMoving = false;
	}
	Mode = EVeyraCompanionMode::Anchored;
	// It keeps the way it was set facing, whatever way it walked.
	GetCharacterMovement()->bOrientRotationToMovement = false;
	HoldPoint = Where;
	const FVector Flat = Facing.GetSafeNormal2D();
	AnchorFacing = Flat.IsNearlyZero() ? GetActorForwardVector().GetSafeNormal2D() : Flat;
	// Whatever it was doing ends where it stood: a windup, a path.
	BasicAttack->CancelAttack();
	if (AController* Brain = GetController())
	{
		Brain->StopMovement();
	}
	const FRotator Turned(0.0, AnchorFacing.Rotation().Yaw, 0.0);
	if (!TeleportTo(Where, Turned, /*bIsATest*/ false, /*bNoCheck*/ false))
	{
		TeleportTo(Where, Turned, /*bIsATest*/ false, /*bNoCheck*/ true);
	}
	KeepHeldStatuses();
}

bool AVeyraCompanion::NextPosture(const FVector& Facing)
{
	check(HasAuthority());
	const FVeyraCompanionTuning* Tuning = GetDefinition();
	if (!Tuning || Tuning->Postures.Num() < 2)
	{
		return false;
	}
	// Moving, it holds its moving statuses still: only the posture it returns to changes.
	if (!bMoving)
	{
		DropHeldStatuses();
		const FVector Flat = Facing.GetSafeNormal2D();
		if (!Flat.IsNearlyZero())
		{
			AnchorFacing = Flat;
			FaceAnchor();
		}
	}
	Posture = (Posture + 1) % Tuning->Postures.Num();
	if (HoldsFire())
	{
		BasicAttack->CancelAttack();
	}
	KeepHeldStatuses();
	return true;
}

bool AVeyraCompanion::HoldsFire() const
{
	const FVeyraCompanionTuning* Tuning = GetDefinition();
	return !bMoving && Tuning && Tuning->Postures.IsValidIndex(Posture) && Tuning->Postures[Posture].Attacks == EVeyraPostureAttacks::Holds;
}

void AVeyraCompanion::StartMoving(AActor& Ally, const FVector& Facing)
{
	check(HasAuthority());
	// Its posture's statuses give way to its moving ones; sent again while moving, its moving ones are given afresh.
	DropHeldStatuses();
	bMoving = true;
	Mode = EVeyraCompanionMode::Escort;
	BoundTo = &Ally;
	const FVector Flat = Facing.GetSafeNormal2D();
	if (!Flat.IsNearlyZero())
	{
		AnchorFacing = Flat;
	}
	GetCharacterMovement()->bOrientRotationToMovement = false;
	FaceAnchor();
	KeepHeldStatuses();
}

TConstArrayView<FVeyraContentId> AVeyraCompanion::HeldStatuses() const
{
	const FVeyraCompanionTuning* Tuning = GetDefinition();
	if (!Tuning)
	{
		return {};
	}
	if (bMoving)
	{
		return Tuning->MovingStatuses;
	}
	return Tuning->Postures.IsValidIndex(Posture) ? TConstArrayView<FVeyraContentId>(Tuning->Postures[Posture].Statuses) : TConstArrayView<FVeyraContentId>();
}

void AVeyraCompanion::KeepHeldStatuses()
{
	if (!HasAuthority() || !IsAlive() || bBanished)
	{
		return;
	}
	for (const FVeyraContentId& Id : HeldStatuses())
	{
		if (VeyraCombat::HasStatusFrom(this, Id, *AbilitySystem))
		{
			continue;
		}
		if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(Id))
		{
			VeyraCombat::ApplyStatus(*AbilitySystem, *AbilitySystem, Status.GetValue());
		}
	}
}

void AVeyraCompanion::DropHeldStatuses()
{
	for (const FVeyraContentId& Id : HeldStatuses())
	{
		VeyraCombat::RemoveStatusFrom(*AbilitySystem, Id, *AbilitySystem);
	}
}

void AVeyraCompanion::FaceAnchor()
{
	SetActorRotation(FRotator(0.0, AnchorFacing.Rotation().Yaw, 0.0));
}

void AVeyraCompanion::SetChained(bool bInChained)
{
	if (bChained != bInChained)
	{
		bChained = bInChained;
		MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraCompanion, bChained, this);
	}
}

const FVeyraCompanionTuning* AVeyraCompanion::GetDefinition() const
{
	return UVeyraAbilitiesTuningSubsystem::FindCompanion(Definition);
}

bool AVeyraCompanion::IsAlive() const
{
	return Life && Life->IsAlive();
}

UVeyraMovementComponent* AVeyraCompanion::GetVeyraMovement() const
{
	return CastChecked<UVeyraMovementComponent>(GetCharacterMovement());
}

void AVeyraCompanion::OnRep_Definition()
{
	ApplyBody();
}

void AVeyraCompanion::OnRep_Banished()
{
	ApplyBanished();
}

void AVeyraCompanion::ApplyBody()
{
	if (const FVeyraCompanionTuning* Tuning = GetDefinition())
	{
		GetCapsuleComponent()->SetCapsuleSize(static_cast<float>(Tuning->CapsuleRadius), static_cast<float>(Tuning->CapsuleHalfHeight));
	}
}

void AVeyraCompanion::ApplyBanished()
{
	SetActorHiddenInGame(bBanished);
	SetActorEnableCollision(!bBanished);
}
