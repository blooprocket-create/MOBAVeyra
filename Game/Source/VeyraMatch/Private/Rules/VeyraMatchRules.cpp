// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Rules/VeyraMatchRules.h"

#include "Slots/VeyraAbilitySlot.h"
#include "Tuning/VeyraMatchTuning.h"
#include "Tuning/VeyraVanguardsTuning.h"

namespace VeyraMatchRules
{
double RespawnDelaySeconds(int32 Level, double MatchClockSeconds, const FVeyraRespawnTuning& Respawn)
{
	if (Respawn.SecondsByLevel.IsEmpty())
	{
		return 0.0;
	}
	const double LevelSeconds = Respawn.SecondsByLevel[FMath::Clamp(Level - 1, 0, Respawn.SecondsByLevel.Num() - 1)];
	constexpr double SecondsPerMinute = 60.0;
	const double MinutesPast = FMath::Max(0.0, MatchClockSeconds - Respawn.Elapsed.StartSeconds) / SecondsPerMinute;
	return LevelSeconds * (1.0 + FMath::Min(Respawn.Elapsed.MaxFraction, MinutesPast * Respawn.Elapsed.FractionPerMinute));
}

EVeyraEndCustomMatchRefusal CheckEndCustomMatch(EVeyraMatchRules Rules, EVeyraMatchPhase Phase, bool bRequesterIsHost)
{
	if (!HasHost(Rules))
	{
		return EVeyraEndCustomMatchRefusal::NotCustomMatch;
	}
	if (Phase == EVeyraMatchPhase::Ended)
	{
		return EVeyraEndCustomMatchRefusal::AlreadyEnded;
	}
	return bRequesterIsHost ? EVeyraEndCustomMatchRefusal::None : EVeyraEndCustomMatchRefusal::NotHost;
}

bool HasHost(EVeyraMatchRules Rules)
{
	return Rules == EVeyraMatchRules::Practice || Rules == EVeyraMatchRules::Custom;
}

bool HasVictory(EVeyraMatchRules Rules, const TOptional<FVeyraCustomSettings>& Custom)
{
	switch (Rules)
	{
	case EVeyraMatchRules::Standard:
		return true;
	case EVeyraMatchRules::Custom:
		return Custom.IsSet() && Custom->bVictoryEnabled;
	case EVeyraMatchRules::Practice:
		return false;
	}
	return false;
}

bool AllowsBuyback(EVeyraMatchRules Rules)
{
	return Rules == EVeyraMatchRules::Standard || Rules == EVeyraMatchRules::Custom;
}

bool DoesPrimeWellWin(bool bHasVictory, EVeyraMatchPhase Phase)
{
	return bHasVictory && Phase == EVeyraMatchPhase::Live;
}

FString CheckAssignedVanguard(const FVeyraContentId& Vanguard, const FVeyraVanguardDefinition* Definition, bool bShipping)
{
	if (!Vanguard.IsValid())
	{
		return TEXT("the Vanguard is missing");
	}
	if (!Definition)
	{
		return FString::Printf(TEXT("Vanguards.json does not define %s"), *Vanguard.ToString());
	}
	if (bShipping && Definition->Availability != EVeyraVanguardAvailability::Playable)
	{
		return FString::Printf(TEXT("%s is a developer Vanguard, which Shipping servers do not host"), *Vanguard.ToString());
	}
	return FString();
}

FString CheckAssignedFluxSpells(TConstArrayView<FVeyraContentId> Spells, TConstArrayView<FVeyraContentId> Roster)
{
	if (Spells.Num() > static_cast<int32>(UE_ARRAY_COUNT(VeyraAbilitySlots::Spells)))
	{
		return FString::Printf(TEXT("%d Flux Spells, but a Vanguard has %d spell slots"), Spells.Num(), static_cast<int32>(UE_ARRAY_COUNT(VeyraAbilitySlots::Spells)));
	}
	for (int32 Index = 0; Index < Spells.Num(); ++Index)
	{
		const FVeyraContentId& Spell = Spells[Index];
		if (!Spell.IsValid())
		{
			continue;
		}
		if (!Roster.Contains(Spell))
		{
			return FString::Printf(TEXT("Flux Spell %s is not on Abilities.json's roster"), *Spell.ToString());
		}
		if (Spells.IndexOfByKey(Spell) != Index)
		{
			return FString::Printf(TEXT("Flux Spell %s is in two slots"), *Spell.ToString());
		}
	}
	return FString();
}
}
