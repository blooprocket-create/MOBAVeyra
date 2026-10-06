// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraBasicAttackTypes.h"

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraDefenceSet.h"
#include "Damage/VeyraDamageResolver.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"

const TCHAR* LexToString(EVeyraAttackRejection Rejection)
{
	switch (Rejection)
	{
	case EVeyraAttackRejection::None:
		return TEXT("None");
	case EVeyraAttackRejection::NoProfile:
		return TEXT("NoProfile");
	case EVeyraAttackRejection::AttackerDead:
		return TEXT("AttackerDead");
	case EVeyraAttackRejection::CrowdControlled:
		return TEXT("CrowdControlled");
	case EVeyraAttackRejection::Busy:
		return TEXT("Busy");
	case EVeyraAttackRejection::OnCooldown:
		return TEXT("OnCooldown");
	case EVeyraAttackRejection::InvalidTarget:
		return TEXT("InvalidTarget");
	case EVeyraAttackRejection::OutOfRange:
		return TEXT("OutOfRange");
	case EVeyraAttackRejection::NotVisible:
		return TEXT("NotVisible");
	}
	return TEXT("Unknown");
}

void FVeyraAttackPlan::AddDamage(EVeyraDamageType Type, double Amount)
{
	if (FVeyraDamageComponent* Existing = Damage.Components.FindByPredicate([Type](const FVeyraDamageComponent& Component) { return Component.Type == Type; }))
	{
		Existing->Amount += Amount;
		return;
	}
	Damage.Components.Add({ Type, Amount });
}

void FVeyraAttackPlan::OfferSecondaryImpact(const FVeyraSecondaryImpact& Impact)
{
	if (!SecondaryImpact.IsSet() || Impact.Priority > SecondaryImpact->Priority)
	{
		SecondaryImpact = Impact;
	}
}

namespace VeyraBasicAttacks
{
namespace
{
	bool IsPositiveFinite(double Value)
	{
		return FMath::IsFinite(Value) && Value > 0.0;
	}

	bool IsNonNegativeFinite(double Value)
	{
		return FMath::IsFinite(Value) && Value >= 0.0;
	}
}

TArray<FString> Validate(const FVeyraBasicAttackProfile& Profile)
{
	TArray<FString> Problems;
	if (!IsPositiveFinite(Profile.Range))
	{
		Problems.Add(TEXT("range: must be above 0"));
	}
	if (!IsNonNegativeFinite(Profile.PhysicalPowerRatio) || !IsNonNegativeFinite(Profile.MagicPowerRatio))
	{
		Problems.Add(TEXT("physicalPowerRatio: both power ratios must be at least 0"));
	}
	if (!IsPositiveFinite(Profile.WindupFraction) || Profile.WindupFraction >= 1.0)
	{
		Problems.Add(TEXT("windupFraction: must be above 0 and below 1"));
	}
	if (!IsNonNegativeFinite(Profile.AcquisitionRadius) || !IsNonNegativeFinite(Profile.MinimumIntervalSeconds))
	{
		Problems.Add(TEXT("acquisitionRadius: the acquisition radius and minimum interval must be at least 0"));
	}
	if (Profile.Projectile.Num() > 1 || (Profile.Projectile.Num() == 1 && !IsPositiveFinite(Profile.Projectile[0].Speed)))
	{
		Problems.Add(TEXT("projectile: at most one, whose speed is above 0"));
	}
	if (Profile.Projectile.Num() == 1 && !IsNonNegativeFinite(Profile.Projectile[0].Radius))
	{
		Problems.Add(TEXT("projectile: its radius must be at least 0"));
	}
	if (Profile.Cleave.Num() > 1)
	{
		Problems.Add(TEXT("cleave: at most one"));
	}
	for (const FVeyraShape& Shape : Profile.Cleave)
	{
		for (const FString& ShapeProblem : VeyraShapes::Validate(Shape))
		{
			Problems.Add(TEXT("cleave: ") + ShapeProblem);
		}
	}
	return Problems;
}

FVeyraRawDamageEvent AgainstStructure(const FVeyraAttackPlan& Plan, double Effectiveness)
{
	FVeyraRawDamageEvent Damage = Plan.Damage;
	for (FVeyraDamageComponent& Component : Damage.Components)
	{
		const FVeyraDamageComponent* Base = Plan.BaseDamage.FindByPredicate([&Component](const FVeyraDamageComponent& Own) { return Own.Type == Component.Type; });
		const double Own = Base ? FMath::Min(Base->Amount, Component.Amount) : 0.0;
		Component.Amount = Own + (Component.Amount - Own) * Effectiveness;
	}
	return Damage;
}

double RawHit(const FVeyraBasicAttackProfile& Profile, double PhysicalPower, double MagicPower)
{
	return PhysicalPower * Profile.PhysicalPowerRatio + MagicPower * Profile.MagicPowerRatio;
}

double ShareTaken(const FVeyraBasicAttackProfile& Profile, const UAbilitySystemComponent& Target)
{
	if (Profile.DamageType == EVeyraDamageType::TrueDamage)
	{
		return 1.0;
	}
	const FGameplayAttribute Resistance = Profile.DamageType == EVeyraDamageType::Magic ? UVeyraDefenceSet::GetMagicResistAttribute() : UVeyraDefenceSet::GetArmorAttribute();
	const double Value = Target.HasAttributeSetForAttribute(Resistance) ? Target.GetNumericAttribute(Resistance) : 0.0;
	return VeyraDamage::ResistanceDamageMultiplier(Value, UVeyraCombatTuningSubsystem::Get().Resistance.MitigationConstant);
}

double ExpectedHit(const FVeyraBasicAttackProfile& Profile, double PhysicalPower, double MagicPower, const UAbilitySystemComponent& Target)
{
	return RawHit(Profile, PhysicalPower, MagicPower) * ShareTaken(Profile, Target);
}
}
