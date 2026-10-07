// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraVanguardAnimInstance.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Greybox/VeyraLimbIK.h"
#include "Greybox/VeyraVanguardArtSet.h"

namespace
{
	constexpr int32 ClipCount = static_cast<int32>(EVeyraVanguardClip::None);

	/**
	 * The worker-thread half (ADR-064 §3): samples the clips the state names and blends them, Idle under Run, then
	 * each animation over them, on the upper body only while the body runs.
	 */
	struct FVeyraVanguardAnimProxy final : public FAnimInstanceProxy
	{
		explicit FVeyraVanguardAnimProxy(UAnimInstance* Instance)
			: FAnimInstanceProxy(Instance)
		{
		}

		virtual void PreEvaluateAnimation(UAnimInstance* InAnimInstance) override
		{
			FAnimInstanceProxy::PreEvaluateAnimation(InAnimInstance);
			// The game thread's latest, after this frame's update.
			const UVeyraVanguardAnimInstance& Instance = *CastChecked<UVeyraVanguardAnimInstance>(InAnimInstance);
			for (int32 Index = 0; Index < ClipCount; ++Index)
			{
				Clips[Index] = Instance.GetClip(static_cast<EVeyraVanguardClip>(Index));
			}
			UpperBodyBone = Instance.GetUpperBodyBone();
			State = Instance.GetState();
			Limbs = Instance.GetLimbFrame();
		}

		virtual bool Evaluate(FPoseContext& Output) override
		{
			const UAnimSequence* Idle = Clips[static_cast<int32>(EVeyraVanguardClip::Idle)];
			if (!Idle)
			{
				Output.ResetToRefPose();
				return true;
			}
			Sample(*Idle, State.IdlePosition, true, Output);
			if (const UAnimSequence* Run = Clips[static_cast<int32>(EVeyraVanguardClip::Run)]; Run && FAnimationRuntime::HasWeight(State.RunWeight))
			{
				FPoseContext Running(Output);
				Sample(*Run, State.RunPosition, true, Running);
				FAnimationPoseData Locomotion(Output);
				FAnimationRuntime::BlendTwoPosesTogetherInPlace(Locomotion, FAnimationPoseData(Running), 1.0f - State.RunWeight);
			}
			MarkUpperBody(Output.Pose);
			for (const FVeyraVanguardAnimSlot* Slot : { &State.Previous, &State.Current })
			{
				const UAnimSequence* Sequence = Slot->Clip == EVeyraVanguardClip::None ? nullptr : Clips[static_cast<int32>(Slot->Clip)];
				if (!Sequence || !FAnimationRuntime::HasWeight(Slot->Weight))
				{
					continue;
				}
				FPoseContext Action(Output);
				Sample(*Sequence, Slot->Position, VeyraVanguardAnim::Loops(Slot->Clip), Action);
				// The legs keep running under an attack, a cast or a hit.
				const float Lower = VeyraVanguardAnim::IsWholeBody(Slot->Clip) ? Slot->Weight : Slot->Weight * (1.0f - State.RunWeight);
				Weights.SetNumUninitialized(UpperBody.Num());
				for (int32 Bone = 0; Bone < UpperBody.Num(); ++Bone)
				{
					Weights[Bone] = UpperBody[Bone] ? Slot->Weight : Lower;
				}
				FPoseContext Blended(Output);
				FAnimationRuntime::BlendTwoPosesTogetherPerBone(Output.Pose, Action.Pose, Weights, Blended.Pose);
				Output.Pose.CopyBonesFrom(Blended.Pose);
			}
			SolveLimbs(Output.Pose);
			return true;
		}

	private:
		static void Sample(const UAnimSequence& Sequence, float Time, bool bLooping, FPoseContext& Into)
		{
			FAnimationPoseData Pose(Into);
			Sequence.GetAnimationPose(Pose, FAnimExtractContext(static_cast<double>(Time), false, FDeltaTimeRecord(), bLooping));
		}

		/** Which of the pose's bones are the upper body: the upper-body bone and every bone below it. */
		void MarkUpperBody(const FCompactPose& Pose)
		{
			const FBoneContainer& Bones = Pose.GetBoneContainer();
			const FReferenceSkeleton& Reference = Bones.GetReferenceSkeleton();
			const int32 Upper = Reference.FindBoneIndex(UpperBodyBone);
			UpperBody.SetNumUninitialized(Pose.GetNumBones());
			for (const FCompactPoseBoneIndex Bone : Pose.ForEachBoneIndex())
			{
				const int32 MeshBone = Bones.MakeMeshPoseIndex(Bone).GetInt();
				UpperBody[Bone.GetInt()] = Upper != INDEX_NONE && (MeshBone == Upper || Reference.BoneIsChildOf(MeshBone, Upper));
			}
		}

