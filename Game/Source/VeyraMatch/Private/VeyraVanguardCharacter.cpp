// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraVanguardCharacter.h"

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraMobilitySet.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/VeyraCameraSettings.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"

AVeyraVanguardCharacter::AVeyraVanguardCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The GameMode creates and keeps each Vanguard's controller, so nothing spawns one automatically.
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	// A fixed top-down view that does not turn with the Vanguard.
	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(GetRootComponent());
	CameraArm->SetUsingAbsoluteRotation(true);
	CameraArm->bDoCollisionTest = false;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm, USpringArmComponent::SocketName);
}

void AVeyraVanguardCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	// Server and clients read the same tuning (its hash is checked on join), so both build the same
	// body.
	const FVeyraDeveloperLoadoutTuning& Loadout = UVeyraMatchTuningSubsystem::Get().DeveloperLoadout;
	GetCapsuleComponent()->SetCapsuleSize(static_cast<float>(Loadout.CapsuleRadius), static_cast<float>(Loadout.CapsuleHalfHeight));
	GetCharacterMovement()->RotationRate = FRotator(0.0, Loadout.TurnRateDegreesPerSecond, 0.0);

	const UVeyraCameraSettings& View = *GetDefault<UVeyraCameraSettings>();
	CameraArm->TargetArmLength = View.Distance;
	CameraArm->SetWorldRotation(FRotator(View.PitchDegrees, 0.0, 0.0));
}

UAbilitySystemComponent* AVeyraVanguardCharacter::GetAbilitySystemComponent() const
{
	const IAbilitySystemInterface* Participant = Cast<IAbilitySystemInterface>(GetPlayerState());
	return Participant ? Participant->GetAbilitySystemComponent() : nullptr;
}

EVeyraTeam AVeyraVanguardCharacter::GetVeyraTeam() const
{
	return VeyraTeams::TeamOf(GetPlayerState());
}

void AVeyraVanguardCharacter::OnPlayerStateChanged(APlayerState* NewPlayerState, APlayerState* OldPlayerState)
{
	Super::OnPlayerStateChanged(NewPlayerState, OldPlayerState);

	// This runs on the server when the GameMode hands the Vanguard its PlayerState, and on clients
	// when that replicates, so the avatar is correct everywhere.
	if (const IAbilitySystemInterface* OldOwner = Cast<IAbilitySystemInterface>(OldPlayerState))
	{
		UAbilitySystemComponent* OldAbilitySystem = OldOwner->GetAbilitySystemComponent();
		if (OldAbilitySystem && OldAbilitySystem->GetAvatarActor() == this)
		{
			OldAbilitySystem->InitAbilityActorInfo(OldPlayerState, OldPlayerState);
		}
	}

	UAbilitySystemComponent* NewAbilitySystem = nullptr;
	if (const IAbilitySystemInterface* NewOwner = Cast<IAbilitySystemInterface>(NewPlayerState))
	{
		NewAbilitySystem = NewOwner->GetAbilitySystemComponent();
		if (NewAbilitySystem)
		{
			NewAbilitySystem->InitAbilityActorInfo(NewPlayerState, this);
		}
	}

	if (HasAuthority())
	{
		FollowMoveSpeed(NewAbilitySystem);
	}
}

void AVeyraVanguardCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FollowMoveSpeed(nullptr);
	Super::EndPlay(EndPlayReason);
}

void AVeyraVanguardCharacter::FollowMoveSpeed(UAbilitySystemComponent* AbilitySystem)
{
	if (UAbilitySystemComponent* Followed = FollowedAbilitySystem.Get())
	{
		Followed->GetGameplayAttributeValueChangeDelegate(UVeyraMobilitySet::GetMoveSpeedAttribute()).Remove(MoveSpeedChangedHandle);
	}
	MoveSpeedChangedHandle.Reset();
	FollowedAbilitySystem = AbilitySystem;

	if (AbilitySystem)
	{
		// Only the server moves Vanguards, so only its movement component needs the speed.
		MoveSpeedChangedHandle = AbilitySystem->GetGameplayAttributeValueChangeDelegate(UVeyraMobilitySet::GetMoveSpeedAttribute())
			.AddUObject(this, &AVeyraVanguardCharacter::OnMoveSpeedChanged);
		GetCharacterMovement()->MaxWalkSpeed = AbilitySystem->GetNumericAttribute(UVeyraMobilitySet::GetMoveSpeedAttribute());
	}
}

void AVeyraVanguardCharacter::OnMoveSpeedChanged(const FOnAttributeChangeData& Change)
{
	GetCharacterMovement()->MaxWalkSpeed = Change.NewValue;
}
