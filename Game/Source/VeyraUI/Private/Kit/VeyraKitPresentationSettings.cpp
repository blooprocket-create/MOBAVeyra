// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Kit/VeyraKitPresentationSettings.h"

#include "NiagaraSystem.h"

namespace
{
	/** A cooked build keeps one spelling per name, so IDs compare ignoring case. */
	bool SameId(FName A, FName B)
	{
		return A.ToString().Equals(B.ToString(), ESearchCase::IgnoreCase);
	}
}

TArray<FString> UVeyraKitPresentationSettings::Validate() const
{
	TArray<FString> Problems;
	const auto Require = [&Problems](bool bValid, const TCHAR* Field, const FString& Message) {
		if (!bValid)
		{
			Problems.Add(FString::Printf(TEXT("%s: %s"), Field, *Message));
		}
	};
	Require(StrandThickness > 0.0f && StrandHeightShare > 0.0f && StrandHeightShare <= 1.0f, TEXT("StrandThickness"),
		TEXT("a strand needs a thickness above 0, and runs above the feet, at most at the top of the body."));
	Require(StrandBeads >= 1 && StrandFlowSeconds > 0.0f && !StrandEffect.IsNull(), TEXT("StrandBeads"),
		TEXT("a strand needs at least one bead, a time above 0 to cross it, and the effect each bead is."));
	Require(AuraThickness > 0.0f && AuraPulseSeconds > 0.0f, TEXT("AuraThickness"), TEXT("an aura's ring needs a thickness and a pulse above 0."));
	Require(BurstSeconds > 0.0f && BurstThickness > 0.0f && !BurstEffect.IsNull(), TEXT("BurstSeconds"),
		TEXT("a burst needs some time to spread, a thickness and an effect."));
	for (const FVeyraStatusStrand& Strand : StatusStrands)
	{
		Require(!Strand.Status.IsNone(), TEXT("StatusStrands"), TEXT("each strand names a status."));
	}
	for (const FVeyraStatusMark& Mark : StatusMarks)
	{
		Require(!Mark.Status.IsNone() && !Mark.Effect.IsNull() && Mark.Scale > 0.0f, TEXT("StatusMarks"),
			FString::Printf(TEXT("%s needs a status, an effect and a scale above 0."), *Mark.Status.ToString()));
	}
	Require(ProjectileEndWindowSeconds > 0.0f || !AbilityEffects.ContainsByPredicate([](const FVeyraAbilityEffects& Effects) { return Effects.Impact.IsSet(); }),
		TEXT("ProjectileEndWindowSeconds"), TEXT("a projectile's impact needs a window above 0 to match its end to what was drawn."));
	Require(!ChannelLengthParameter.IsNone() || !AbilityEffects.ContainsByPredicate([](const FVeyraAbilityEffects& Effects) { return Effects.Channel.IsSet(); }),
		TEXT("ChannelLengthParameter"), TEXT("a channel's effect needs the parameter its length is set by."));
	for (const FVeyraAbilityEffects& Effects : AbilityEffects)
	{
		const FString Name = Effects.Ability.ToString();
		const FVeyraSkillEffectStage* Stages[] = { &Effects.Windup, &Effects.Channel, &Effects.Commit, &Effects.Travel, &Effects.Impact };
		bool bAny = false;
		bool bScaled = true;
		for (const FVeyraSkillEffectStage* Stage : Stages)
		{
			bAny |= Stage->IsSet();
			bScaled &= !Stage->IsSet() || Stage->Scale > 0.0f;
		}
		Require(!Effects.Ability.IsNone() && bAny, TEXT("AbilityEffects"), FString::Printf(TEXT("%s needs an ability and at least one stage."), *Name));
		Require(bScaled, TEXT("AbilityEffects"), FString::Printf(TEXT("%s: every stage with an effect needs a scale above 0."), *Name));
		Require(Effects.Windup.IsSet() == !Effects.WindupBones.IsEmpty(), TEXT("AbilityEffects"),
			FString::Printf(TEXT("%s: a windup effect pours from the bones it names, and only it names bones."), *Name));
		Require(Effects.Commit.IsSet() || !Effects.bCommitAtTarget, TEXT("AbilityEffects"),
			FString::Printf(TEXT("%s: only a commit effect is placed at the target."), *Name));
		Require(AbilityEffects.FilterByPredicate([&Effects](const FVeyraAbilityEffects& Other) { return SameId(Other.Ability, Effects.Ability); }).Num() == 1,
			TEXT("AbilityEffects"), FString::Printf(TEXT("%s is listed once."), *Name));
	}
	return Problems;
}

const FVeyraAbilityEffects* UVeyraKitPresentationSettings::EffectsOf(FName Ability) const
{
	return AbilityEffects.FindByPredicate([Ability](const FVeyraAbilityEffects& Effects) { return SameId(Effects.Ability, Ability); });
}

bool UVeyraKitPresentationSettings::IsStrand(FName Status) const
{
	return StatusStrands.ContainsByPredicate([Status](const FVeyraStatusStrand& Strand) { return SameId(Strand.Status, Status); });
}

const FVeyraStatusMark* UVeyraKitPresentationSettings::MarkOf(FName Status) const
{
	return StatusMarks.FindByPredicate([Status](const FVeyraStatusMark& Mark) { return SameId(Mark.Status, Status); });
}
