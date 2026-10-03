// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraVanguardArtSet.h"

#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"

namespace
{
	constexpr EVeyraVanguardClip EveryClip[] = { EVeyraVanguardClip::Idle, EVeyraVanguardClip::Run, EVeyraVanguardClip::AttackWindup,
		EVeyraVanguardClip::AttackStrike, EVeyraVanguardClip::Cast, EVeyraVanguardClip::Hit, EVeyraVanguardClip::Death, EVeyraVanguardClip::Recall };
}

UAnimSequence* FVeyraVanguardArt::Find(EVeyraVanguardClip Clip) const
{
	const TObjectPtr<UAnimSequence>* Sequence = Animations.Find(VeyraVanguardAnim::NameOf(Clip));
	return Sequence ? Sequence->Get() : nullptr;
}

FVeyraVanguardClipLengths FVeyraVanguardArt::Lengths() const
{
	FVeyraVanguardClipLengths Lengths;
	for (const EVeyraVanguardClip Clip : EveryClip)
	{
		const UAnimSequence* Sequence = Find(Clip);
		Lengths.Seconds[static_cast<int32>(Clip)] = Sequence ? static_cast<float>(Sequence->GetPlayLength()) : 0.0f;
	}
	return Lengths;
}

TArray<FString> UVeyraVanguardArtSet::Validate() const
{
	TArray<FString> Problems;
	for (const TPair<FName, FVeyraVanguardArt>& Entry : Art)
	{
		const FString Id = Entry.Key.ToString();
		const FVeyraVanguardArt& Body = Entry.Value;
		const USkeleton* Skeleton = Body.Mesh ? Body.Mesh->GetSkeleton() : nullptr;
		if (!Skeleton)
		{
			Problems.Add(Id + TEXT(": a skeletal mesh with a skeleton is required."));
			continue;
		}
		for (const EVeyraVanguardClip Clip : EveryClip)
		{
			const UAnimSequence* Sequence = Body.Find(Clip);
			if (!Sequence || Sequence->GetPlayLength() <= 0.0)
			{
				Problems.Add(FString::Printf(TEXT("%s: the %s animation is required."), *Id, *VeyraVanguardAnim::NameOf(Clip).ToString()));
			}
			else if (Sequence->GetSkeleton() != Skeleton)
			{
				Problems.Add(FString::Printf(TEXT("%s: the %s animation is on another skeleton."), *Id, *VeyraVanguardAnim::NameOf(Clip).ToString()));
			}
		}
		if (Body.RunStride <= 0.0f)
		{
			Problems.Add(Id + TEXT(": RunStride must be above 0."));
		}
		if (Body.CastReleaseShare <= 0.0f || Body.CastReleaseShare >= 1.0f)
		{
			Problems.Add(Id + TEXT(": CastReleaseShare must be above 0 and below 1."));
		}
		if (Body.Mesh->GetRefSkeleton().FindBoneIndex(Body.UpperBodyBone) == INDEX_NONE)
		{
			Problems.Add(Id + TEXT(": UpperBodyBone must be a bone of its mesh."));
		}
	}
	return Problems;
}
