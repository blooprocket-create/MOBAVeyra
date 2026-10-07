// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraVanguardArtSet.h"

#include "Animation/AnimSequence.h"
#include "Content/VeyraContentId.h"
#include "Engine/SkeletalMesh.h"
#include "NiagaraSystem.h"

namespace
{
	constexpr EVeyraVanguardClip EveryClip[] = { EVeyraVanguardClip::Idle, EVeyraVanguardClip::Run, EVeyraVanguardClip::AttackWindup,
		EVeyraVanguardClip::AttackStrike, EVeyraVanguardClip::Cast, EVeyraVanguardClip::Hit, EVeyraVanguardClip::Death, EVeyraVanguardClip::Recall };
}

UAnimSequence* FVeyraVanguardBody::Find(EVeyraVanguardClip Clip) const
{
	const TObjectPtr<UAnimSequence>* Sequence = Animations.Find(VeyraVanguardAnim::NameOf(Clip));
	return Sequence ? Sequence->Get() : nullptr;
}

FVeyraVanguardClipLengths FVeyraVanguardBody::Lengths() const
{
	FVeyraVanguardClipLengths Lengths;
	for (const EVeyraVanguardClip Clip : EveryClip)
	{
		const UAnimSequence* Sequence = Find(Clip);
		Lengths.Seconds[static_cast<int32>(Clip)] = Sequence ? static_cast<float>(Sequence->GetPlayLength()) : 0.0f;
	}
	return Lengths;
}

TArray<FString> FVeyraVanguardBody::Validate(const FString& Label) const
{
	TArray<FString> Problems;
	const USkeleton* Skeleton = Mesh ? Mesh->GetSkeleton() : nullptr;
	if (!Skeleton)
	{
		Problems.Add(Label + TEXT(": a skeletal mesh with a skeleton is required."));
		return Problems;
	}
	for (const EVeyraVanguardClip Clip : EveryClip)
	{
		const UAnimSequence* Sequence = Find(Clip);
		if (!Sequence || Sequence->GetPlayLength() <= 0.0)
		{
			Problems.Add(FString::Printf(TEXT("%s: the %s animation is required."), *Label, *VeyraVanguardAnim::NameOf(Clip).ToString()));
		}
		else if (Sequence->GetSkeleton() != Skeleton)
		{
			Problems.Add(FString::Printf(TEXT("%s: the %s animation is on another skeleton."), *Label, *VeyraVanguardAnim::NameOf(Clip).ToString()));
		}
	}
	if (RunStride <= 0.0f)
	{
		Problems.Add(Label + TEXT(": RunStride must be above 0."));
	}
	if (CastReleaseShare <= 0.0f || CastReleaseShare >= 1.0f)
	{
		Problems.Add(Label + TEXT(": CastReleaseShare must be above 0 and below 1."));
	}
	if (Mesh->GetRefSkeleton().FindBoneIndex(UpperBodyBone) == INDEX_NONE)
	{
		Problems.Add(Label + TEXT(": UpperBodyBone must be a bone of its mesh."));
	}
	if (Effect && EffectBones.IsEmpty())
	{
		Problems.Add(Label + TEXT(": an Effect needs EffectBones to pour from."));
	}
	if (!(EffectScale > 0.0f))
	{
		Problems.Add(Label + TEXT(": EffectScale must be above 0."));
	}
	for (const FName& Bone : EffectBones)
	{
		if (!Effect || Mesh->GetRefSkeleton().FindBoneIndex(Bone) == INDEX_NONE)
		{
			Problems.Add(FString::Printf(TEXT("%s: EffectBones' %s must be a bone of its mesh, with an Effect to pour."), *Label, *Bone.ToString()));
		}
	}
	// Every bone its inverse kinematics names is one of its skeleton's (ADR-069).
	const FReferenceSkeleton& Bones = Mesh->GetRefSkeleton();
	TArray<const FVeyraLimbChain*> Chains;
	for (const FVeyraLimbChain& Chain : FootChains)
	{
		Chains.Add(&Chain);
	}
	if (OffHand.IsSet() || !OffHandAnchor.IsNone())
	{
		Chains.Add(&OffHand);
		if (Bones.FindBoneIndex(OffHandAnchor) == INDEX_NONE)
		{
			Problems.Add(FString::Printf(TEXT("%s: the off hand's anchor %s is no bone of its skeleton."), *Label, *OffHandAnchor.ToString()));
		}
	}
	for (const FVeyraLimbChain* Chain : Chains)
	{
		for (const FName Bone : { Chain->Root, Chain->Joint, Chain->End })
		{
			if (Bones.FindBoneIndex(Bone) == INDEX_NONE)
			{
				Problems.Add(FString::Printf(TEXT("%s: the limb bone %s is no bone of its skeleton."), *Label, *Bone.ToString()));
			}
		}
	}	return Problems;
}

const FVeyraVanguardBody& FVeyraVanguardArt::BodyFor(TFunctionRef<bool(FName)> Holds) const
{
	// By priority, then status ID, so a unit holding two statuses with bodies always wears the same one.
	TArray<FName> Statuses;
	StatusBodies.GetKeys(Statuses);
	Statuses.Sort([this](FName A, FName B) {
		const int32 First = StatusBodies[A].Priority, Second = StatusBodies[B].Priority;
		return First != Second ? First > Second : FNameLexicalLess()(A, B);
	});
	for (const FName& Status : Statuses)
	{
		if (Holds(Status))
		{
			return StatusBodies[Status];
		}
	}
	return *this;
}

TArray<FString> UVeyraVanguardArtSet::Validate() const
{
	TArray<FString> Problems;
	TArray<TPair<FString, const FVeyraVanguardArt*>> Entries;
	for (const TPair<FName, FVeyraVanguardArt>& Entry : Art)
	{
		Entries.Emplace(Entry.Key.ToString(), &Entry.Value);
	}
	for (const TPair<FName, FVeyraVanguardArt>& Entry : CompanionArt)
	{
		Entries.Emplace(TEXT("companion ") + Entry.Key.ToString(), &Entry.Value);
	}
	for (const TPair<FString, const FVeyraVanguardArt*>& Entry : Entries)
	{
		const FString& Id = Entry.Key;
		Problems.Append(Entry.Value->Validate(Id));
		for (const TPair<FName, FVeyraVanguardBody>& Status : Entry.Value->StatusBodies)
		{
			// Whether the status is one the ability tuning defines is the committed art's test to check, as its Vanguard
			// IDs are: tuning a test scopes may hold other statuses.
			const FString Label = Id + TEXT(" while ") + Status.Key.ToString();
			if (!FVeyraContentId::FromText(Status.Key.ToString()))
			{
				Problems.Add(Label + TEXT(": the status must be a content ID."));
			}
			Problems.Append(Status.Value.Validate(Label));
		}
	}
	return Problems;
}
