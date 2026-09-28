// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraUIInputSettings.h"

TArray<FString> UVeyraUIInputSettings::Validate() const
{
	TArray<FString> Problems;
	if (!MatchMenuKey.IsValid())
	{
		Problems.Add(TEXT("MatchMenuKey: a key is required."));
	}
	return Problems;
}