		/**
		 * The limbs that hold to something fixed (ADR-069), over the blended clips: each planted foot to its ground (the
		 * pelvis lowering for the lower foot), and the off hand to its weapon hand. Swinging limbs stay as the clips move
		 * them: a foot's hold follows how planted it is.
		 */
		void SolveLimbs(FCompactPose& Pose) const
		{
			const bool bFeet = Limbs.Feet.Num() > 0 && Limbs.FootOffsets.Num() == Limbs.Feet.Num() && Limbs.FootWeight > 0.0f;
			const bool bHand = Limbs.OffHand.IsSet() && Limbs.OffHandWeight > 0.0f;
			if (!bFeet && !bHand)
			{
				return;
			}
			const FBoneContainer& Bones = Pose.GetBoneContainer();
			const auto Index = [&Bones](FName Name) {
				const int32 Mesh = Bones.GetPoseBoneIndexForBoneName(Name);
				return Mesh == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Mesh));
			};
			FCSPose<FCompactPose> Space;
			Space.InitPose(Pose);
			if (bFeet)
			{
				TArray<float> Planted;
				for (int32 Foot = 0; Foot < Limbs.Feet.Num(); ++Foot)
				{
					const FCompactPoseBoneIndex End = Index(Limbs.Feet[Foot].End);
					const float Lift = End.IsValid() ? Space.GetComponentSpaceTransform(End).GetLocation().Z - Limbs.FootRestHeights[Foot] : 0.0f;
					Planted.Add(VeyraLimbIK::PlantWeight(Lift, Limbs.PlantFade) * Limbs.FootWeight);
				}
				// The pelvis lowers so the lower foot reaches its ground (two feet; more legs keep their height).
				float Drop = 0.0f;
				const FCompactPoseBoneIndex Pelvis = Index(Limbs.Pelvis);
				if (Planted.Num() == 2 && Pelvis.IsValid())
				{
					Drop = VeyraLimbIK::PelvisDrop(Limbs.FootOffsets[0], Planted[0], Limbs.FootOffsets[1], Planted[1], Limbs.MaxPelvisDrop);
					FTransform Hips = Space.GetComponentSpaceTransform(Pelvis);
					Hips.AddToTranslation(FVector(0.0, 0.0, Drop));
					Space.SetComponentSpaceTransform(Pelvis, Hips);
				}
				for (int32 Foot = 0; Foot < Limbs.Feet.Num(); ++Foot)
				{
					const FVector Lift(0.0, 0.0, Limbs.FootOffsets[Foot] * Planted[Foot] - Drop);
					const FQuat Tilt = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(FVector::UpVector, Limbs.FootNormals[Foot]),
						Limbs.GroundTilt * Planted[Foot]);
					Reach(Space, Index, Limbs.Feet[Foot], [&](const FTransform& End) {
						return FTransform(Tilt * End.GetRotation(), End.GetLocation() + Lift);
					}, 1.0f);
				}
			}
			if (bHand)
			{
				const FCompactPoseBoneIndex Anchor = Index(Limbs.OffHandAnchor);
				const FCompactPoseBoneIndex Hand = Index(Limbs.OffHand.End);
				if (Anchor.IsValid() && Hand.IsValid())
				{
					const FTransform Hold = Limbs.OffHandFromAnchor * Space.GetComponentSpaceTransform(Anchor);
					// Held as near as the clip keeps it: a drift is corrected, a gesture away from the grip let through.
					const float Away = static_cast<float>(FVector::Dist(Space.GetComponentSpaceTransform(Hand).GetLocation(), Hold.GetLocation()));
					Reach(Space, Index, Limbs.OffHand, [&Hold](const FTransform&) { return Hold; },
						Limbs.OffHandWeight * VeyraLimbIK::HoldWeight(Away, Limbs.OffHandRelease));
				}
			}
			FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(Space), Pose);
		}

		/** Solves Chain so its end reaches Goal(its end as the clips leave it), Weight of the way, bending as it bent. */
		template <typename IndexOf, typename GoalOf>
		void Reach(FCSPose<FCompactPose>& Space, const IndexOf& Index, const FVeyraLimbChain& Chain, const GoalOf& Goal, float Weight) const
		{
			const FCompactPoseBoneIndex Root = Index(Chain.Root), Joint = Index(Chain.Joint), End = Index(Chain.End);
			if (!Root.IsValid() || !Joint.IsValid() || !End.IsValid())
			{
				return;
			}
			const FTransform EndNow = Space.GetComponentSpaceTransform(End);
			const FTransform Wanted = Goal(EndNow);
			VeyraLimbIK::FChain Now;
			Now.Root = Space.GetComponentSpaceTransform(Root).GetLocation();
			Now.Joint = Space.GetComponentSpaceTransform(Joint).GetLocation();
			Now.End = EndNow.GetLocation();
			// The pole keeps the joint in the plane it bends in now, so a knee or elbow never flips.
			const FVector Middle = (Now.Root + Now.End) * 0.5;
			const FVector Pole = Now.Joint + (Now.Joint - Middle).GetSafeNormal() * 50.0;
			const FVector Target = FMath::Lerp(Now.End, Wanted.GetLocation(), Weight);
			const VeyraLimbIK::FChain Solved = VeyraLimbIK::Solve(Now, Target, Pole, Limbs.MaxStretch);
			FTransform RootT = Space.GetComponentSpaceTransform(Root);
			RootT.SetRotation(FQuat::FindBetweenVectors(Now.Joint - Now.Root, Solved.Joint - Solved.Root) * RootT.GetRotation());
			Space.SetComponentSpaceTransform(Root, RootT);
			FTransform JointT = Space.GetComponentSpaceTransform(Joint);
			const FVector EndAfterRoot = Space.GetComponentSpaceTransform(End).GetLocation();
			JointT.SetRotation(FQuat::FindBetweenVectors(EndAfterRoot - JointT.GetLocation(), Solved.End - Solved.Joint) * JointT.GetRotation());
			JointT.SetLocation(Solved.Joint);
			Space.SetComponentSpaceTransform(Joint, JointT);
			const FQuat Turn = FQuat::Slerp(EndNow.GetRotation(), Wanted.GetRotation(), Weight);
			Space.SetComponentSpaceTransform(End, FTransform(Turn, Solved.End, EndNow.GetScale3D()));
		}

		const UAnimSequence* Clips[ClipCount] = {};
		FName UpperBodyBone;
		FVeyraVanguardAnimState State;
		UVeyraVanguardAnimInstance::FLimbFrame Limbs;
		TArray<bool> UpperBody;
		TArray<float> Weights;
	};
}

