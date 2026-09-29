// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Statuses/VeyraStatusTypes.h"

namespace VeyraStatuses
{
namespace
{
	/** Kinds whose Magnitude is a fraction removed per stack, where every stack together stays below 1. */
	bool IsReductionKind(EVeyraStatusKind Kind)
	{
		return Kind == EVeyraStatusKind::Tenacity || Kind == EVeyraStatusKind::DamageReduction || Kind == EVeyraStatusKind::DisplacementResistance;
	}

	/** Kinds whose Magnitude is a signed change per stack, where the stat always stays above 0. */
	bool IsChangeKind(EVeyraStatusKind Kind)
	{
		return Kind == EVeyraStatusKind::MoveSpeed || Kind == EVeyraStatusKind::AttackSpeed || Kind == EVeyraStatusKind::HealthRegeneration
			|| Kind == EVeyraStatusKind::DamageAmplification;
	}
}

TArray<FString> Validate(const FVeyraStatusSpec& Spec)
{
	TArray<FString> Problems;
	if (!Spec.Id.IsValid())
	{
		Problems.Add(TEXT("id: must be a content ID"));
	}
	if (!FMath::IsFinite(Spec.DurationSeconds) || Spec.DurationSeconds <= 0.0)
	{
		Problems.Add(TEXT("durationSeconds: must be finite and above 0"));
	}
	const bool bStacks = Spec.Stacking == EVeyraStackingPolicy::Stacking;
	if (Spec.MaxStacks < 1 || (!bStacks && Spec.MaxStacks != 1))
	{
		Problems.Add(TEXT("maxStacks: must be at least 1, and exactly 1 unless the status stacks"));
	}
	if (bStacks && (Spec.Kind == EVeyraStatusKind::Stun || Spec.Kind == EVeyraStatusKind::Slow))
	{
		Problems.Add(TEXT("stacking: crowd control does not stack; overlapping instances are tracked instead (Combat Bible §8)"));
	}
	const double Extension = Spec.TakedownExtensionSeconds;
	const double MaxExtension = Spec.TakedownExtensionMaxSeconds;
	const bool bNoExtension = Extension == 0.0 && MaxExtension == 0.0;
	const bool bValidExtension = FMath::IsFinite(Extension) && FMath::IsFinite(MaxExtension) && Extension > 0.0 && MaxExtension >= Extension;
	if (!bNoExtension && !bValidExtension)
	{
		Problems.Add(TEXT("takedownExtensionSeconds: both takedown extension values are 0, or the extension is above 0 and the maximum at least as long"));
	}

	const double Magnitude = Spec.Magnitude;
	const double AllStacks = Magnitude * FMath::Max(Spec.MaxStacks, 1);
	bool bMagnitudeValid = FMath::IsFinite(Magnitude);
	switch (Spec.Kind)
	{
	case EVeyraStatusKind::Stun:
		bMagnitudeValid &= Magnitude == 0.0;
		break;
	case EVeyraStatusKind::Slow:
		bMagnitudeValid &= Magnitude > 0.0 && Magnitude < 1.0;
		break;
	case EVeyraStatusKind::MoveSpeed:
	case EVeyraStatusKind::AttackSpeed:
	case EVeyraStatusKind::HealthRegeneration:
		bMagnitudeValid &= Magnitude != 0.0 && AllStacks > -1.0;
		break;
	case EVeyraStatusKind::DamageAmplification:
		bMagnitudeValid &= Magnitude > 0.0 && AllStacks <= 1.0;
		break;
	case EVeyraStatusKind::Tenacity:
	case EVeyraStatusKind::DamageReduction:
	case EVeyraStatusKind::DisplacementResistance:
		bMagnitudeValid &= Magnitude > 0.0 && AllStacks < 1.0;
		break;
	case EVeyraStatusKind::AttackCleave:
	case EVeyraStatusKind::MoveSpeedTowardEnemyVanguards:
		bMagnitudeValid &= Magnitude > 0.0 && AllStacks <= 1.0;
		break;
	}
	if (!bMagnitudeValid)
	{
		Problems.Add(TEXT("magnitude: out of range for the status's kind (EVeyraStatusKind)"));
	}
	return Problems;
}

bool IsTenacityReducible(EVeyraStatusKind Kind)
{
	return Kind == EVeyraStatusKind::Stun || Kind == EVeyraStatusKind::Slow;
}

double ApplyTenacity(double DurationSeconds, double TenacityRetained, double FloorSeconds)
{
	const double Shortened = DurationSeconds * FMath::Clamp(TenacityRetained, 0.0, 1.0);
	return FMath::Max(Shortened, FMath::Min(DurationSeconds, FloorSeconds));
}

double StatMultiplier(EVeyraStatusKind Kind, double Magnitude, int32 Stacks)
{
	if (IsChangeKind(Kind))
	{
		return 1.0 + Magnitude * Stacks;
	}
	if (IsReductionKind(Kind))
	{
		return 1.0 - Magnitude * Stacks;
	}
	return 1.0;
}

bool IsStronger(const FVeyraStatusEntry& Active, double Magnitude, double EndsAt)
{
	const double NewStrength = FMath::Abs(Magnitude);
	const double ActiveStrength = FMath::Abs(Active.Magnitude);
	if (!FMath::IsNearlyEqual(NewStrength, ActiveStrength))
	{
		return NewStrength > ActiveStrength;
	}
	return EndsAt > Active.EndsAt;
}

double Strongest(TConstArrayView<FVeyraStatusEntry> Entries, EVeyraStatusKind Kind)
{
	double Largest = 0.0;
	for (const FVeyraStatusEntry& Entry : Entries)
	{
		if (Entry.Kind == Kind)
		{
			Largest = FMath::Max(Largest, Entry.Magnitude);
		}
	}
	return Largest;
}

double StrongestSlow(TConstArrayView<FVeyraStatusEntry> Entries)
{
	return Strongest(Entries, EVeyraStatusKind::Slow);
}

EVeyraActionBlocks ActionBlocks(TConstArrayView<FVeyraStatusEntry> Entries)
{
	EVeyraActionBlocks Blocks = EVeyraActionBlocks::None;
	for (const FVeyraStatusEntry& Entry : Entries)
	{
		if (Entry.Kind == EVeyraStatusKind::Stun)
		{
			Blocks |= EVeyraActionBlocks::Move | EVeyraActionBlocks::Attack | EVeyraActionBlocks::Cast;
		}
	}
	return Blocks;
}
}
