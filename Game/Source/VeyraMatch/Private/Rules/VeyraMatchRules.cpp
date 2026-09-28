// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Rules/VeyraMatchRules.h"

#include "Tuning/VeyraVanguardsTuning.h"

namespace VeyraMatchRules
{
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