void UVeyraVanguardAnimInstance::Configure(const FVeyraVanguardBody& Art, const FVeyraVanguardAnimShape& InShape)
{
	Clips.SetNum(ClipCount);
	for (int32 Index = 0; Index < ClipCount; ++Index)
	{
		Clips[Index] = Art.Find(static_cast<EVeyraVanguardClip>(Index));
	}
	UpperBodyBone = Art.UpperBodyBone;
	ConfigureLimbs(Art);
	Shape = InShape;
	Shape.Lengths = Art.Lengths();
	Shape.RunStride = Art.RunStride;
	Shape.CastReleaseShare = Art.CastReleaseShare;
	State = FVeyraVanguardAnimState();
}

void UVeyraVanguardAnimInstance::ConfigureLimbs(const FVeyraVanguardBody& Art)
{
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	Limbs = FLimbFrame();
	if (!Art.Mesh)
	{
		return;
	}
	// The rest pose in the skin's space, bone by bone from the root: where each foot rests, and where the off hand
	// holds the weapon hand.
	const FReferenceSkeleton& Reference = Art.Mesh->GetRefSkeleton();
	const TArray<FTransform>& Local = Reference.GetRefBonePose();
	TArray<FTransform> Rest;
	Rest.SetNum(Local.Num());
	for (int32 Bone = 0; Bone < Local.Num(); ++Bone)
	{
		const int32 Parent = Reference.GetParentIndex(Bone);
		Rest[Bone] = Parent == INDEX_NONE ? Local[Bone] : Local[Bone] * Rest[Parent];
	}
	const auto RestOf = [&Reference, &Rest](FName Name) {
		const int32 Bone = Reference.FindBoneIndex(Name);
		return Bone == INDEX_NONE ? FTransform::Identity : Rest[Bone];
	};
	if (Settings.bFootIK)
	{
		for (const FVeyraLimbChain& Chain : Art.FootChains)
		{
			if (Chain.IsSet())
			{
				Limbs.Feet.Add(Chain);
				Limbs.FootRestHeights.Add(RestOf(Chain.End).GetLocation().Z);
				Limbs.FootOffsets.Add(0.0f);
				Limbs.FootNormals.Add(FVector::UpVector);
				// The pelvis: the bone both legs hang from.
				const int32 Root = Reference.FindBoneIndex(Chain.Root);
				const int32 Parent = Root == INDEX_NONE ? INDEX_NONE : Reference.GetParentIndex(Root);
				Limbs.Pelvis = Parent == INDEX_NONE ? NAME_None : Reference.GetBoneName(Parent);
			}
		}
	}
	if (Art.OffHand.IsSet() && !Art.OffHandAnchor.IsNone())
	{
		Limbs.OffHand = Art.OffHand;
		Limbs.OffHandAnchor = Art.OffHandAnchor;
		Limbs.OffHandFromAnchor = RestOf(Art.OffHand.End).GetRelativeTransform(RestOf(Art.OffHandAnchor));
		Limbs.OffHandWeight = Settings.OffHandIKWeight;
		Limbs.OffHandRelease = Settings.OffHandReleaseDistance;
	}
	Limbs.MaxStretch = Settings.LimbMaxStretch;
	Limbs.MaxPelvisDrop = Settings.FootMaxPelvisDrop;
	Limbs.PlantFade = Settings.FootPlantFade;
	Limbs.GroundTilt = Settings.FootGroundTilt;
}

