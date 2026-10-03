// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Cues/VeyraCombatCueSubsystem.h"
#include "Engine/SkeletalMesh.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Greybox/VeyraGreyboxSubsystem.h"
#include "Greybox/VeyraVanguardAnimInstance.h"
#include "Greybox/VeyraVanguardArtSet.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"

namespace VeyraVanguardBodyTests
{
	using namespace VeyraAbilitiesTests;

	// Veyra.UI.VanguardBodies.*: the Vanguards' generated, animated bodies (ADR-064): the committed art set, the bodies'
	// facing and footing, and the grey-box presentation dressing a Vanguard in its body in a client world.
	TEST_CLASS(VanguardBodies, "Veyra.UI")
	{
		// Fixture values: centimetres of slack, how long a windup has left, how far a body's height may stray from its
		// capsule's (antlers, masts and hats stand above it), and how high a floating body may hover, as a share of its height.
		static constexpr double Tolerance = 1.0;
		static constexpr double WindupSeconds = 0.2;
		static constexpr double LeastHeightShare = 0.5;
		static constexpr double MostHeightShare = 1.4;
		static constexpr double MostHoverShare = 0.2;

		FActorTestSpawner Spawner;

		static const UVeyraVanguardArtSet& ArtSet()
		{
			return *GetDefault<UVeyraGreyboxSettings>()->VanguardArt.LoadSynchronous();
		}

		/** A Vanguard the art set dresses: the first by ID. */
		static FName DressedId()
		{
			TArray<FName> Ids;
			ArtSet().Art.GetKeys(Ids);
			Ids.Sort(FNameLexicalLess());
			return Ids.IsEmpty() ? NAME_None : Ids[0];
		}

		static FVector BoneAt(const USkeletalMesh& Mesh, FName Bone)
		{
			const FReferenceSkeleton& Reference = Mesh.GetRefSkeleton();
			return FAnimationRuntime::GetComponentSpaceTransformRefPose(Reference, Reference.FindBoneIndex(Bone)).GetLocation();
		}

		UVeyraGreyboxSubsystem& RefreshedGreybox()
		{
			UVeyraGreyboxSubsystem* Subsystem = Spawner.GetWorld().GetSubsystem<UVeyraGreyboxSubsystem>();
			check(Subsystem);
			Subsystem->Refresh();
			return *Subsystem;
		}

		/** A Vanguard on side A whose participant plays Id. */
		AVeyraVanguardCharacter& SpawnPlaying(FName Id)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Unit = World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Unit.GetPlayerState<AVeyraPlayerState>()->SetVanguardId(FVeyraContentId::FromText(Id.ToString()).GetValue());
			return Unit;
		}

		TEST_METHOD(TheArtSetIsCompleteAndDressesOnlyVanguards)
		{
			const UVeyraVanguardArtSet* Set = GetDefault<UVeyraGreyboxSettings>()->VanguardArt.LoadSynchronous();
			ASSERT_THAT(IsNotNull(Set));
			const TArray<FString> Problems = Set->Validate();
			ASSERT_THAT(IsTrue(Problems.IsEmpty(), Problems.IsEmpty() ? TEXT("") : *Problems[0]));
			ASSERT_THAT(IsFalse(Set->Art.IsEmpty()));
			for (const TPair<FName, FVeyraVanguardArt>& Entry : Set->Art)
			{
				const TOptional<FVeyraContentId> Id = FVeyraContentId::FromText(Entry.Key.ToString());
				const FVeyraVanguardDefinition* Vanguard = Id ? UVeyraVanguardsTuningSubsystem::FindVanguard(*Id) : nullptr;
				ASSERT_THAT(IsTrue(Vanguard != nullptr, *Entry.Key.ToString()));
				// Fitted to its capsule, which stays its only collision and movement.
				const double Height = Entry.Value.Mesh->GetBounds().BoxExtent.Z * 2.0;
				const double CapsuleHeight = Vanguard->Body.CapsuleHalfHeight * 2.0;
				ASSERT_THAT(IsTrue(Height >= CapsuleHeight * LeastHeightShare && Height <= CapsuleHeight * MostHeightShare, *Entry.Key.ToString()));
			}
		}

		TEST_METHOD(EveryBodyFacesForwardAndStandsOnItsFeet)
		{
			for (const TPair<FName, FVeyraVanguardArt>& Entry : ArtSet().Art)
			{
				const USkeletalMesh& Mesh = *Entry.Value.Mesh;
				const FString Id = Entry.Key.ToString();
				// Unreal's +X ahead and +Y to the right: its left hand (or forefoot) on the left, its head above or ahead of its
				// middle, and the knees of a body that has them ahead of its ankles.
				ASSERT_THAT(IsTrue(BoneAt(Mesh, TEXT("hand_l")).Y < 0.0 && BoneAt(Mesh, TEXT("hand_r")).Y > 0.0, *Id));
				const FVector Head = BoneAt(Mesh, TEXT("head"));
				const FVector Middle = BoneAt(Mesh, TEXT("pelvis"));
				ASSERT_THAT(IsTrue(Head.X + Head.Z > Middle.X + Middle.Z, *Id));
				if (Mesh.GetRefSkeleton().FindBoneIndex(TEXT("calf_l")) != INDEX_NONE)
				{
					ASSERT_THAT(IsTrue(BoneAt(Mesh, TEXT("calf_l")).X > BoneAt(Mesh, TEXT("foot_l")).X, *Id));
				}
				// Its root on the ground under it; nothing below the ground, and a floating body near it.
				ASSERT_THAT(IsTrue(BoneAt(Mesh, TEXT("root")).IsNearlyZero(Tolerance), *Id));
				const FBox Bounds = Mesh.GetBounds().GetBox();
				ASSERT_THAT(IsTrue(Bounds.Min.Z > -Tolerance * 2.0 && Bounds.Min.Z < Bounds.GetSize().Z * MostHoverShare, *Id));
			}
		}

