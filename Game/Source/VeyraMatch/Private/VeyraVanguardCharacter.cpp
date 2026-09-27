// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraVanguardCharacter.h"

#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/VeyraCameraSettings.h"
#include "Movement/VeyraMovementComponent.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraPlayerState.h"

AVeyraVanguardCharacter::AVeyraVanguardCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UVeyraMovementComponent>(ACharacter::CharacterMovementComponentName))
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

	const UVeyraCameraSettings& View = *GetDefault<UVeyraCameraSettings>();
	CameraArm->TargetArmLength = View.Distance;
	CameraArm->SetWorldRotation(FRotator(View.PitchDegrees, 0.0, 0.0));
}

UVeyraMovementComponent* AVeyraVanguardCharacter::GetVeyraMovement() const
{
	return CastChecked<UVeyraMovementComponent>(GetCharacterMovement());
}

void AVeyraVanguardCharacter::ApplyVanguardBody()
{
	const AVeyraPlayerState* Participant = GetPlayerState<AVeyraPlayerState>();
	const FVeyraVanguardDefinition* Definition =
		Participant && Participant->GetVanguardId().IsValid() ? UVeyraVanguardsTuningSubsystem::FindVanguard(Participant->GetVanguardId()) : nullptr;
	if (!Definition)
	{
		return;
	}
	const FVeyraVanguardBodyTuning& Body = Definition->Body;
	GetCapsuleComponent()->SetCapsuleSize(static_cast<float>(Body.CapsuleRadius), static_cast<float>(Body.CapsuleHalfHeight));
	GetCharacterMovement()->RotationRate = FRotator(0.0, Body.TurnRateDegreesPerSecond, 0.0);
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

	// Only the server moves Vanguards, so only its movement follows the participant.
	if (HasAuthority())
	{
		GetVeyraMovement()->BindCombatant(NewAbilitySystem);
	}
	ApplyVanguardBody();
}
