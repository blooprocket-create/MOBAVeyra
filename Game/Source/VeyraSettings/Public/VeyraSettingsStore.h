// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "VeyraSettingsRegistry.h"

/** What a change to a setting did. */
enum class EVeyraSettingChange : uint8
{
	Changed,
	/** The setting already had that value. */
	Unchanged,
	UnknownSetting,
	/** Not a value the setting takes (VeyraSettings::Normalize). */
	InvalidValue,
	/** The setting may change only outside a live match (Settings Bible §6.2). */
	NotInMatch,
};

/**
 * The player's settings over a registry (ADR-024 §5), with no storage of its own: its owner loads
 * and saves each scope. It keeps only values that differ from their defaults, refuses values a
 * setting does not take, and remembers the most recent change for one-step Undo (Settings Bible §6.1).
 */
class VEYRASETTINGS_API FVeyraSettingsStore
{
public:
	/** Registry must outlive the store. */
	explicit FVeyraSettingsStore(const FVeyraSettingsRegistry& InRegistry);

	const FVeyraSettingsRegistry& GetRegistry() const { return *Registry; }

	/** Id's value: the player's, or its default; empty for an ID the registry does not have. */
	FString Get(const FVeyraContentId& Id) const;

	/** Whether a toggle is On. */
	bool IsOn(const FVeyraContentId& Id) const;

	/** A range's number; 0 for anything else. */
	double GetNumber(const FVeyraContentId& Id) const;

	/** Whether the player's value differs from the default. */
	bool IsChanged(const FVeyraContentId& Id) const { return Values.Contains(Id); }

	/** Sets Id to Value. bInLiveMatch refuses a setting that may change only outside one. */
	EVeyraSettingChange Set(const FVeyraContentId& Id, FStringView Value, bool bInLiveMatch = false);

	/** Returns Id to its default, as a change Undo can take back. */
	EVeyraSettingChange Reset(const FVeyraContentId& Id, bool bInLiveMatch = false);

	/** Returns every setting of Category, or every setting, to its default. Undo does not take these back. */
	void ResetCategory(EVeyraSettingCategory Category);
	void ResetAll();

	/** Takes back the most recent single change, once (Settings Bible §6.1). */
	bool CanUndo() const { return LastChange.IsSet(); }
	bool Undo();

	/** The player's values in Scope that differ from their defaults, as text by setting ID. */
	TMap<FString, FString> SaveScope(EVeyraSettingScope Scope) const;

	/**
	 * Replaces the player's values in Scope with Saved's. A value for a setting the registry lacks, of
	 * another scope, or that the setting does not take is dropped, so an old file never breaks a newer
	 * build. Returns how many were dropped. Undo forgets the last change.
	 */
	int32 LoadScope(EVeyraSettingScope Scope, const TMap<FString, FString>& Saved);

	/** A setting's value changed, whatever changed it. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnChanged, const FVeyraContentId&);
	FOnChanged OnChanged;

private:
	/** Makes Id's value Value, or its default when unset; says whether its value changed. */
	bool Store(const FVeyraContentId& Id, const TOptional<FString>& Value);

	const FVeyraSettingsRegistry* Registry = nullptr;

	/** Only the values that differ from their defaults. */
	TMap<FVeyraContentId, FString> Values;

	struct FLastChange
	{
		FVeyraContentId Id;
		TOptional<FString> Previous;
	};
	TOptional<FLastChange> LastChange;
};
