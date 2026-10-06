// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Movement/VeyraDrawnBody.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

USceneComponent* VeyraDrawnBody::AnchorOf(const AActor& Unit)
{
	if (const ACharacter* Character = Cast<ACharacter>(&Unit))
	{
		if (USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			return Mesh;
		}
	}
	return Unit.GetRootComponent();
}

FVector VeyraDrawnBody::LocationOf(const AActor& Unit)
{
	const ACharacter* Character = Cast<ACharacter>(&Unit);
	const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return Unit.GetActorLocation();
	}
	// The mesh rests at its base offset from the capsule; whatever more it is offset by is the easing.
	return Mesh->GetComponentLocation() - Character->GetActorQuat().RotateVector(Character->GetBaseTranslationOffset());
}
