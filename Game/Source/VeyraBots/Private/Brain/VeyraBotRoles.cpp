// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Brain/VeyraBotRoles.h"

namespace VeyraBotRoles
{
TArray<int32> Deal(TConstArrayView<EVeyraBotRole> Places, TConstArrayView<TArray<EVeyraBotRole>> Preferences)
{
	const int32 Count = FMath::Min(Places.Num(), Preferences.Num());
	TArray<int32> PlaceOf;
	PlaceOf.Init(INDEX_NONE, Count);
	// The Jungle places first: a team's jungler is the place a bot most needs its kit for.
	TArray<int32> Order;
	for (const bool bJungle : { true, false })
	{
		for (int32 Place = 0; Place < Count; ++Place)
		{
			if ((Places[Place] == EVeyraBotRole::Jungle) == bJungle)
			{
				Order.Add(Place);
			}
		}
	}
	for (const int32 Place : Order)
	{
		int32 Best = INDEX_NONE;
		int32 BestRank = MAX_int32;
		for (int32 Bot = 0; Bot < Count; ++Bot)
		{
			if (PlaceOf[Bot] != INDEX_NONE)
			{
				continue;
			}
			const int32 Found = Preferences[Bot].IndexOfByKey(Places[Place]);
			const int32 Rank = Found == INDEX_NONE ? MAX_int32 - 1 : Found;
			if (Rank < BestRank)
			{
				Best = Bot;
				BestRank = Rank;
			}
		}
		if (Best != INDEX_NONE)
		{
			PlaceOf[Best] = Place;
		}
	}
	return PlaceOf;
}
}
