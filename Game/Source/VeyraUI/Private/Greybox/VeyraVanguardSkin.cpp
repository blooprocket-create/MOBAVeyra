// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraVanguardSkin.h"

#include "Components/SkeletalMeshComponent.h"
#include "Cues/VeyraCombatCues.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Greybox/VeyraVanguardAnimInstance.h"
#include "Greybox/VeyraVanguardArtSet.h"
#include "Recall/VeyraRecallComponent.h"
#include "Targeting/VeyraTargeting.h"

USkeletalMeshComponent* VeyraVanguardSkin::Attach(APawn& Unit)
{
	USceneComponent* Root = Unit.GetRootComponent();
	if (!Root)
	{
		return nullptr;
	}
	// Presentation only, as a body is.
	USkeletalMeshComponent* Skin = NewObject<USkeletalMeshComponent>(&Unit, NAME_None, RF_Transient);
	Skin->SetMobility(EComponentMobility::Movable);
	Skin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Skin->SetGenerateOverlapEvents(false);
	Skin->SetCanEverAffectNavigation(false);
	Skin->SetupAttachment(Root);
	float Radius = 0.0f;
	float HalfHeight = 0.0f;
	Unit.GetSimpleCollisionCylinder(Radius, HalfHeight);
	// Its root bone stands at the capsule's foot, facing the way the capsule faces.
	Skin->SetRelativeLocation(FVector(0.0, 0.0, -HalfHeight));
	Skin->RegisterComponent();
	return Skin;
}

void VeyraVanguardSkin::Dress(USkeletalMeshComponent& Skin, const FVeyraVanguardArt& Art, const FVeyraVanguardAnimShape& Shape)
{
	if (!Art.Mesh || Skin.GetSkeletalMeshAsset() == Art.Mesh)
	{
		return;
	}
	Skin.SetSkeletalMeshAsset(Art.Mesh);
	Skin.SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Skin.SetAnimInstanceClass(UVeyraVanguardAnimInstance::StaticClass());
	if (UVeyraVanguardAnimInstance* Animation = Cast<UVeyraVanguardAnimInstance>(Skin.GetAnimInstance()))
	{
		Animation->Configure(Art, Shape);
	}
}

FVeyraVanguardAnimShape VeyraVanguardSkin::ShapeOf(const UVeyraGreyboxSettings& Settings)
{
	FVeyraVanguardAnimShape Shape;
	Shape.BlendSeconds = Settings.VanguardBlendSeconds;
	Shape.RunBlendSpeed = Settings.VanguardRunBlendSpeed;
	Shape.MinPlayRate = Settings.VanguardMinPlayRate;
	Shape.MaxPlayRate = Settings.VanguardMaxPlayRate;
	return Shape;
}

FVeyraVanguardAnimInputs VeyraVanguardSkin::InputsOf(const APawn& Unit, EVeyraTeam Viewer)
{
	FVeyraVanguardAnimInputs Inputs;
	Inputs.GroundSpeed = static_cast<float>(Unit.GetVelocity().Size2D());
	Inputs.bAlive = VeyraTargeting::IsAlive(&Unit);
	// A Vanguard's Recall is its participant's (ADR-012 §8).
	const APlayerState* Participant = Unit.GetPlayerState();
	const UVeyraRecallComponent* Recall = Participant ? Participant->FindComponentByClass<UVeyraRecallComponent>() : nullptr;
	Inputs.bRecalling = Recall && Recall->IsRecalling();
	if (const TOptional<FVeyraUnitSighting> Sighting = VeyraCombatCues::Sight(Unit, Viewer))
	{
		Inputs.bAttackWindingUp = Sighting->AttackPhase == EVeyraAttackPhase::Windup;
		Inputs.bCastHeld = Sighting->CastPhase == EVeyraCastPhase::Windup || Sighting->CastPhase == EVeyraCastPhase::Channel;
	}
	return Inputs;
}
