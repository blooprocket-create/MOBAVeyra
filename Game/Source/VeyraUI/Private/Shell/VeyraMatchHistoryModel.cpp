// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraMatchHistoryModel.h"

#include "Tuning/VeyraVanguardsTuningSubsystem.h"

#define LOCTEXT_NAMESPACE "VeyraMatchHistory"

namespace VeyraMatchHistoryModel
{
FText OutcomeText(const FString& Outcome, bool bPersonalLoss)
{
	if (bPersonalLoss)
	{
		return LOCTEXT("PersonalLoss", "Defeat (personal)");
	}
	if (Outcome == TEXT("win"))
	{
		return LOCTEXT("Win", "Victory");
	}
	if (Outcome == TEXT("loss"))
	{
		return LOCTEXT("Loss", "Defeat");
	}
	return LOCTEXT("NoContest", "No Contest");
}

FVeyraHistoryModel Describe(const FVeyraClientSnapshot& Snapshot, bool bCanLoadMore)
{
	const FVeyraMatchHistory& History = Snapshot.History;
	FVeyraHistoryModel Model;
	if (History.Opened.IsSet())
	{
		Model.Opened = VeyraShellModels::DescribeOutcome(*History.Opened);
		return Model;
	}

	DescribeFilters(Snapshot, History.Filter, History.Modes, Model.Vanguards, Model.Modes, Model.Outcomes);
	for (const VeyraBackendProtocol::FHistoryEntry& Entry : History.Entries)
	{
		Model.Rows.Add(DescribeRow(Entry));
	}
	if (Model.Rows.IsEmpty())
	{
		Model.Empty = History.bLoaded ? LOCTEXT("NoMatches", "No completed matches fit these filters.") : LOCTEXT("Loading", "Loading your matches...");
	}
	Model.bOffersLoadMore = bCanLoadMore;
	return Model;
}

void DescribeFilters(const FVeyraClientSnapshot& Snapshot, const VeyraBackendProtocol::FHistoryFilter& Filter, TConstArrayView<FString> SavedModes,
	TArray<FVeyraHistoryOption>& OutVanguards, TArray<FVeyraHistoryOption>& OutModes, TArray<FVeyraHistoryOption>& OutOutcomes)
{
	// Every released Vanguard, not only those on the pages read: the filters reach every record (UX-67).
	OutVanguards.Add({ FString(), LOCTEXT("AllVanguards", "All Vanguards"), Filter.VanguardId.IsEmpty() });
	TArray<FString> Released;
	for (const TPair<FVeyraContentId, FVeyraVanguardDefinition>& Vanguard : UVeyraVanguardsTuningSubsystem::Get().Vanguards)
	{
		if (Vanguard.Value.Availability == EVeyraVanguardAvailability::Playable)
		{
			Released.Add(Vanguard.Key.ToString());
		}
	}
	Released.Sort();
	for (const FString& Vanguard : Released)
	{
		OutVanguards.Add({ Vanguard, VeyraShellModels::VanguardNameOf(Vanguard), Filter.VanguardId == Vanguard });
	}

	// The modes the Play screen offers, and every mode the player has a saved match in, such as
	// practice, whichever pages are read (UX-67).
	TArray<FString> Modes;
	for (const VeyraBackendProtocol::FModeInfo& Mode : Snapshot.Modes)
	{
		Modes.AddUnique(Mode.Id);
	}
	for (const FString& Mode : SavedModes)
	{
		Modes.AddUnique(Mode);
	}
	if (!Filter.Mode.IsEmpty())
	{
		Modes.AddUnique(Filter.Mode);
	}
	OutModes.Add({ FString(), LOCTEXT("AllModes", "All Modes"), Filter.Mode.IsEmpty() });
	for (const FString& Mode : Modes)
	{
		OutModes.Add({ Mode, VeyraShellModels::ModeNameOf(Mode), Filter.Mode == Mode });
	}

	OutOutcomes.Add({ FString(), LOCTEXT("AllOutcomes", "All Outcomes"), Filter.Outcome.IsEmpty() });
	for (const TCHAR* Outcome : { TEXT("win"), TEXT("loss"), TEXT("no_contest") })
	{
		OutOutcomes.Add({ Outcome, OutcomeText(Outcome), Filter.Outcome == Outcome });
	}
}

FVeyraHistoryRow DescribeRow(const VeyraBackendProtocol::FHistoryEntry& Entry)
{
	FVeyraHistoryRow Row;
	Row.MatchId = Entry.MatchId;
	Row.Summary = FText::Format(LOCTEXT("Row", "{0}   {1}   {2}   {3}   {4}"), FText::AsDateTime(Entry.EndedAt, EDateTimeStyle::Medium, EDateTimeStyle::Short),
		VeyraShellModels::ModeNameOf(Entry.Mode), VeyraShellModels::FormatCountdown(Entry.DurationSeconds),
		Entry.VanguardId.IsEmpty() ? LOCTEXT("UnknownVanguard", "Unknown Vanguard") : VeyraShellModels::VanguardNameOf(Entry.VanguardId),
		OutcomeText(Entry.Outcome, Entry.bPersonalLoss));
	return Row;
}
}

#undef LOCTEXT_NAMESPACE
