// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Text/VeyraAbilityNumbers.h"

#include "Text/VeyraContentText.h"
#include "Tuning/VeyraAbilitiesTuning.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

namespace VeyraAbilityNumbers
{
namespace
{
	const TCHAR* TypeWord(EVeyraDamageType Type)
	{
		switch (Type)
		{
		case EVeyraDamageType::Physical:
			return TEXT("physical");
		case EVeyraDamageType::Magic:
			return TEXT("magic");
		case EVeyraDamageType::TrueDamage:
			return TEXT("true");
		}
		return TEXT("");
	}

	/** A whole number when it is one, else to a tenth. */
	FString Number(double Value)
	{
		constexpr double WholeWithin = 0.05;
		const double Rounded = FMath::RoundToDouble(Value);
		return FMath::IsNearlyEqual(Value, Rounded, WholeWithin) ? FString::Printf(TEXT("%.0f"), Rounded) : FString::Printf(TEXT("%.1f"), Value);
	}

	/** Its ratios, as "50% Physical Power + 30% Magic Power"; empty for none. */
	FString Ratios(const FVeyraDamageTuning& Damage)
	{
		constexpr double Percent = 100.0;
		TArray<FString> Parts;
		if (Damage.PhysicalPowerRatio != 0.0)
		{
			Parts.Add(FString::Printf(TEXT("%s%% Physical Power"), *Number(Damage.PhysicalPowerRatio * Percent)));
		}
		if (Damage.MagicPowerRatio != 0.0)
		{
			Parts.Add(FString::Printf(TEXT("%s%% Magic Power"), *Number(Damage.MagicPowerRatio * Percent)));
		}
		return FString::Join(Parts, TEXT(" + "));
	}
}

TArray<FString> AtRank(const FVeyraContentId& Ability, int32 Rank, double PhysicalPower, double MagicPower, const FString& ResourceName)
{
	const FVeyraAbilitiesTuning& Tuning = UVeyraAbilitiesTuningSubsystem::Get();
	TArray<FString> Lines;
	const double Cooldown = VeyraAbilityRules::CooldownSeconds(Tuning, Ability, Rank);
	const double Cost = VeyraAbilityRules::ResourceCost(Tuning, Ability, Rank);
	TArray<FString> Cast;
	if (Cooldown > 0.0)
	{
		Cast.Add(FString::Printf(TEXT("Cooldown %s s"), *Number(Cooldown)));
	}
	if (Cost > 0.0)
	{
		Cast.Add(ResourceName.IsEmpty() ? FString::Printf(TEXT("Cost %s"), *Number(Cost)) : FString::Printf(TEXT("Cost %s %s"), *Number(Cost), *ResourceName));
	}
	if (!Cast.IsEmpty())
	{
		Lines.Add(FString::Join(Cast, TEXT("   ")));
	}
	for (const VeyraAbilityRules::FVeyraAbilityDamagePart& Part : VeyraAbilityRules::DamageParts(Tuning, Ability))
	{
		const FString Role = VeyraContentText::DamageRole(Part.Role).ToString();
		for (const FVeyraDamageTuning& Damage : Part.Damage)
		{
			const double Amount = VeyraAbilityRules::DamageAmount(Damage, Rank, PhysicalPower, MagicPower);
			FString Line = FString::Printf(TEXT("%s: %s %s damage"), *Role, *Number(Amount), TypeWord(Damage.Type));
			const FString Scaling = Ratios(Damage);
			if (!Scaling.IsEmpty())
			{
				Line += FString::Printf(TEXT(" (%s + %s)"), *Number(VeyraAbilityRules::ValueAtRank(Damage.AmountByRank, Rank)), *Scaling);
			}
			Lines.Add(Line);
		}
	}
	return Lines;
}

TArray<FString> ByRank(const FVeyraContentId& Ability)
{
	TArray<FString> Lines;
	for (const VeyraAbilityRules::FVeyraAbilityDamagePart& Part : VeyraAbilityRules::DamageParts(UVeyraAbilitiesTuningSubsystem::Get(), Ability))
	{
		const FString Role = VeyraContentText::DamageRole(Part.Role).ToString();
		for (const FVeyraDamageTuning& Damage : Part.Damage)
		{
			TArray<FString> Amounts;
			for (const double Amount : Damage.AmountByRank)
			{
				Amounts.Add(Number(Amount));
			}
			const FString Scaling = Ratios(Damage);
			const FString WithScaling = Scaling.IsEmpty() ? FString() : FString::Printf(TEXT(" (+%s)"), *Scaling);
			Lines.Add(FString::Printf(TEXT("%s: %s%s %s damage"), *Role, *FString::Join(Amounts, TEXT(" / ")), *WithScaling, TypeWord(Damage.Type)));
		}
	}
	return Lines;
}
}
