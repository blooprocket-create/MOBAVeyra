// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Hud/VeyraKillFeedModel.h"

#include "Text/VeyraContentText.h"

namespace VeyraKillFeedView
{
namespace
{
	/** A Vanguard as a line names it: its Vanguard's name, which says more than a bot's player name. */
	FString VanguardOf(const FVeyraContentId& Vanguard, const FString& PlayerName)
	{
		return Vanguard.IsValid() ? VeyraContentText::VanguardName(Vanguard).ToString() : PlayerName;
	}

	const TCHAR* LaneWord(EVeyraLane Lane)
	{
		switch (Lane)
		{
		case EVeyraLane::Top:
			return TEXT("top");
		case EVeyraLane::Mid:
			return TEXT("mid");
		case EVeyraLane::Bottom:
			return TEXT("bottom");
		}
		return TEXT("");
	}

	const TCHAR* SpireWord(int32 Order)
	{
		switch (Order)
		{
		case 0:
			return TEXT("outer");
		case 1:
			return TEXT("middle");
		default:
			return TEXT("inner");
		}
	}

	/** Whether an announcement of Line is good news for OwnSide: what fell was the enemy's. */
	bool IsGood(const FVeyraKillFeedLine& Line, EVeyraTeam OwnSide)
	{
		return Line.VictimSide != OwnSide;
	}
}

void Forget(TArray<FVeyraKillFeedArrival>& Arrivals, double Now, double KeepSeconds)
{
	Arrivals.RemoveAll([Now, KeepSeconds](const FVeyraKillFeedArrival& Arrival) { return Now - Arrival.ReceivedAt >= KeepSeconds; });
}

TArray<FVeyraKillFeedRow> Rows(TConstArrayView<FVeyraKillFeedArrival> Arrivals, double Now, double ShowSeconds, int32 MaxRows)
{
	TArray<FVeyraKillFeedRow> Shown;
	for (const FVeyraKillFeedArrival& Arrival : Arrivals)
	{
		const double Age = Now - Arrival.ReceivedAt;
		if (ShowSeconds > 0.0 && Age >= 0.0 && Age < ShowSeconds)
		{
			Shown.Add(FVeyraKillFeedRow{ Arrival.Line, Age / ShowSeconds });
		}
	}
	if (MaxRows >= 0 && Shown.Num() > MaxRows)
	{
		Shown.RemoveAt(0, Shown.Num() - MaxRows);
	}
	return Shown;
}

FString StructureName(const FVeyraKillFeedLine& Line)
{
	switch (Line.StructureKind)
	{
	case EVeyraStructureKind::LaneSpire:
		return Line.bHasLane ? FString::Printf(TEXT("%s %s Spire"), LaneWord(Line.Lane), SpireWord(Line.StructureOrder)) : FString(TEXT("Spire"));
	case EVeyraStructureKind::Inhibitor:
		return Line.bHasLane ? FString::Printf(TEXT("%s inhibitor"), LaneWord(Line.Lane)) : FString(TEXT("inhibitor"));
	case EVeyraStructureKind::BaseTower:
		return TEXT("base tower");
	case EVeyraStructureKind::PrimeWell:
		return TEXT("Prime Well");
	}
	return FString();
}

FString AnnouncementOf(const FVeyraKillFeedLine& Line, int32 OwnPlayerId, EVeyraTeam OwnSide)
{
	const FString Victim = VanguardOf(Line.VictimVanguard, Line.VictimName);
	const FString Killer = VanguardOf(Line.KillerVanguard, Line.KillerName);
	switch (Line.Kind)
	{
	case EVeyraKillFeedKind::Structure:
		return Line.VictimSide == OwnSide ? FString::Printf(TEXT("Your %s has fallen"), *StructureName(Line))
										  : FString::Printf(TEXT("Enemy %s destroyed"), *StructureName(Line));
	case EVeyraKillFeedKind::Takedown:
		if (Line.KillerPlayerId != INDEX_NONE && Line.KillerPlayerId == OwnPlayerId)
		{
			return Line.bFirstBlood ? FString::Printf(TEXT("First Blood! You slew %s"), *Victim) : FString::Printf(TEXT("You slew %s"), *Victim);
		}
		if (Line.VictimPlayerId != INDEX_NONE && Line.VictimPlayerId == OwnPlayerId)
		{
			return FString::Printf(TEXT("%s slew you"), *Killer);
		}
		return Line.bFirstBlood ? FString::Printf(TEXT("First Blood! %s slew %s"), *Killer, *Victim) : FString();
	case EVeyraKillFeedKind::Execution:
		return Line.VictimPlayerId != INDEX_NONE && Line.VictimPlayerId == OwnPlayerId ? FString(TEXT("You were executed")) : FString();
	}
	return FString();
}

TOptional<FVeyraAnnouncement> Announcement(TConstArrayView<FVeyraKillFeedArrival> Arrivals, double Now, double Seconds, int32 OwnPlayerId, EVeyraTeam OwnSide)
{
	for (int32 Index = Arrivals.Num() - 1; Index >= 0; --Index)
	{
		const FVeyraKillFeedArrival& Arrival = Arrivals[Index];
		const double Age = Now - Arrival.ReceivedAt;
		if (Seconds <= 0.0 || Age < 0.0 || Age >= Seconds)
		{
			continue;
		}
		const FString Text = AnnouncementOf(Arrival.Line, OwnPlayerId, OwnSide);
		if (!Text.IsEmpty())
		{
			return FVeyraAnnouncement{ Text, IsGood(Arrival.Line, OwnSide), Age / Seconds };
		}
	}
	return {};
}
}
