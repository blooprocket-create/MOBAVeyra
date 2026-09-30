// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Internationalization/Text.h"
#include "Misc/Optional.h"
#include "VeyraSettingsRegistry.h"

class FVeyraSettingsStore;

/** A value a toggle or a choice may take, as its button shows it. */
struct FVeyraSettingOptionModel
{
	FString Value;
	FText Label;
	bool bSelected = false;
};

/** One setting as the Settings screen shows it (Settings Bible §6.3). */
struct FVeyraSettingRowModel
{
	FVeyraContentId Id;
	EVeyraSettingKind Kind = EVeyraSettingKind::Toggle;
	EVeyraSettingCategory Category = EVeyraSettingCategory::Controls;
	FText Name;
	FText Description;
	/** The value as players read it: On or Off, an option's name, or the number. */
	FText ValueText;
	/** It differs from its default, so it offers Reset. */
	bool bChanged = false;
	/** It takes effect only after the game restarts (§6.3). */
	bool bAfterRestart = false;
	/** It changes only outside matches, and this is a live match (§6.2): it shows, but cannot change. */
	bool bLocked = false;
	/** A toggle's On and Off, or a choice's options, in order. */
	TArray<FVeyraSettingOptionModel> Options;
	/** A range's values one step down and one step up; unset at its bounds. */
	TOptional<FString> Lower;
	TOptional<FString> Higher;
};

/** The Settings screen: its categories, the settings it shows, and what it offers. */
struct FVeyraSettingsModel
{
	/** The categories that hold settings, in the layout's order: only those with real effects appear (ADR-024 §5). */
	TArray<EVeyraSettingCategory> Categories;
	/** The category shown while not searching. */
	EVeyraSettingCategory Category = EVeyraSettingCategory::Controls;
	/** Searching: the rows are every setting the search finds, each naming its category (§6.3). */
	bool bSearching = false;
	TArray<FVeyraSettingRowModel> Rows;
	/** One-step Undo has a change to take back (§6.1). */
	bool bCanUndo = false;
	/** A setting the shown category's reset, or the reset of everything, would change. */
	bool bCategoryChanged = false;
	bool bAnyChanged = false;
};

/** The Settings screen's model, apart from its widgets, so tests can read it (ADR-024 §5). */
namespace VeyraSettingsModels
{
	VEYRAUI_API FText CategoryName(EVeyraSettingCategory Category);

	/** Whether Search finds Setting: part of its name or of its search words, ignoring case. An empty search finds nothing. */
	VEYRAUI_API bool Matches(const FVeyraSettingInfo& Setting, const FString& Search);

	/**
	 * The screen over Store: the settings of Category, or of the first category when it holds none;
	 * with Search, every setting it finds instead. In a live match, the settings that change only
	 * outside matches are locked.
	 */
	VEYRAUI_API FVeyraSettingsModel Describe(const FVeyraSettingsStore& Store, EVeyraSettingCategory Category, const FString& Search, bool bInLiveMatch);
}
