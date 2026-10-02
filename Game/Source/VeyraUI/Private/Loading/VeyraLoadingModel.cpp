// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Loading/VeyraLoadingModel.h"

#include "Math/RandomStream.h"

namespace VeyraLoadingModel
{
TOptional<EVeyraLoadingStage> StageOf(TOptional<EVeyraMatchPhase> Phase, bool bHasOwnVanguard)
{
	if (Phase.IsSet() && Phase.GetValue() != EVeyraMatchPhase::Loading)
	{
		return {};
	}
	return Phase.IsSet() && bHasOwnVanguard ? EVeyraLoadingStage::WaitingForPlayers : EVeyraLoadingStage::LoadingMatch;
}

EVeyraLoadingContent ParseContent(const FString& Option)
{
	return Option == TEXT("TipsOnly") ? EVeyraLoadingContent::TipsOnly
		: Option == TEXT("LoreOnly")  ? EVeyraLoadingContent::LoreOnly
		: Option == TEXT("Off")		  ? EVeyraLoadingContent::Off
									  : EVeyraLoadingContent::Both;
}

TArray<FVeyraLoadingEntry> EntriesFor(EVeyraLoadingContent Content, TConstArrayView<FText> Tips, TConstArrayView<FText> Lore)
{
	TArray<FVeyraLoadingEntry> Entries;
	if (Content == EVeyraLoadingContent::Both || Content == EVeyraLoadingContent::TipsOnly)
	{
		for (const FText& Tip : Tips)
		{
			Entries.Add(FVeyraLoadingEntry{ Tip, /*bLore*/ false });
		}
	}
	if (Content == EVeyraLoadingContent::Both || Content == EVeyraLoadingContent::LoreOnly)
	{
		for (const FText& Fact : Lore)
		{
			Entries.Add(FVeyraLoadingEntry{ Fact, /*bLore*/ true });
		}
	}
	return Entries;
}

FVeyraLoadingRotation Start(int32 Count, int32 Seed, double Now)
{
	FVeyraLoadingRotation Rotation;
	Rotation.ShownAt = Now;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Rotation.Order.Add(Index);
	}
	// Fisher-Yates: every order equally likely, so no entry always comes first.
	FRandomStream Stream(Seed);
	for (int32 Index = Count - 1; Index > 0; --Index)
	{
		Rotation.Order.Swap(Index, Stream.RandRange(0, Index));
	}
	return Rotation;
}

double SecondsFor(const FText& Text, const FVeyraLoadingTiming& Timing)
{
	const int32 Beyond = FMath::Max(0, Text.ToString().Len() - Timing.BaseCharacters);
	return Timing.MinimumSeconds + static_cast<double>(Beyond) / FMath::Max(1, Timing.CharactersPerSecond);
}

bool Advance(FVeyraLoadingRotation& Rotation, TConstArrayView<FVeyraLoadingEntry> Entries, const FVeyraLoadingTiming& Timing, double Now)
{
	const TOptional<int32> Shown = Rotation.Shown();
	if (Rotation.bManual || !Shown.IsSet() || !Entries.IsValidIndex(Shown.GetValue()) || Rotation.Order.Num() < 2
		|| Now - Rotation.ShownAt < SecondsFor(Entries[Shown.GetValue()].Text, Timing))
	{
		return false;
	}
	// Through the shuffled order, then round again: each entry once before any repeats.
	Rotation.Position = (Rotation.Position + 1) % Rotation.Order.Num();
	Rotation.ShownAt = Now;
	return true;
}

void Browse(FVeyraLoadingRotation& Rotation, int32 Step, double Now)
{
	const int32 Count = Rotation.Order.Num();
	if (Count == 0)
	{
		return;
	}
	Rotation.Position = ((Rotation.Position + Step) % Count + Count) % Count;
	Rotation.ShownAt = Now;
	Rotation.bManual = true;
}
}