void UVeyraVanguardAnimInstance::TraceFeet(float DeltaSeconds)
{
	const USkeletalMeshComponent* Skin = GetSkelMeshComponent();
	const UWorld* World = Skin ? Skin->GetWorld() : nullptr;
	if (Limbs.Feet.IsEmpty() || !World)
	{
		return;
	}
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	Limbs.FootWeight = VeyraLimbIK::SpeedWeight(Inputs.GroundSpeed, Shape.RunBlendSpeed, Settings.FootIKMovingWeight);
	// Sparingly: a body no one has seen lately is not traced for.
	if (!Skin->WasRecentlyRendered(Settings.FootTraceRecentSeconds))
	{
		return;
	}
	const FTransform Frame = Skin->GetComponentTransform();
	const double Floor = Frame.GetLocation().Z;
	const double Scale = FMath::Max(Frame.GetScale3D().Z, UE_KINDA_SMALL_NUMBER);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(VeyraFootIK), false, Skin->GetOwner());
	for (int32 Foot = 0; Foot < Limbs.Feet.Num(); ++Foot)
	{
		const FVector Where = Skin->GetBoneLocation(Limbs.Feet[Foot].End);
		FHitResult Hit;
		const bool bGround = World->LineTraceSingleByObjectType(Hit, FVector(Where.X, Where.Y, Floor + Settings.FootTraceAbove),
			FVector(Where.X, Where.Y, Floor - Settings.FootTraceBelow), FCollisionObjectQueryParams(ECC_WorldStatic), Query);
		const double Offset = bGround ? FMath::Clamp(Hit.ImpactPoint.Z - Floor, -Settings.FootMaxOffset, Settings.FootMaxOffset) : 0.0;
		const FVector Normal = bGround ? Frame.InverseTransformVectorNoScale(Hit.ImpactNormal) : FVector::UpVector;
		// Eased toward, so a step's edge or a stray hit does not pop the foot.
		Limbs.FootOffsets[Foot] = FMath::FInterpTo(Limbs.FootOffsets[Foot], static_cast<float>(Offset / Scale), DeltaSeconds, Settings.FootIKInterpSpeed);
		Limbs.FootNormals[Foot] = FMath::VInterpNormalRotationTo(Limbs.FootNormals[Foot], Normal, DeltaSeconds, Settings.FootTiltSpeed);
	}
}

void UVeyraVanguardAnimInstance::NoteCue(EVeyraCombatCueKind Cue, float SecondsLeft)
{
	if (IsConfigured())
	{
		VeyraVanguardAnim::NoteCue(State, Cue, SecondsLeft, Shape);
	}
}

const UAnimSequence* UVeyraVanguardAnimInstance::GetClip(EVeyraVanguardClip Clip) const
{
	const int32 Index = static_cast<int32>(Clip);
	return Clips.IsValidIndex(Index) ? Clips[Index].Get() : nullptr;
}

void UVeyraVanguardAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	if (IsConfigured())
	{
		VeyraVanguardAnim::Advance(State, DeltaSeconds, Inputs, Shape);
		TraceFeet(DeltaSeconds);
	}
}

FAnimInstanceProxy* UVeyraVanguardAnimInstance::CreateAnimInstanceProxy()
{
	return new FVeyraVanguardAnimProxy(this);
}

void UVeyraVanguardAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}
