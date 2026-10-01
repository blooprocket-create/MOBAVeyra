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

	// Every released Vanguard, not only those on the pages read: the filters reach every record (UX-67).
	Model.Vanguards.Add({ FString(), LOCTEXT("AllVanguards", "All Vanguards"), History.Filter.VanguardId.IsEmpty() });
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
		Model.Vanguards.Add({ Vanguard, VeyraShellModels::VanguardNameOf(Vanguard), History.Filter.VanguardId == Vanguard });
	}

	// The modes the Play screen offers, and every mode the player has a saved match in, such as
	// practice, whichever pages are read (UX-67).
	TArray<FString> Modes;
	for (const VeyraBackendProtocol::FModeInfo& Mode : Snapshot.Modes)
	{
		Modes.AddUnique(Mode.Id);
	}
	for (const FString& Mode : History.Modes)
	{
		Modes.AddUnique(Mode);
	}
	if (!History.Filter.Mode.IsEmpty())
	{
		Modes.AddUnique(History.Filter.Mode);
	}
	Model.Modes.Add({ FString(), LOCTEXT("AllModes", "All Modes"), History.Filter.Mode.IsEmpty() });
	for (const FString& Mode : Modes)
	{
		Model.Modes.Add({ Mode, VeyraShellModels::ModeNameOf(Mode), History.Filter.Mode == Mode });
	}

	Model.Outcomes.Add({ FString(), LOCTEXT("AllOutcomes", "All Outcomes"), History.Filter.Outcome.IsEmpty() });
	for (const TCHAR* Outcome : { TEXT("win"), TEXT("loss"), TEXT("no_contest") })
	{
		Model.Outcomes.Add({ Outcome, OutcomeText(Outcome), History.Filter.Outcome == Outcome });
	}

	for (const VeyraBackendProtocol::FHistoryEntry& Entry : History.Entries)
	{
		FVeyraHistoryRow& Row = Model.Rows.AddDefaulted_GetRef();
		Row.MatchId = Entry.MatchId;
		Row.Summary = FText::Format(LOCTEXT("Row", "{0}   {1}   {2}   {3}   {4}"), FText::AsDateTime(Entry.EndedAt, EDateTimeStyle::Medium, EDateTimeStyle::Short),
			VeyraShellModels::ModeNameOf(Entry.Mode), VeyraShellModels::FormatCountdown(Entry.DurationSeconds),
			Entry.VanguardId.IsEmpty() ? LOCTEXT("UnknownVanguard", "Unknown Vanguard") : VeyraShellModels::VanguardNameOf(Entry.VanguardId),
			OutcomeText(Entry.Outcome, Entry.bPersonalLoss));
	}
	if (Model.Rows.IsEmpty())
	{
		Model.Empty = History.bLoaded ? LOCTEXT("NoMatches", "No completed matches fit these filters.") : LOCTEXT("Loading", "Loading your matches...");
	}
	Model.bOffersLoadMore = bCanLoadMore;
	return Model;
}
}

#undef LOCTEXT_NAMESPACE
