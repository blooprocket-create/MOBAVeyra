// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Pings/VeyraPingRules.h"

#include "Tuning/VeyraMatchTuning.h"

namespace VeyraPings
{
bool Allow(TArray<double>& SentAt, double Now, const FVeyraPingsTuning& Tuning)
{
	SentAt.RemoveAll([Now, &Tuning](double At) { return Now - At >= Tuning.WindowSeconds; });
	if (SentAt.Num() >= Tuning.MaxPerWindow)
	{
		return false;
	}
	SentAt.Add(Now);
	return true;
}

void Forget(TArray<FVeyraReceivedPing>& Held, double Now, const FVeyraPingsTuning& Tuning)
{
	Held.RemoveAll([Now, &Tuning](const FVeyraReceivedPing& Ping) { return Now - Ping.ReceivedAt >= Tuning.KeepSeconds; });
}
}
