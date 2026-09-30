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
	if (!ScoreboardKey.IsValid())
	{
		Problems.Add(TEXT("ScoreboardKey: a key is required."));
	}
	else if (ScoreboardKey == MatchMenuKey || ScoreboardKey == ShopKey)
	{
		Problems.Add(TEXT("ScoreboardKey: the menu or the shop already uses that key."));
	}
	if (!ChatKey.IsValid())
	{
		Problems.Add(TEXT("ChatKey: a key is required."));
	}
	else if (ChatKey == MatchMenuKey || ChatKey == ShopKey || ChatKey == ScoreboardKey)
	{
		Problems.Add(TEXT("ChatKey: the menu, the shop or the scoreboard already uses that key."));
	}
	return Problems;
}
