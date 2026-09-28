// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Rules/VeyraMatchRules.h"

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
	if (Rules != EVeyraMatchRules::Practice)
	{
		return EVeyraEndCustomMatchRefusal::NotCustomMatch;
	}
	if (Phase == EVeyraMatchPhase::Ended)
	{
		return EVeyraEndCustomMatchRefusal::AlreadyEnded;
	}
	return bRequesterIsHost ? EVeyraEndCustomMatchRefusal::None : EVeyraEndCustomMatchRefusal::NotHost;
}

bool DoesPrimeWellWin(EVeyraMatchRules Rules, EVeyraMatchPhase Phase)
{
	return Rules == EVeyraMatchRules::Standard && Phase == EVeyraMatchPhase::Live;
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
}
