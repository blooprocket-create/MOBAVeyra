// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraBotsTuning.h"

#include "Algo/Count.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraItemsTuning.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"

namespace VeyraBots
{
namespace
{
	/** Whether Part goes into Whole's recipe, at any depth. */
	bool IsPartOf(const FVeyraItemsTuning& Items, const FVeyraContentId& Part, const FVeyraContentId& Whole)
	{
		const FVeyraItemDefinition* Definition = Items.Items.Find(Whole);
		if (!Definition)
		{
			return false;
		}
		for (const FVeyraContentId& Component : Definition->Components)
		{
			if (Component == Part || IsPartOf(Items, Part, Component))
			{
				return true;
			}
		}
		return false;
	}
}

TArray<FString> Validate(const FVeyraBotsTuning& Tuning)
{
	TArray<FString> Problems;
	const FVeyraItemsTuning& Items = UVeyraItemsTuningSubsystem::Get();
	for (const TPair<FVeyraContentId, FVeyraBotVanguardTuning>& Pair : Tuning.Vanguards)
	{
		const FString Pointer = FString::Printf(TEXT("/vanguards/%s"), *Pair.Key.ToString());
		const FVeyraBotVanguardTuning& Vanguard = Pair.Value;
		for (int32 Index = 0; Index < Vanguard.Build.Num(); ++Index)
		{
			const FVeyraContentId& Item = Vanguard.Build[Index];
			if (!Items.Items.Contains(Item))
			{
				Problems.Add(FString::Printf(TEXT("%s/build/%d: names \"%s\", which Items.json does not sell"), *Pointer, Index, *Item.ToString()));
				continue;
			}
			for (int32 Other = 0; Other < Vanguard.Build.Num(); ++Other)
			{
				if (Other != Index && (Vanguard.Build[Other] == Item || IsPartOf(Items, Item, Vanguard.Build[Other])))
				{
					Problems.Add(FString::Printf(TEXT("%s/build/%d: \"%s\" is listed twice or is part of \"%s\"'s recipe, which would take it apart"), *Pointer,
						Index, *Item.ToString(), *Vanguard.Build[Other].ToString()));
					break;
				}
			}
		}
		for (const EVeyraBotSkill Skill : { EVeyraBotSkill::Q, EVeyraBotSkill::W, EVeyraBotSkill::E })
		{
			const int32 Count = Algo::Count(Vanguard.SkillPriority, Skill);
			if (Count != 1)
			{
				Problems.Add(FString::Printf(TEXT("%s/skillPriority: must name Q, W and E once each"), *Pointer));
				break;
			}
		}
	}

	// Every released Vanguard can be a bot, so each needs its entry, covering its whole kit.
	for (const TPair<FVeyraContentId, FVeyraVanguardDefinition>& Pair : UVeyraVanguardsTuningSubsystem::Get().Vanguards)
	{
		if (Pair.Value.Availability != EVeyraVanguardAvailability::Playable)
		{
			continue;
		}
		const FVeyraBotVanguardTuning* Vanguard = Tuning.Vanguards.Find(Pair.Key);
		if (!Vanguard)
		{
			Problems.Add(FString::Printf(TEXT("/vanguards: has no entry for %s, which Vanguards.json releases"), *Pair.Key.ToString()));
			continue;
		}
		const FVeyraVanguardKitTuning& Kit = Pair.Value.Abilities;
		for (const TArray<FVeyraContentId>* Slot : { &Kit.Q, &Kit.W, &Kit.E, &Kit.R })
		{
			for (const FVeyraContentId& Ability : *Slot)
			{
				if (!Vanguard->Abilities.Contains(Ability))
				{
					Problems.Add(FString::Printf(TEXT("/vanguards/%s/abilities: says nothing of %s, in its kit"), *Pair.Key.ToString(), *Ability.ToString()));
				}
			}
		}
	}

	// Each seat takes roster spells, none twice, and knows what each is for (ADR-015 §8).
	const TArray<FVeyraContentId>& Roster = UVeyraAbilitiesTuningSubsystem::Get().FluxSpells.Roster;
	for (int32 Index = 0; Index < Tuning.Seats.Num(); ++Index)
	{
		const FString Pointer = FString::Printf(TEXT("/seats/%d/fluxSpells"), Index);
		const TArray<FVeyraContentId>& Spells = Tuning.Seats[Index].FluxSpells;
		for (int32 SpellIndex = 0; SpellIndex < Spells.Num(); ++SpellIndex)
		{
			const FVeyraContentId& Spell = Spells[SpellIndex];
			if (!Roster.Contains(Spell))
			{
				Problems.Add(FString::Printf(TEXT("%s: %s is not on Abilities.json's roster"), *Pointer, *Spell.ToString()));
			}
			else if (Spells.IndexOfByKey(Spell) != SpellIndex)
			{
				Problems.Add(FString::Printf(TEXT("%s: takes %s twice"), *Pointer, *Spell.ToString()));
			}
			else if (!Tuning.FluxSpells.Contains(Spell))
			{
				Problems.Add(FString::Printf(TEXT("/fluxSpells: says nothing of %s, which seat %d takes"), *Spell.ToString(), Index));
			}
		}
	}
	return Problems;
}
}
