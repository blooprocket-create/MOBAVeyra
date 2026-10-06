// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "AbilitySystemGlobals.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Companions/VeyraCompanion.h"
#include "Companions/VeyraCompanionSubsystem.h"
#include "Cues/VeyraCombatCueSubsystem.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Movement/VeyraDrawnBody.h"
#include "NiagaraComponent.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Greybox/VeyraGreyboxSubsystem.h"
#include "Greybox/VeyraVanguardAnimInstance.h"
#include "Greybox/VeyraVanguardArtSet.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "VeyraPlayerController.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
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
		/** How far an easing mesh trails its capsule in the fixture, in centimetres. */
		static constexpr double EasingOffset = 30.0;
		/** The Level a fixture Vanguard rises to. */
		static constexpr double RisenLevel = 4.0;

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

		/** Every body the set holds, labelled: each Vanguard's and companion's own, and those it wears while it holds a status. */
		static TArray<TPair<FString, const FVeyraVanguardBody*>> EveryBody()
		{
			TArray<TPair<FString, const FVeyraVanguardBody*>> Bodies;
			for (const TMap<FName, FVeyraVanguardArt>* Set : { &ArtSet().Art, &ArtSet().CompanionArt })
			{
				for (const TPair<FName, FVeyraVanguardArt>& Entry : *Set)
				{
					Bodies.Emplace(Entry.Key.ToString(), &Entry.Value);
					for (const TPair<FName, FVeyraVanguardBody>& Status : Entry.Value.StatusBodies)
					{
						Bodies.Emplace(Entry.Key.ToString() + TEXT(" while ") + Status.Key.ToString(), &Status.Value);
					}
				}
			}
			return Bodies;
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
				// A body it wears while it holds a status is for a status its abilities give, or for a stance one of them
				// takes.
				for (const TPair<FName, FVeyraVanguardBody>& Status : Entry.Value.StatusBodies)
				{
					const TOptional<FVeyraContentId> StatusId = FVeyraContentId::FromText(Status.Key.ToString());
					ASSERT_THAT(IsTrue(StatusId && (UVeyraAbilitiesTuningSubsystem::FindStatus(*StatusId).IsSet() || UVeyraAbilitiesTuningSubsystem::FindStance(*StatusId)),
						*Status.Key.ToString()));
				}
				// Each of its bodies fitted to its capsule, which stays its only collision and movement: its own to the
				// capsule as defined, one it wears while a status grows its body to the capsule as grown.
				const double CapsuleHeight = Vanguard->Body.CapsuleHalfHeight * 2.0;
				const double OwnHeight = Entry.Value.Mesh->GetBounds().BoxExtent.Z * 2.0;
				ASSERT_THAT(IsTrue(OwnHeight >= CapsuleHeight * LeastHeightShare && OwnHeight <= CapsuleHeight * MostHeightShare, *Entry.Key.ToString()));
				for (const TPair<FName, FVeyraVanguardBody>& Status : Entry.Value.StatusBodies)
				{
					// A stance grows no body.
					const TOptional<FVeyraStatusSpec> Spec = UVeyraAbilitiesTuningSubsystem::FindStatus(FVeyraContentId::FromText(Status.Key.ToString()).GetValue());
					const double Grown = Spec && Spec->Kind == EVeyraStatusKind::BodyScale ? Spec->Magnitude : 1.0;
					const double Height = Status.Value.Mesh->GetBounds().BoxExtent.Z * 2.0;
					ASSERT_THAT(IsTrue(Height >= CapsuleHeight * LeastHeightShare && Height <= CapsuleHeight * Grown * MostHeightShare, *Status.Key.ToString()));
				}
			}
		}

		TEST_METHOD(EveryBodyFacesForwardAndStandsOnItsFeet)
		{
			for (const TPair<FString, const FVeyraVanguardBody*>& Body : EveryBody())
			{
				const USkeletalMesh& Mesh = *Body.Value->Mesh;
				const FString& Id = Body.Key;
				// Unreal's +X ahead and +Y to the right: its left shoulder (or foreleg) on the left, since hands on one weapon
				// cross, its head above or ahead of its middle, and the knees of a body that has them ahead of its ankles.
				ASSERT_THAT(IsTrue(BoneAt(Mesh, TEXT("upperarm_l")).Y < 0.0 && BoneAt(Mesh, TEXT("upperarm_r")).Y > 0.0, *Id));
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

		TEST_METHOD(ItsDrawnBodyHangsFromTheMeshItsMovementEases)
		{
			AVeyraVanguardCharacter& Unit = SpawnPlaying(DressedId());
			UVeyraGreyboxSubsystem& Presentation = RefreshedGreybox();
			const USkeletalMeshComponent* Skin = Presentation.FindSkin(Unit);
			ASSERT_THAT(IsNotNull(Skin));
			// Skin and disc hang from the character's mesh, which its movement eases after each update from the server.
			ASSERT_THAT(IsTrue(Skin->GetAttachParent() == Unit.GetMesh(), TEXT("the skin hangs from the eased mesh")));
			ASSERT_THAT(IsTrue(Presentation.FindBody(Unit)->GetAttachParent() == Unit.GetMesh(), TEXT("and so does its disc")));
			const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
			const UCharacterMovementComponent& Movement = *Unit.GetCharacterMovement();
			ASSERT_THAT(IsTrue(Movement.NetworkSmoothingMode == ENetworkSmoothingMode::Exponential));
			ASSERT_THAT(IsNear(Movement.NetworkSimulatedSmoothLocationTime, Settings.VanguardEaseLocationSeconds, KINDA_SMALL_NUMBER));
			ASSERT_THAT(IsNear(Movement.NetworkSimulatedSmoothRotationTime, Settings.VanguardEaseRotationSeconds, KINDA_SMALL_NUMBER));
			// At rest it is drawn where it stands; while the mesh eases, the body and the place drawn over it go with the mesh.
			ASSERT_THAT(IsTrue(VeyraDrawnBody::LocationOf(Unit).Equals(Unit.GetActorLocation(), Tolerance)));
			const FVector Before = Skin->GetComponentLocation();
			const FVector Easing(EasingOffset, 0.0, 0.0);
			Unit.GetMesh()->SetRelativeLocation(Unit.GetBaseTranslationOffset() + Easing);
			ASSERT_THAT(IsTrue(Skin->GetComponentLocation().Equals(Before + Easing, Tolerance), TEXT("the skin goes with the mesh")));
			ASSERT_THAT(IsTrue(VeyraDrawnBody::LocationOf(Unit).Equals(Unit.GetActorLocation() + Easing, Tolerance), TEXT("and so does the drawn place")));
		}

		TEST_METHOD(AVanguardWearsAStatusBodyOnlyWhileItHoldsTheStatus)
		{
			// The first Vanguard (by ID) with a body for a status, such as a rider and its ride (a stance is no status).
			const auto IsStatus = [](FName Key) {
				const TOptional<FVeyraContentId> Id = FVeyraContentId::FromText(Key.ToString());
				return Id && UVeyraAbilitiesTuningSubsystem::FindStatus(*Id).IsSet();
			};
			TArray<FName> Ids;
			ArtSet().Art.GetKeys(Ids);
			Ids.Sort(FNameLexicalLess());
			const FName* Id = Ids.FindByPredicate([&IsStatus](FName Candidate) {
				TArray<FName> Keys;
				ArtSet().Find(Candidate)->StatusBodies.GetKeys(Keys);
				return Keys.ContainsByPredicate(IsStatus);
			});
			ASSERT_THAT(IsNotNull(Id, TEXT("the committed art holds a status body")));
			const FVeyraVanguardArt& Art = *ArtSet().Find(*Id);
			TArray<FName> Statuses;
			Art.StatusBodies.GetKeys(Statuses);
			Statuses.RemoveAll([&IsStatus](FName Key) { return !IsStatus(Key); });
			Statuses.Sort(FNameLexicalLess());
			const FVeyraContentId Status = FVeyraContentId::FromText(Statuses[0].ToString()).GetValue();
			AVeyraVanguardCharacter& Unit = SpawnPlaying(*Id);
			UAbilitySystemComponent* Abilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
			ASSERT_THAT(IsNotNull(Abilities));
			UVeyraGreyboxSubsystem& Presentation = RefreshedGreybox();
			const USkeletalMeshComponent* Skin = Presentation.FindSkin(Unit);
			ASSERT_THAT(IsNotNull(Skin));
			ASSERT_THAT(IsTrue(Skin->GetSkeletalMeshAsset() == Art.Mesh, TEXT("its own body first")));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Abilities, *Abilities, UVeyraAbilitiesTuningSubsystem::FindStatus(Status).GetValue())));
			RefreshedGreybox();
			ASSERT_THAT(IsTrue(Skin->GetSkeletalMeshAsset() == Art.StatusBodies[Statuses[0]].Mesh, TEXT("the status's body while it holds the status")));
			const UVeyraVanguardAnimInstance* Animation = Cast<UVeyraVanguardAnimInstance>(Skin->GetAnimInstance());
			ASSERT_THAT(IsTrue(Animation && Animation->GetClip(EVeyraVanguardClip::Run) == Art.StatusBodies[Statuses[0]].Find(EVeyraVanguardClip::Run),
				TEXT("animated as that body")));
			ASSERT_THAT(IsTrue(VeyraCombat::RemoveStatus(*Abilities, Status)));
			RefreshedGreybox();
			ASSERT_THAT(IsTrue(Skin->GetSkeletalMeshAsset() == Art.Mesh, TEXT("its own body again once the status ends")));
		}

		TEST_METHOD(AVanguardWearsAStanceBodyOnlyWhileItsSlotsHoldTheStance)
		{
			// The first Vanguard (by ID) with a body for a stance, as Angeru's Blade House.
			TArray<FName> Ids;
			ArtSet().Art.GetKeys(Ids);
			Ids.Sort(FNameLexicalLess());
			FName Stance;
			const FName* Id = Ids.FindByPredicate([&Stance](FName Candidate) {
				for (const TPair<FName, FVeyraVanguardBody>& Status : ArtSet().Find(Candidate)->StatusBodies)
				{
					const TOptional<FVeyraContentId> Key = FVeyraContentId::FromText(Status.Key.ToString());
					if (Key && UVeyraAbilitiesTuningSubsystem::FindStance(*Key))
					{
						Stance = Status.Key;
						return true;
					}
				}
				return false;
			});
			ASSERT_THAT(IsNotNull(Id, TEXT("the committed art holds a stance body")));
			const FVeyraVanguardArt& Art = *ArtSet().Find(*Id);
			AVeyraVanguardCharacter& Unit = SpawnPlaying(*Id);
			UVeyraAbilityLoadoutComponent* Loadout = Unit.GetPlayerState()->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
			ASSERT_THAT(IsNotNull(Loadout));
			UVeyraGreyboxSubsystem& Presentation = RefreshedGreybox();
			const USkeletalMeshComponent* Skin = Presentation.FindSkin(Unit);
			ASSERT_THAT(IsNotNull(Skin));
			ASSERT_THAT(IsTrue(Skin->GetSkeletalMeshAsset() == Art.Mesh, TEXT("its own body in its own set")));
			Loadout->SetStance(FVeyraContentId::FromText(Stance.ToString()).GetValue());
			RefreshedGreybox();
			ASSERT_THAT(IsTrue(Skin->GetSkeletalMeshAsset() == Art.StatusBodies[Stance].Mesh, TEXT("the stance's body while its set is in the slots")));
			Loadout->SetStance(FVeyraContentId());
			RefreshedGreybox();
			ASSERT_THAT(IsTrue(Skin->GetSkeletalMeshAsset() == Art.Mesh, TEXT("its own body again in its own set")));
		}

		TEST_METHOD(OfTheStatusesItHoldsTheHighestPriorityBodyWinsThenTheFirstById)
		{
			// A body it holds all the while in some ground, and a brief burst's body above it.
			FVeyraVanguardArt Art;
			const FName Ground(TEXT("a_ground")), Burst(TEXT("b_burst")), Other(TEXT("c_other"));
			Art.StatusBodies.Add(Ground).RunStride = 1.0f;
			FVeyraVanguardBody& BurstBody = Art.StatusBodies.Add(Burst);
			BurstBody.RunStride = 2.0f;
			BurstBody.Priority = 1;
			Art.StatusBodies.Add(Other).RunStride = 3.0f;
			const auto Holding = [](TArray<FName> Held) {
				return [Held = MoveTemp(Held)](FName Status) { return Held.Contains(Status); };
			};
			ASSERT_THAT(IsTrue(&Art.BodyFor(Holding({ Ground, Burst })) == &Art.StatusBodies[Burst], TEXT("the burst over the ground")));
			ASSERT_THAT(IsTrue(&Art.BodyFor(Holding({ Ground, Other })) == &Art.StatusBodies[Ground], TEXT("ties go by status ID")));
			ASSERT_THAT(IsTrue(&Art.BodyFor(Holding({ Other })) == &Art.StatusBodies[Other]));
			ASSERT_THAT(IsTrue(&Art.BodyFor(Holding({})) == &Art, TEXT("its own body when it holds none")));
		}

		TEST_METHOD(ACompanionWearsItsBodyAndItsStatusBodyWhileItHoldsTheStatus)
		{
			// The first companion (by ID) with art, summoned by a Vanguard, wears its own generated body: the pair's other half.
			TArray<FName> Ids;
			ArtSet().CompanionArt.GetKeys(Ids);
			Ids.Sort(FNameLexicalLess());
			ASSERT_THAT(IsFalse(Ids.IsEmpty(), TEXT("the committed art dresses a companion")));
			const FVeyraVanguardArt& Art = *ArtSet().FindCompanion(Ids[0]);
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Summoner = World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			UVeyraCompanionSubsystem* Keeper = Spawner.GetWorld().GetSubsystem<UVeyraCompanionSubsystem>();
			ASSERT_THAT(IsTrue(Keeper && Keeper->Summon(*Summoner.GetAbilitySystemComponent(), FVeyraContentId::FromText(Ids[0].ToString()).GetValue())));
			AVeyraCompanion* Companion = Keeper->Find(*Summoner.GetAbilitySystemComponent());
			ASSERT_THAT(IsNotNull(Companion));
			UVeyraGreyboxSubsystem& Presentation = RefreshedGreybox();
			const USkeletalMeshComponent* Skin = Presentation.FindSkin(*Companion);
			ASSERT_THAT(IsNotNull(Skin));
			ASSERT_THAT(IsTrue(Skin->GetSkeletalMeshAsset() == Art.Mesh));
			ASSERT_THAT(IsTrue(Skin->GetCollisionEnabled() == ECollisionEnabled::NoCollision, TEXT("visual only")));
			// What its body is made of where no mesh shows it (smoke) pours off each of its bones, on its body, at the size
			// of the body it pours from.
			const FNiagaraVariable Scale(FNiagaraTypeDefinition::GetFloatDef(), *(TEXT("User.") + GetDefault<UVeyraGreyboxSettings>()->EffectScaleParameter.ToString()));
			const auto PoursFrom = [&Presentation, &Scale, Companion, Skin](const FVeyraVanguardBody& Worn) {
				const TArray<UNiagaraComponent*> Effects = Presentation.FindBodyEffects(*Companion);
				if (Effects.Num() != Worn.EffectBones.Num())
				{
					return false;
				}
				for (int32 Index = 0; Index < Effects.Num(); ++Index)
				{
					if (Effects[Index]->GetAsset() != Worn.Effect || Effects[Index]->GetAttachParent() != Skin
						|| Effects[Index]->GetAttachSocketName() != Worn.EffectBones[Index]
						|| Effects[Index]->GetOverrideParameters().GetParameterValueOrDefault(Scale, 0.0f) != Worn.EffectScale)
					{
						return false;
					}
				}
				return true;
			};
			ASSERT_THAT(IsTrue(PoursFrom(Art), TEXT("its own body's effect, one per bone")));
			// A status it holds (its true form, say) dresses it in that body while it lasts.
			if (Art.StatusBodies.IsEmpty())
			{
				return;
			}
			TArray<FName> Statuses;
			Art.StatusBodies.GetKeys(Statuses);
			Statuses.Sort(FNameLexicalLess());
			const FVeyraContentId Status = FVeyraContentId::FromText(Statuses[0].ToString()).GetValue();
			UAbilitySystemComponent& Abilities = *Companion->GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Abilities, Abilities, UVeyraAbilitiesTuningSubsystem::FindStatus(Status).GetValue())));
			RefreshedGreybox();
			ASSERT_THAT(IsTrue(Skin->GetSkeletalMeshAsset() == Art.StatusBodies[Statuses[0]].Mesh, TEXT("the status's body while it holds it")));
			ASSERT_THAT(IsTrue(PoursFrom(Art.StatusBodies[Statuses[0]]), TEXT("and what that body pours")));
			ASSERT_THAT(IsTrue(VeyraCombat::RemoveStatus(Abilities, Status)));
			RefreshedGreybox();
			ASSERT_THAT(IsTrue(Skin->GetSkeletalMeshAsset() == Art.Mesh));
			ASSERT_THAT(IsTrue(PoursFrom(Art)));
		}

		TEST_METHOD(OnlyThePlayersOwnVanguardsLevelUpIsTheirs)
		{
			// As in a match: the local controller possesses nothing; its participant's Vanguard is its own (ADR-006 §6).
			AVeyraVanguardCharacter& Own = SpawnPlaying(DressedId());
			AVeyraVanguardCharacter& Other = SpawnPlaying(DressedId());
			AVeyraPlayerController& Local = Spawner.SpawnActor<AVeyraPlayerController>();
			Local.PlayerState = Own.GetPlayerState();
			ASSERT_THAT(IsTrue(Local.GetPawn() == nullptr && Local.GetVanguard() == &Own));
			UVeyraGreyboxSubsystem& Presentation = RefreshedGreybox();
			UVeyraCombatCueSubsystem* Cues = Spawner.GetWorld().GetSubsystem<UVeyraCombatCueSubsystem>();
			FVeyraCombatCue LevelUp;
			LevelUp.Kind = EVeyraCombatCueKind::LevelUp;
			LevelUp.Unit = &Other;
			LevelUp.Amount = RisenLevel;
			Cues->OnCue.Broadcast(LevelUp);
			ASSERT_THAT(IsFalse(Presentation.GetOwnLevelUp().IsSet(), TEXT("another Vanguard's level-up is only its burst")));
			LevelUp.Unit = &Own;
			Cues->OnCue.Broadcast(LevelUp);
			ASSERT_THAT(IsTrue(Presentation.GetOwnLevelUp().IsSet() && Presentation.GetOwnLevelUp()->Level == static_cast<int32>(RisenLevel),
				TEXT("the player's own is announced")));
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
