// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraVanguardsTuning.h"

namespace VeyraVanguardRules
{
TArray<FString> Validate(const FVeyraVanguardsTuning& Tuning, const FVeyraAbilitiesTuning& Abilities, int32 BasicAbilityMaxRank, int32 UltimateMaxRank)
{
	TArray<FString> Problems;
	const auto Problem = [&Problems](const FString& Pointer, const FString& Message) { Problems.Add(FString::Printf(TEXT("%s: %s"), *Pointer, *Message)); };

	for (const TPair<FVeyraContentId, FVeyraVanguardDefinition>& Entry : Tuning.Vanguards)
	{
		const FString Pointer = TEXT("/vanguards/") + Entry.Key.ToString();
		const FVeyraVanguardDefinition& Vanguard = Entry.Value;
		// A capsule's half height includes its rounded ends, so it can never be shorter than its radius.
		if (Vanguard.Body.CapsuleHalfHeight < Vanguard.Body.CapsuleRadius)
		{
			Problem(Pointer + TEXT("/body/capsuleHalfHeight"), TEXT("must be at least capsuleRadius"));
		}
		for (const FString& AttackProblem : VeyraBasicAttacks::Validate(Vanguard.BasicAttack))
		{
			Problem(Pointer + TEXT("/basicAttack"), AttackProblem);
		}

		struct FSlot
		{
			const TCHAR* Name;
			const TArray<FVeyraContentId>* Ids;
			int32 RankCount;
		};
		const FSlot Slots[] = { { TEXT("q"), &Vanguard.Abilities.Q, BasicAbilityMaxRank }, { TEXT("w"), &Vanguard.Abilities.W, BasicAbilityMaxRank },
			{ TEXT("e"), &Vanguard.Abilities.E, BasicAbilityMaxRank }, { TEXT("r"), &Vanguard.Abilities.R, UltimateMaxRank } };
		TArray<FVeyraContentId, TInlineAllocator<4>> Kit;
		for (const FSlot& Slot : Slots)
		{
			for (int32 Index = 0; Index < Slot.Ids->Num(); ++Index)
			{
				const FVeyraContentId& Ability = (*Slot.Ids)[Index];
				const FString SlotPointer = FString::Printf(TEXT("%s/abilities/%s/%d"), *Pointer, Slot.Name, Index);
				if (!VeyraAbilityRules::Defines(Abilities, Ability))
				{
					Problem(SlotPointer, FString::Printf(TEXT("names ability \"%s\", which Abilities.json does not define"), *Ability.ToString()));
					continue;
				}
				for (const FString& RankProblem : VeyraAbilityRules::ValidateRanks(Abilities, Ability, Slot.RankCount))
				{
					Problem(SlotPointer, TEXT("in this slot, ") + RankProblem);
				}
				// A cast finds its rank by its ability, so one ability fills one slot.
				if (Kit.Contains(Ability))
				{
					Problem(SlotPointer, TEXT("is already in another slot; each slot has its own ability"));
				}
				Kit.Add(Ability);
			}
		}

		for (int32 Index = 0; Index < Vanguard.Passive.Num(); ++Index)
		{
			if (!Tuning.DeepFoundation.Contains(Vanguard.Passive[Index]) && !Tuning.HitChain.Contains(Vanguard.Passive[Index]))
			{
				Problem(FString::Printf(TEXT("%s/passive/%d"), *Pointer, Index),
					FString::Printf(TEXT("names passive \"%s\", which no passive map defines"), *Vanguard.Passive[Index].ToString()));
			}
		}
	}

	for (const TPair<FVeyraContentId, FVeyraDeepFoundationTuning>& Entry : Tuning.DeepFoundation)
	{
		// Passives have no ranks.
		if (Entry.Value.Shield.AmountByRank.Num() != 1)
		{
			Problem(FString::Printf(TEXT("/deepFoundation/%s/shield/amountByRank"), *Entry.Key.ToString()), TEXT("holds exactly one value: a passive has no ranks"));
		}
	}

	for (const TPair<FVeyraContentId, FVeyraHitChainTuning>& Entry : Tuning.HitChain)
	{
		const FString Pointer = TEXT("/hitChain/") + Entry.Key.ToString();
		if (!Abilities.Statuses.Contains(Entry.Value.Status))
		{
			Problem(Pointer + TEXT("/status"), FString::Printf(TEXT("names status \"%s\", which Abilities.json does not define"), *Entry.Value.Status.ToString()));
		}
		// The passive archetype runs a passive by the map that defines it, so each ID is in one map.
		if (Tuning.DeepFoundation.Contains(Entry.Key))
		{
			Problem(Pointer, TEXT("is also defined in /deepFoundation; a passive belongs to one passive map"));
		}
	}
	return Problems;
}
}
