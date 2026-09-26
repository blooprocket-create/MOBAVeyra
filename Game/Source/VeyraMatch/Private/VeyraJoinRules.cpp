// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraJoinRules.h"

#include "Kismet/GameplayStatics.h"

namespace VeyraJoinRules
{
FString MakeTuningHashOption(const FBlake3Hash& CompositeHash)
{
	return FString::Printf(TEXT("%s=%s"), TuningHashOption, *LexToString(CompositeHash));
}

FString CheckTuningHash(const FString& Options, const FBlake3Hash& ServerHash)
{
	const FString Reported = UGameplayStatics::ParseOption(Options, TuningHashOption);
	const FString Expected = LexToString(ServerHash);
	if (Reported.IsEmpty())
	{
		return FString::Printf(TEXT("The client did not report its tuning hash; this server runs %s."), *Expected);
	}
	if (!Reported.Equals(Expected, ESearchCase::IgnoreCase))
	{
		return FString::Printf(TEXT("Tuning differs between builds: the client has %s and this server has %s."), *Reported, *Expected);
	}
	return FString();
}

FString CheckDirectConnect()
{
#if UE_BUILD_SHIPPING
	return TEXT("This server does not accept direct connections; joining needs the match-join contract.");
#else
	return FString();
#endif
}
}
