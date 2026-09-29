// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraUIInputSettings.h"

TArray<FString> UVeyraUIInputSettings::Validate() const
{
	TArray<FString> Problems;
	if (!MatchMenuKey.IsValid())
	{
		Problems.Add(TEXT("MatchMenuKey: a key is required."));
	}
	if (!ShopKey.IsValid())
	{
		Problems.Add(TEXT("ShopKey: a key is required."));
	}
	else if (ShopKey == MatchMenuKey)
	{
		Problems.Add(TEXT("ShopKey: the menu already uses that key."));
	}
	return Problems;
}
