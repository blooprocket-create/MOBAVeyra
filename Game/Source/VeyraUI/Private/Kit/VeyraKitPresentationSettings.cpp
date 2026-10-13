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
	const bool bRuns = AbilityEffects.ContainsByPredicate(
		[](const FVeyraAbilityEffects& Effects) { return Effects.Channel.IsSet() || Effects.Tether.IsSet() || Effects.Linger.IsSet(); });
	Require(!bRuns || (!LengthParameter.IsNone() && !WidthParameter.IsNone() && LengthParameter != WidthParameter), TEXT("LengthParameter"),
		TEXT("an effect that runs along a channel, a tether or a lingering area needs the two parameters its length and width are set by."));
	Require(!AttackTravel.IsSet() || AttackTravel.Scale > 0.0f, TEXT("AttackTravel"),
		TEXT("a basic attack's effect in flight needs a scale above 0."));
	TSet<FName> AttackingVanguards;
	for (const FVeyraVanguardAttack& Attack : VanguardAttacks)
	{
		const FName Named(*Attack.Vanguard.ToString().ToLower());
		Require(!Attack.Vanguard.IsNone() && Attack.Travel.IsSet() && Attack.Travel.Scale > 0.0f && !AttackingVanguards.Contains(Named), TEXT("VanguardAttacks"),
			FString::Printf(TEXT("%s needs a Vanguard named once, and an effect in flight with a scale above 0."), *Attack.Vanguard.ToString()));
		AttackingVanguards.Add(Named);
	}
	for (const FVeyraAbilityEffects& Effects : AbilityEffects)
	{
		const FString Name = Effects.Ability.ToString();
		const FVeyraSkillEffectStage* Stages[] = { &Effects.Windup, &Effects.Channel, &Effects.Commit, &Effects.Travel, &Effects.Impact, &Effects.Tether,
			&Effects.Mark, &Effects.Arc, &Effects.Linger };
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
		Require(Effects.TetherBone.IsNone() == Effects.TetherFromBone.IsNone() && (!Effects.Tether.IsSet() || !Effects.TetherBone.IsNone()), TEXT("AbilityEffects"),
			FString::Printf(TEXT("%s: a tether names the bone that flies and the bone it returns to, and only a tether runs a tether effect."), *Name));
		Require(Effects.Arc.IsSet() == (!Effects.ArcBone.IsNone() && Effects.ArcHeight > 0.0f), TEXT("AbilityEffects"),
			FString::Printf(TEXT("%s: an arc flies from the bone it names, rising a height above 0, and only an arc names them."), *Name));
		Require(Effects.Channel.IsSet() || Effects.ChannelBone.IsNone(), TEXT("AbilityEffects"),
			FString::Printf(TEXT("%s: only a channel effect pours from a channel bone."), *Name));
		Require(AbilityEffects.FilterByPredicate([&Effects](const FVeyraAbilityEffects& Other) { return SameId(Other.Ability, Effects.Ability); }).Num() == 1,
			TEXT("AbilityEffects"), FString::Printf(TEXT("%s is listed once."), *Name));
	}
	return Problems;
}

const FVeyraVanguardAttack* UVeyraKitPresentationSettings::VanguardAttackOf(FName Vanguard) const
{
	return VanguardAttacks.FindByPredicate([Vanguard](const FVeyraVanguardAttack& Attack) { return Attack.Vanguard.IsEqual(Vanguard, ENameCase::IgnoreCase); });
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
