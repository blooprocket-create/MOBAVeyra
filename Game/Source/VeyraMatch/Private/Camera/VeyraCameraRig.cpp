// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Camera/VeyraCameraRig.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/VeyraCameraSettings.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"

AVeyraCameraRig::AVeyraCameraRig()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetCanBeDamaged(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	// The same fixed top-down view the Vanguard's arm gives, turning with nothing.
	Arm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Arm"));
	Arm->SetupAttachment(Root);
	Arm->SetUsingAbsoluteRotation(true);
	Arm->bDoCollisionTest = false;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Arm, USpringArmComponent::SocketName);
}

void AVeyraCameraRig::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	const UVeyraCameraSettings& View = *GetDefault<UVeyraCameraSettings>();
	Arm->TargetArmLength = View.Distance;
	Arm->SetWorldRotation(FRotator(View.PitchDegrees, 0.0, 0.0));
	Mode = View.DefaultMode;
}

void AVeyraCameraRig::Step(const FVeyraCameraInput& Input, double DeltaSeconds)
{
	const UVeyraCameraSettings& View = *GetDefault<UVeyraCameraSettings>();
	FVeyraCameraLimits Limits;
	Limits.PanSpeed = View.PanSpeed;
	Limits.SemiLockedMaxOffset = View.SemiLockedMaxOffset;
	// The battleground's floor bounds the view.
	Limits.HalfExtent = UVeyraWorldTuningSubsystem::Get().Layout.HalfExtent;
	FVeyraCameraState State;
	State.Focus = GetActorLocation();
	State.Offset = Offset;
	const FVeyraCameraState Next = VeyraCamera::Step(State, Mode, Input, Limits, DeltaSeconds);
	Offset = Next.Offset;
	SetActorLocation(Next.Focus);
}

void AVeyraCameraRig::LookAt(const FVector& Point)
{
	SetActorLocation(Point);
}
