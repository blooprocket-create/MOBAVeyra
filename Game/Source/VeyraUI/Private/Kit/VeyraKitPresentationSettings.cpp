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
	for (const FVeyraAbilityCastEffect& Cast : AbilityCastEffects)
	{
		Require(!Cast.Ability.IsNone() && !Cast.Effect.IsNull() && Cast.Scale > 0.0f, TEXT("AbilityCastEffects"),
			FString::Printf(TEXT("%s needs an ability, an effect and a scale above 0."), *Cast.Ability.ToString()));
	}
	return Problems;
}

const FVeyraAbilityCastEffect* UVeyraKitPresentationSettings::CastEffectOf(FName Ability) const
{
	return AbilityCastEffects.FindByPredicate([Ability](const FVeyraAbilityCastEffect& Cast) { return SameId(Cast.Ability, Ability); });
}

bool UVeyraKitPresentationSettings::IsStrand(FName Status) const
{
	return StatusStrands.ContainsByPredicate([Status](const FVeyraStatusStrand& Strand) { return SameId(Strand.Status, Status); });
}

const FVeyraStatusMark* UVeyraKitPresentationSettings::MarkOf(FName Status) const
{
	return StatusMarks.FindByPredicate([Status](const FVeyraStatusMark& Mark) { return SameId(Mark.Status, Status); });
}