		TEST_METHOD(AVanguardWithArtWearsItsAnimatedBodyOverADisc)
		{
			const FName Id = DressedId();
			AVeyraVanguardCharacter& Unit = SpawnPlaying(Id);
			UVeyraGreyboxSubsystem& Presentation = RefreshedGreybox();
			const USkeletalMeshComponent* Skin = Presentation.FindSkin(Unit);
			ASSERT_THAT(IsNotNull(Skin));
			ASSERT_THAT(IsTrue(Skin->GetSkeletalMeshAsset() == ArtSet().Find(Id)->Mesh));
			ASSERT_THAT(IsTrue(Skin->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !Skin->CanEverAffectNavigation(), TEXT("visual only")));
			const UVeyraVanguardAnimInstance* Animation = Cast<UVeyraVanguardAnimInstance>(Skin->GetAnimInstance());
			ASSERT_THAT(IsTrue(Animation && Animation->IsConfigured()));
			float Radius = 0.0f;
			float HalfHeight = 0.0f;
			Unit.GetSimpleCollisionCylinder(Radius, HalfHeight);
			ASSERT_THAT(IsNear(Skin->GetComponentLocation().Z, Unit.GetActorLocation().Z - HalfHeight, Tolerance, TEXT("standing at the capsule's foot")));
			// Its body, a disc under its feet, still shows its side and its status tint.
			const FBox Disc = Presentation.FindBody(Unit)->Bounds.GetBox();
			ASSERT_THAT(IsNear(Disc.GetSize().Z, static_cast<double>(GetDefault<UVeyraGreyboxSettings>()->VanguardFootDiscHeight), Tolerance));
			ASSERT_THAT(IsNear(Disc.Min.Z, Unit.GetActorLocation().Z - HalfHeight, Tolerance));
			ASSERT_THAT(IsNear(Disc.GetExtent().X, static_cast<double>(Radius), Tolerance));
		}

		TEST_METHOD(AVanguardWithoutArtKeepsItsBody)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Unit = World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			UVeyraGreyboxSubsystem& Presentation = RefreshedGreybox();
			ASSERT_THAT(IsNull(Presentation.FindSkin(Unit)));
			float Radius = 0.0f;
			float HalfHeight = 0.0f;
			Unit.GetSimpleCollisionCylinder(Radius, HalfHeight);
			ASSERT_THAT(IsNear(Presentation.FindBody(Unit)->Bounds.GetBox().GetSize().Z, HalfHeight * 2.0, Tolerance));
		}

		TEST_METHOD(ItsCuesPlayOnItsBodyAndAHitFlashesIt)
		{
			AVeyraVanguardCharacter& Unit = SpawnPlaying(DressedId());
			UVeyraGreyboxSubsystem& Presentation = RefreshedGreybox();
			USkeletalMeshComponent* Skin = Presentation.FindSkin(Unit);
			ASSERT_THAT(IsNotNull(Skin));
			const UVeyraVanguardAnimInstance& Animation = *CastChecked<UVeyraVanguardAnimInstance>(Skin->GetAnimInstance());
			UVeyraCombatCueSubsystem* Cues = Spawner.GetWorld().GetSubsystem<UVeyraCombatCueSubsystem>();
			FVeyraCombatCue Windup;
			Windup.Kind = EVeyraCombatCueKind::AttackWindup;
			Windup.Unit = &Unit;
			Windup.EndsAt = Presentation.GetServerNow() + WindupSeconds;
			Cues->OnCue.Broadcast(Windup);
			ASSERT_THAT(IsTrue(Animation.GetState().Current.Clip == EVeyraVanguardClip::AttackWindup));
			const float Expected = Animation.GetShape().Lengths.Of(EVeyraVanguardClip::AttackWindup) / static_cast<float>(WindupSeconds);
			ASSERT_THAT(IsNear(Animation.GetState().Current.Rate, FMath::Clamp(Expected, Animation.GetShape().MinPlayRate, Animation.GetShape().MaxPlayRate), 0.01f,
				TEXT("timed to end as the attack commits")));
			FVeyraCombatCue Hit;
			Hit.Kind = EVeyraCombatCueKind::Hit;
			Hit.Unit = &Unit;
			Cues->OnCue.Broadcast(Hit);
			RefreshedGreybox();
			ASSERT_THAT(IsTrue(Skin->GetOverlayMaterial() != nullptr, TEXT("the hit flash lies over the animated body")));
			ASSERT_THAT(IsTrue(Animation.GetState().Current.Clip == EVeyraVanguardClip::AttackWindup, TEXT("the flinch never cuts the attack short")));
		}
	};
}

#endif
