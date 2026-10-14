// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Text/VeyraAbilityNumbers.h"

#include "Stats/VeyraHaste.h"
#include "Statuses/VeyraStatusTypes.h"
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

	EVeyraFactTone ToneOf(EVeyraDamageType Type)
	{
		switch (Type)
		{
		case EVeyraDamageType::Physical:
			return EVeyraFactTone::Physical;
		case EVeyraDamageType::Magic:
			return EVeyraFactTone::Magic;
		case EVeyraDamageType::TrueDamage:
			return EVeyraFactTone::TrueDamage;
		}
		return EVeyraFactTone::Plain;
	}

	/** How a part's amount is made from the caster's powers: "60 + 20% of your 58 Physical Power"; empty with no ratio. */
	FString MadeOf(const FVeyraDamageTuning& Damage, int32 Rank, double PhysicalPower, double MagicPower)
	{
		constexpr double Percent = 100.0;
		TArray<FString> Parts;
		if (Damage.PhysicalPowerRatio != 0.0)
		{
			Parts.Add(FString::Printf(TEXT("%s%% of your %s Physical Power"), *Number(Damage.PhysicalPowerRatio * Percent), *Number(PhysicalPower)));
		}
		if (Damage.MagicPowerRatio != 0.0)
		{
			Parts.Add(FString::Printf(TEXT("%s%% of your %s Magic Power"), *Number(Damage.MagicPowerRatio * Percent), *Number(MagicPower)));
		}
		if (Parts.IsEmpty())
		{
			return FString();
		}
		return FString::Printf(TEXT("%s + %s"), *Number(VeyraAbilityRules::ValueAtRank(Damage.AmountByRank, Rank)), *FString::Join(Parts, TEXT(" + ")));
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

FVeyraAbilitySummary Summarise(const FVeyraContentId& Ability, int32 Rank, int32 MaxRank, double PhysicalPower, double MagicPower, double MaxHealth,
	double AbilityHaste, const FString& ResourceName)
{
	constexpr double Percent = 100.0;
	const FVeyraAbilitiesTuning& Tuning = UVeyraAbilitiesTuningSubsystem::Get();
	const int32 Shown = FMath::Max(1, Rank);
	FVeyraAbilitySummary Summary;
	// The wait the caster would have, as their cooldowns start (ADR-012 §6).
	const double BaseCooldown = VeyraAbilityRules::CooldownSeconds(Tuning, Ability, Shown);
	if (BaseCooldown > 0.0)
	{
		Summary.CooldownSeconds = BaseCooldown * VeyraHaste::CooldownMultiplier(AbilityHaste);
		Summary.Cooldown = FString::Printf(TEXT("%s s"), *Number(Summary.CooldownSeconds));
	}
	if (const double Cost = VeyraAbilityRules::ResourceCost(Tuning, Ability, Shown); Cost > 0.0)
	{
		Summary.Cost = ResourceName.IsEmpty() ? Number(Cost) : FString::Printf(TEXT("%s %s"), *Number(Cost), *ResourceName);
	}
	TArray<FString> Next;
	for (const VeyraAbilityRules::FVeyraAbilityDamagePart& Part : VeyraAbilityRules::DamageParts(Tuning, Ability))
	{
		for (const FVeyraDamageTuning& Damage : Part.Damage)
		{
			FVeyraAbilityFact& Fact = Summary.Facts.AddDefaulted_GetRef();
			Fact.Label = VeyraContentText::DamageRole(Part.Role);
			// Damage reads whole, as Combat Text shows it land; how it is made keeps its tenths.
			Fact.Value = FString::Printf(TEXT("%.0f %s"), FMath::RoundToDouble(VeyraAbilityRules::DamageAmount(Damage, Shown, PhysicalPower, MagicPower)), TypeWord(Damage.Type));
			Fact.Note = MadeOf(Damage, Shown, PhysicalPower, MagicPower);
			Fact.Tone = ToneOf(Damage.Type);
			// The whole amount a rank up would deal with the caster's power now, comparable with the value above it.
			if (Rank >= 1 && Rank < MaxRank)
			{
				Next.Add(FString::Printf(TEXT("%.0f"), FMath::RoundToDouble(VeyraAbilityRules::DamageAmount(Damage, Rank + 1, PhysicalPower, MagicPower))));
			}
		}
	}
	Summary.NextRank = FString::Join(Next, TEXT(" / "));
	// What its shields hold now, how each is made from the caster's Max Health and Magic Power, and for how long.
	for (const VeyraAbilityRules::FVeyraAbilityShieldPart& Part : VeyraAbilityRules::ShieldParts(Tuning, Ability))
	{
		const FVeyraShieldTuning& Shield = Part.Shield;
		FVeyraAbilityFact& Fact = Summary.Facts.AddDefaulted_GetRef();
		Fact.Label = VeyraContentText::DamageRole(Part.Role);
		Fact.Value = FString::Printf(TEXT("%.0f shield"), FMath::RoundToDouble(VeyraAbilityRules::ShieldAmount(Shield, Shown, MaxHealth, MagicPower)));
		TArray<FString> Made;
		if (Shield.MaxHealthRatio != 0.0)
		{
			Made.Add(FString::Printf(TEXT("%s%% of your %s Max Health"), *Number(Shield.MaxHealthRatio * Percent), *Number(MaxHealth)));
		}
		if (Shield.MagicPowerRatio != 0.0)
		{
			Made.Add(FString::Printf(TEXT("%s%% of your %s Magic Power"), *Number(Shield.MagicPowerRatio * Percent), *Number(MagicPower)));
		}
		TArray<FString> Note;
		if (!Made.IsEmpty())
		{
			Note.Add(FString::Printf(TEXT("%s + %s"), *Number(VeyraAbilityRules::ValueAtRank(Shield.AmountByRank, Shown)), *FString::Join(Made, TEXT(" + "))));
		}
		if (Shield.DurationSeconds > 0.0)
		{
			Note.Add(FString::Printf(TEXT("for %s s"), *Number(Shield.DurationSeconds)));
		}
		Fact.Note = FString::Join(Note, TEXT(", "));
		Fact.Tone = EVeyraFactTone::Help;
	}
	// What its effects do to what they reach: a strength as a share, and how long.
	for (const VeyraAbilityRules::FVeyraAbilityStatusPart& Part : VeyraAbilityRules::StatusParts(Tuning, Ability))
	{
		for (const FVeyraContentId& Id : Part.Statuses)
		{
			const FVeyraStatusTuning* Status = Tuning.Statuses.Find(Id);
			if (!Status)
			{
				continue;
			}
			FVeyraAbilityFact& Fact = Summary.Facts.AddDefaulted_GetRef();
			Fact.Label = VeyraContentText::DamageRole(Part.Role);
			const FString Word = VeyraContentText::StatusKindName(Status->Kind).ToString();
			const FString Duration = Status->DurationSeconds > 0.0 ? FString::Printf(TEXT("%s s"), *Number(Status->DurationSeconds)) : FString();
			if (Status->Magnitude != 0.0 && FMath::Abs(Status->Magnitude) <= 1.0)
			{
				Fact.Value = FString::Printf(TEXT("%s %s%%"), *Word, *Number(FMath::Abs(Status->Magnitude) * Percent));
				Fact.Note = Duration.IsEmpty() ? FString() : FString::Printf(TEXT("for %s"), *Duration);
			}
			else
			{
				Fact.Value = Duration.IsEmpty() ? Word : FString::Printf(TEXT("%s %s"), *Word, *Duration);
			}
			Fact.Tone = VeyraStatuses::IsHarmful(Status->Kind, Status->Magnitude) ? EVeyraFactTone::Harm : EVeyraFactTone::Help;
		}
	}
	return Summary;
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
