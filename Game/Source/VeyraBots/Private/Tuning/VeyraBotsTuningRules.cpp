// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraBotsTuning.h"

#include "Algo/Count.h"
#include "Brain/VeyraBotAbilities.h"
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
		// Each ability it names is one a bot knows how to aim: one that no archetype defines would
		// never be cast. One cast at a foe, to damage, engage, escape or secure, must reach one: an
		// ability that reaches only allies is cast to defend.
		const FVeyraVanguardDefinition* Definition = UVeyraVanguardsTuningSubsystem::FindVanguard(Pair.Key);
		for (const TPair<FVeyraContentId, EVeyraBotAbilityUse>& Use : Vanguard.Abilities)
		{
			const TOptional<FVeyraBotAbilityProfile> Profile = VeyraBotAbilities::ProfileOf(Use.Key, Definition ? Definition->BasicAttack.Range : 0.0);
			if (!Profile.IsSet())
			{
				Problems.Add(FString::Printf(TEXT("%s/abilities: %s is not an ability a bot can aim"), *Pointer, *Use.Key.ToString()));
			}
			else if (Definition && Use.Value != EVeyraBotAbilityUse::Defend && Use.Value != EVeyraBotAbilityUse::Empower && Use.Value != EVeyraBotAbilityUse::Never
				&& !(Profile->Reach > 0.0))
			{
				Problems.Add(FString::Printf(TEXT("%s/abilities: %s reaches no enemy, so it would never be cast at one"), *Pointer, *Use.Key.ToString()));
			}
		}
		// It names the roles it plays, each once (ADR-039 §5).
		const TSet<EVeyraBotRole> Distinct(Vanguard.Roles);
		if (Vanguard.Roles.IsEmpty() || Distinct.Num() != Vanguard.Roles.Num())
		{
			Problems.Add(FString::Printf(TEXT("%s/roles: names at least one role, each once"), *Pointer));
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

	// Each warding seat is a seat, named once (ADR-016 §7).
	for (int32 Index = 0; Index < Tuning.Warding.Seats.Num(); ++Index)
	{
		const int32 Seat = Tuning.Warding.Seats[Index];
		if (!Tuning.Seats.IsValidIndex(Seat))
		{
			Problems.Add(FString::Printf(TEXT("/warding/seats/%d: seat %d is not one of the %d seats"), Index, Seat, Tuning.Seats.Num()));
		}
		else if (Tuning.Warding.Seats.IndexOfByKey(Seat) != Index)
		{
			Problems.Add(FString::Printf(TEXT("/warding/seats/%d: names seat %d twice"), Index, Seat));
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
