// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraVanguardSkin.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "Cues/VeyraCombatCues.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Greybox/VeyraVanguardAnimInstance.h"
#include "Greybox/VeyraVanguardArtSet.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Movement/VeyraDrawnBody.h"
#include "Recall/VeyraRecallComponent.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Targeting/VeyraTargeting.h"

USkeletalMeshComponent* VeyraVanguardSkin::Attach(APawn& Unit)
{
	// It hangs from where the body is drawn, which glides after the capsule on machines that only show it (ADR-065 §12).
	USceneComponent* Anchor = VeyraDrawnBody::AnchorOf(Unit);
	if (!Anchor)
	{
		return nullptr;
	}
	// Presentation only, as a body is.
	USkeletalMeshComponent* Skin = NewObject<USkeletalMeshComponent>(&Unit, NAME_None, RF_Transient);
	Skin->SetMobility(EComponentMobility::Movable);
	Skin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Skin->SetGenerateOverlapEvents(false);
	Skin->SetCanEverAffectNavigation(false);
	Skin->SetupAttachment(Anchor);
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
	// A unit's statuses sit beside its ability system: a Vanguard's on its participant, a companion's on itself. Their
	// ledger reaches every machine for presentation.
	const UAbilitySystemComponent* Abilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
	const AActor* Holder = Abilities ? Abilities->GetOwner() : nullptr;
	const UVeyraStatusComponent* Statuses = Holder ? Holder->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
	// So does its stance: the stance ability whose set its slots hold, which every machine receives too (ADR-031 §3).
	const UVeyraAbilityLoadoutComponent* Loadout = Holder ? Holder->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
	if ((!Statuses && !Loadout) || Art.StatusBodies.IsEmpty())
	{
		return Art;
	}
	// A cooked build keeps one spelling per name, so IDs compare ignoring case.
	const FString Stance = Loadout && Loadout->GetStance().IsValid() ? Loadout->GetStance().ToString() : FString();
	return Art.BodyFor([Statuses, &Stance](FName Key) {
		const FString Wanted = Key.ToString();
		return (!Stance.IsEmpty() && Stance.Equals(Wanted, ESearchCase::IgnoreCase))
			|| (Statuses && Statuses->GetLedger().Entries.ContainsByPredicate([&Wanted](const FVeyraStatusEntry& Entry) {
				return Entry.Id.ToString().Equals(Wanted, ESearchCase::IgnoreCase);
			}));
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
		Inputs.CastAbility = Inputs.bCastHeld && Sighting->CastAbility.IsValid() ? FName(*Sighting->CastAbility.ToString()) : NAME_None;
		Inputs.CastWindupSecondsLeft = Sighting->CastPhase == EVeyraCastPhase::Windup
			? static_cast<float>(FMath::Max(0.0, Sighting->CastPhaseEndsAt - ServerNow)) : 0.0f;
	}
	return Inputs;
}
