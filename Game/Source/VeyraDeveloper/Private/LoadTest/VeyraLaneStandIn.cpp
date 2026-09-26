// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "LoadTest/VeyraLaneStandIn.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	// Load-test fixture values: a small unit's capsule, and how close to a lane end counts as there.
	constexpr float CapsuleRadius = 30.0f;
	constexpr float CapsuleHalfHeight = 60.0f;
	constexpr double ArrivalDistance = 50.0;
}

AVeyraLaneStandIn::AVeyraLaneStandIn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
	// Stand-ins walk through each other and through Vanguards, so a crowded lane never jams.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	bUseControllerRotationYaw = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	// No controller: the server moves it directly.
	Movement->bRunPhysicsWithNoController = true;
}

void AVeyraLaneStandIn::Walk(const FVector& From, const FVector& To, float Speed)
{
	LaneFrom = From;
	LaneTo = To;
	bTowardTo = true;
	bWalking = true;
	GetCharacterMovement()->MaxWalkSpeed = Speed;
}

void AVeyraLaneStandIn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !bWalking)
	{
		return;
	}
	const FVector Target = bTowardTo ? LaneTo : LaneFrom;
	const FVector ToTarget = Target - GetActorLocation();
	if (ToTarget.Size2D() <= ArrivalDistance)
	{
		bTowardTo = !bTowardTo;
		return;
	}
	AddMovementInput(ToTarget.GetSafeNormal2D());
}
