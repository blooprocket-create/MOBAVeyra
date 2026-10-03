// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraVanguardAnimInstance.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
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

		const UAnimSequence* Clips[ClipCount] = {};
		FName UpperBodyBone;
		FVeyraVanguardAnimState State;
		TArray<bool> UpperBody;
		TArray<float> Weights;
	};
}

void UVeyraVanguardAnimInstance::Configure(const FVeyraVanguardArt& Art, const FVeyraVanguardAnimShape& InShape)
{
	Clips.SetNum(ClipCount);
	for (int32 Index = 0; Index < ClipCount; ++Index)
	{
		Clips[Index] = Art.Find(static_cast<EVeyraVanguardClip>(Index));
	}
	UpperBodyBone = Art.UpperBodyBone;
	Shape = InShape;
	Shape.Lengths = Art.Lengths();
	Shape.RunStride = Art.RunStride;
	Shape.CastReleaseShare = Art.CastReleaseShare;
	State = FVeyraVanguardAnimState();
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
