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
#include "Statuses/VeyraStatusComponent.h"
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

void VeyraVanguardSkin::Dress(USkeletalMeshComponent& Skin, const FVeyraVanguardBody& Body, const FVeyraVanguardAnimShape& Shape)
{
	if (!Body.Mesh || Skin.GetSkeletalMeshAsset() == Body.Mesh)
	{
		return;
	}
	Skin.SetSkeletalMeshAsset(Body.Mesh);
	Skin.SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Skin.SetAnimInstanceClass(UVeyraVanguardAnimInstance::StaticClass());
	if (UVeyraVanguardAnimInstance* Animation = Cast<UVeyraVanguardAnimInstance>(Skin.GetAnimInstance()))
	{
		Animation->Configure(Body, Shape);
	}
}

const FVeyraVanguardBody& VeyraVanguardSkin::BodyOf(const APawn& Unit, const FVeyraVanguardArt& Art)
{
	// A Vanguard's statuses are its participant's, and their ledger reaches every machine for presentation.
	const APlayerState* Participant = Unit.GetPlayerState();
	const UVeyraStatusComponent* Statuses = Participant ? Participant->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
	if (!Statuses || Art.StatusBodies.IsEmpty())
	{
		return Art;
	}
	// A cooked build keeps one spelling per name, so status IDs compare ignoring case.
	return Art.BodyFor([Statuses](FName Status) {
		const FString Wanted = Status.ToString();
		return Statuses->GetLedger().Entries.ContainsByPredicate([&Wanted](const FVeyraStatusEntry& Entry) {
			return Entry.Id.ToString().Equals(Wanted, ESearchCase::IgnoreCase);
		});
	});
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

FVeyraVanguardAnimInputs VeyraVanguardSkin::InputsOf(const APawn& Unit, EVeyraTeam Viewer, double ServerNow)
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
		Inputs.AttackWindupSecondsLeft = Inputs.bAttackWindingUp ? static_cast<float>(FMath::Max(0.0, Sighting->AttackPhaseEndsAt - ServerNow)) : 0.0f;
		Inputs.bCastHeld = Sighting->CastPhase == EVeyraCastPhase::Windup || Sighting->CastPhase == EVeyraCastPhase::Channel;
	}
	return Inputs;
}
