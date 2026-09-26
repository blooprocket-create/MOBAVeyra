// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Absorption/VeyraAbsorptionLedger.h"

#include "Math/UnrealMathUtility.h"

namespace VeyraAbsorption
{
namespace
{
	using FShieldCategories = TArray<EVeyraShieldCategory, TInlineAllocator<2>>;

	// Shield priority (§7): the most specialized eligible category first.
	FShieldCategories EligibleCategories(EVeyraDamageType Type)
	{
		switch (Type)
		{
		case EVeyraDamageType::Physical:
			return { EVeyraShieldCategory::Physical, EVeyraShieldCategory::Universal };
		case EVeyraDamageType::Magic:
			return { EVeyraShieldCategory::Magic, EVeyraShieldCategory::Universal };
		case EVeyraDamageType::True:
			return { EVeyraShieldCategory::Universal };
		}
		return {};
	}

	/**
	 * Spends Amount from the eligible entries, oldest first, and removes the entries it empties.
	 * Returns what is left of Amount.
	 */
	template <typename EntryType, typename PredicateType>
	double SpendOldestFirst(TArray<EntryType>& Entries, double Amount, PredicateType IsEligible, double& OutSpent, TArray<int32>& OutDepleted)
	{
		TArray<int32, TInlineAllocator<8>> Order;
		for (int32 Index = 0; Index < Entries.Num(); ++Index)
		{
			if (IsEligible(Entries[Index]))
			{
				Order.Add(Index);
			}
		}
		Order.Sort([&Entries](int32 A, int32 B) { return Entries[A].Sequence < Entries[B].Sequence; });

		TArray<int32, TInlineAllocator<8>> Emptied;
		for (const int32 Index : Order)
		{
			if (Amount <= 0.0)
			{
				break;
			}
			EntryType& Entry = Entries[Index];
			const double Spent = FMath::Min(Amount, Entry.Remaining);
			Entry.Remaining -= Spent;
			Amount -= Spent;
			OutSpent += Spent;
			if (Entry.Remaining <= 0.0)
			{
				Emptied.Add(Entry.Sequence);
			}
		}

		Entries.RemoveAll([&Emptied](const EntryType& Entry) { return Emptied.Contains(Entry.Sequence); });
		OutDepleted.Append(Emptied);
		return Amount;
	}
}

FVeyraAbsorptionResult Absorb(EVeyraDamageType Type, double Amount, bool bInvulnerable, FVeyraAbsorptionLedger& Ledger, double Health)
{
	FVeyraAbsorptionResult Result;

	// Step 7: Invulnerability ends the event before anything is consumed (author ruling, 2026-09-25).
	if (bInvulnerable)
	{
		Result.bBlockedByInvulnerability = true;
		return Result;
	}

	double Remaining = FMath::Max(0.0, Amount);

	// Step 8: shields.
	for (const EVeyraShieldCategory Category : EligibleCategories(Type))
	{
		Remaining = SpendOldestFirst(Ledger.Shields, Remaining,
			[Category](const FVeyraShieldEntry& Shield) { return Shield.Category == Category; },
			Result.ShieldAbsorbed, Result.DepletedShields);
	}

	// Step 9: Temporary Health for every damage type, then ordinary Health.
	Remaining = SpendOldestFirst(Ledger.TemporaryHealth, Remaining,
		[](const FVeyraTemporaryHealthGrant&) { return true; },
		Result.TemporaryHealthSpent, Result.DepletedTemporaryHealth);

	Result.HealthLost = FMath::Min(Remaining, FMath::Max(0.0, Health));
	Result.Overkill = Remaining - Result.HealthLost;
	return Result;
}

double TotalTemporaryHealth(const FVeyraAbsorptionLedger& Ledger)
{
	double Total = 0.0;
	for (const FVeyraTemporaryHealthGrant& Grant : Ledger.TemporaryHealth)
	{
		Total += Grant.Remaining;
	}
	return Total;
}

double GetEffectiveHealth(double Health, const FVeyraAbsorptionLedger& Ledger)
{
	return Health + TotalTemporaryHealth(Ledger);
}

double GetEffectiveMaxHealth(double MaxHealth, const FVeyraAbsorptionLedger& Ledger)
{
	return MaxHealth + TotalTemporaryHealth(Ledger);
}
}
