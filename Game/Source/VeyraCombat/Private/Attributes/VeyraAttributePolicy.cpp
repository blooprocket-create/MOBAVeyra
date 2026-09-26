// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraAttributePolicy.h"

#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraMobilitySet.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraResourceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Effects/VeyraDamageExecution.h"
#include "Effects/VeyraResourceSpendExecution.h"
#include "GameplayEffect.h"
#include "VeyraCombatLog.h"

namespace VeyraAttributePolicy
{
namespace
{
	struct FRule
	{
		FGameplayAttribute Attribute;
		EVeyraModifierRule Rule;
	};

	const TArray<FRule>& Rules()
	{
		static const TArray<FRule> Table = {
			{ UVeyraVitalsSet::GetHealthAttribute(), EVeyraModifierRule::None },
			{ UVeyraVitalsSet::GetMaxHealthAttribute(), EVeyraModifierRule::Stat },
			{ UVeyraVitalsSet::GetIncomingPhysicalDamageAttribute(), EVeyraModifierRule::None },
			{ UVeyraVitalsSet::GetIncomingMagicDamageAttribute(), EVeyraModifierRule::None },
			{ UVeyraVitalsSet::GetIncomingTrueDamageAttribute(), EVeyraModifierRule::None },
			{ UVeyraOffenceSet::GetOutgoingDamageMultiplierAttribute(), EVeyraModifierRule::Percentage },
			{ UVeyraOffenceSet::GetPhysicalPenetrationFlatAttribute(), EVeyraModifierRule::Flat },
			{ UVeyraOffenceSet::GetPhysicalPenetrationRetainedAttribute(), EVeyraModifierRule::Percentage },
			{ UVeyraOffenceSet::GetMagicPenetrationFlatAttribute(), EVeyraModifierRule::Flat },
			{ UVeyraOffenceSet::GetMagicPenetrationRetainedAttribute(), EVeyraModifierRule::Percentage },
			{ UVeyraDefenceSet::GetArmorAttribute(), EVeyraModifierRule::Stat },
			{ UVeyraDefenceSet::GetMagicResistAttribute(), EVeyraModifierRule::Stat },
			{ UVeyraDefenceSet::GetArmorReductionFlatAttribute(), EVeyraModifierRule::Flat },
			{ UVeyraDefenceSet::GetArmorReductionRetainedAttribute(), EVeyraModifierRule::Percentage },
			{ UVeyraDefenceSet::GetMagicResistReductionFlatAttribute(), EVeyraModifierRule::Flat },
			{ UVeyraDefenceSet::GetMagicResistReductionRetainedAttribute(), EVeyraModifierRule::Percentage },
			{ UVeyraDefenceSet::GetIncomingDamageMultiplierAttribute(), EVeyraModifierRule::Percentage },
			{ UVeyraMobilitySet::GetMoveSpeedAttribute(), EVeyraModifierRule::Stat },
			{ UVeyraResourceSet::GetResourceAttribute(), EVeyraModifierRule::None },
			{ UVeyraResourceSet::GetMaxResourceAttribute(), EVeyraModifierRule::Stat },
			{ UVeyraResourceSet::GetResourceSpendAttribute(), EVeyraModifierRule::None },
		};
		return Table;
	}

	bool OperationFits(EVeyraModifierRule Rule, EGameplayModOp::Type Operation)
	{
		const bool bFlat = Operation == EGameplayModOp::AddBase;
		const bool bPercentage = Operation == EGameplayModOp::MultiplyCompound;
		switch (Rule)
		{
		case EVeyraModifierRule::None:
			return false;
		case EVeyraModifierRule::Flat:
			return bFlat;
		case EVeyraModifierRule::Percentage:
			return bPercentage;
		case EVeyraModifierRule::Stat:
			return bFlat || bPercentage;
		}
		return false;
	}
}

TOptional<EVeyraModifierRule> RuleFor(const FGameplayAttribute& Attribute)
{
	for (const FRule& Entry : Rules())
	{
		if (Entry.Attribute == Attribute)
		{
			return Entry.Rule;
		}
	}
	return {};
}

TArray<FString> Check(const UGameplayEffect& Effect)
{
	TArray<FString> Problems;
	const bool bChangesBaseValues = Effect.DurationPolicy == EGameplayEffectDurationType::Instant || Effect.Period.Value > 0.0f;

	for (const FGameplayModifierInfo& Modifier : Effect.Modifiers)
	{
		const FString Attribute = Modifier.Attribute.GetName();
		const TOptional<EVeyraModifierRule> Rule = RuleFor(Modifier.Attribute);
		if (!Rule.IsSet())
		{
			Problems.Add(FString::Printf(TEXT("%s has no modifier rule"), *Attribute));
		}
		else if (Rule.GetValue() == EVeyraModifierRule::None)
		{
			Problems.Add(FString::Printf(TEXT("%s cannot be changed by a modifier"), *Attribute));
		}
		else if (bChangesBaseValues)
		{
			Problems.Add(FString::Printf(TEXT("a modifier on %s must belong to a duration effect, not an instant or periodic one"), *Attribute));
		}
		else if (!OperationFits(Rule.GetValue(), Modifier.ModifierOp))
		{
			Problems.Add(FString::Printf(TEXT("%s cannot use %s; flat changes use AddBase and percentages use MultiplyCompound"),
				*Attribute, *EGameplayModOpToString(Modifier.ModifierOp)));
		}
	}

	for (const FGameplayEffectExecutionDefinition& Execution : Effect.Executions)
	{
		if (Execution.CalculationClass != UVeyraDamageExecution::StaticClass() && Execution.CalculationClass != UVeyraResourceSpendExecution::StaticClass())
		{
			Problems.Add(FString::Printf(TEXT("execution %s is not a Veyra execution"), *GetNameSafe(Execution.CalculationClass)));
		}
	}
	return Problems;
}

bool AllowsSpec(const FActiveGameplayEffectsContainer& /*ActiveEffects*/, const FGameplayEffectSpec& Spec)
{
	if (!Spec.Def)
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Rejected a Gameplay Effect spec without a definition."));
		return false;
	}
	const TArray<FString> Problems = Check(*Spec.Def);
	if (Problems.IsEmpty())
	{
		return true;
	}
	UE_LOG(LogVeyraCombat, Error, TEXT("Rejected %s: %s."), *GetNameSafe(Spec.Def.Get()), *FString::Join(Problems, TEXT("; ")));
	return false;
}
}
