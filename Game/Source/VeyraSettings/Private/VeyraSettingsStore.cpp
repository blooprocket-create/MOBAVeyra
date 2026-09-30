// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraSettingsStore.h"

FVeyraSettingsStore::FVeyraSettingsStore(const FVeyraSettingsRegistry& InRegistry)
	: Registry(&InRegistry)
{
}

FString FVeyraSettingsStore::Get(const FVeyraContentId& Id) const
{
	if (const FString* Value = Values.Find(Id))
	{
		return *Value;
	}
	const TOptional<FVeyraSettingInfo> Setting = VeyraSettings::Find(*Registry, Id);
	return Setting.IsSet() ? Setting->Default : FString();
}

bool FVeyraSettingsStore::IsOn(const FVeyraContentId& Id) const
{
	return Get(Id) == VeyraSettings::On();
}

double FVeyraSettingsStore::GetNumber(const FVeyraContentId& Id) const
{
	double Number = 0.0;
	return Registry->Ranges.Contains(Id) && LexTryParseString(Number, *Get(Id)) ? Number : 0.0;
}

EVeyraSettingChange FVeyraSettingsStore::Set(const FVeyraContentId& Id, FStringView Value, bool bInLiveMatch)
{
	const TOptional<FVeyraSettingInfo> Setting = VeyraSettings::Find(*Registry, Id);
	if (!Setting.IsSet())
	{
		return EVeyraSettingChange::UnknownSetting;
	}
	if (!VeyraSettings::IsChangeable(*Setting, bInLiveMatch))
	{
		return EVeyraSettingChange::NotInMatch;
	}
	const TOptional<FString> Normal = VeyraSettings::Normalize(*Setting, Value);
	if (!Normal.IsSet())
	{
		return EVeyraSettingChange::InvalidValue;
	}
	const TOptional<FString> Previous = Values.Contains(Id) ? TOptional<FString>(Values[Id]) : TOptional<FString>();
	const TOptional<FString> Next = *Normal == Setting->Default ? TOptional<FString>() : Normal;
	if (!Store(Id, Next))
	{
		return EVeyraSettingChange::Unchanged;
	}
	LastChange = FLastChange{ Id, Previous };
	OnChanged.Broadcast(Id);
	return EVeyraSettingChange::Changed;
}

EVeyraSettingChange FVeyraSettingsStore::Follow(const FVeyraContentId& Id, FStringView Value)
{
	const TOptional<FVeyraSettingInfo> Setting = VeyraSettings::Find(*Registry, Id);
	if (!Setting.IsSet())
	{
		return EVeyraSettingChange::UnknownSetting;
	}
	const TOptional<FString> Normal = VeyraSettings::Normalize(*Setting, Value);
	if (!Normal.IsSet())
	{
		return EVeyraSettingChange::InvalidValue;
	}
	if (!Store(Id, *Normal == Setting->Default ? TOptional<FString>() : Normal))
	{
		return EVeyraSettingChange::Unchanged;
	}
	OnChanged.Broadcast(Id);
	return EVeyraSettingChange::Changed;
}

EVeyraSettingChange FVeyraSettingsStore::Reset(const FVeyraContentId& Id, bool bInLiveMatch)
{
	const TOptional<FVeyraSettingInfo> Setting = VeyraSettings::Find(*Registry, Id);
	return Setting.IsSet() ? Set(Id, Setting->Default, bInLiveMatch) : EVeyraSettingChange::UnknownSetting;
}

void FVeyraSettingsStore::ResetCategory(EVeyraSettingCategory Category, bool bInLiveMatch)
{
	LastChange.Reset();
	for (const FVeyraSettingInfo& Setting : VeyraSettings::All(*Registry))
	{
		if (Setting.Category == Category && VeyraSettings::IsChangeable(Setting, bInLiveMatch) && Store(Setting.Id, {}))
		{
			OnChanged.Broadcast(Setting.Id);
		}
	}
}

void FVeyraSettingsStore::ResetAll(bool bInLiveMatch)
{
	LastChange.Reset();
	for (const FVeyraSettingInfo& Setting : VeyraSettings::All(*Registry))
	{
		if (VeyraSettings::IsChangeable(Setting, bInLiveMatch) && Store(Setting.Id, {}))
		{
			OnChanged.Broadcast(Setting.Id);
		}
	}
}

bool FVeyraSettingsStore::Undo()
{
	if (!LastChange.IsSet())
	{
		return false;
	}
	const FLastChange Change = LastChange.GetValue();
	LastChange.Reset();
	if (Store(Change.Id, Change.Previous))
	{
		OnChanged.Broadcast(Change.Id);
	}
	return true;
}

TMap<FString, FString> FVeyraSettingsStore::SaveScope(EVeyraSettingScope Scope) const
{
	TMap<FString, FString> Saved;
	for (const TPair<FVeyraContentId, FString>& Entry : Values)
	{
		const TOptional<FVeyraSettingInfo> Setting = VeyraSettings::Find(*Registry, Entry.Key);
		if (Setting.IsSet() && Setting->Scope == Scope)
		{
			Saved.Add(Entry.Key.ToString(), Entry.Value);
		}
	}
	return Saved;
}

int32 FVeyraSettingsStore::LoadScope(EVeyraSettingScope Scope, const TMap<FString, FString>& Saved)
{
	LastChange.Reset();
	TMap<FVeyraContentId, FString> Loaded;
	int32 Dropped = 0;
	for (const TPair<FString, FString>& Entry : Saved)
	{
		const TOptional<FVeyraContentId> Id = FVeyraContentId::FromText(Entry.Key);
		const TOptional<FVeyraSettingInfo> Setting = Id.IsSet() ? VeyraSettings::Find(*Registry, *Id) : TOptional<FVeyraSettingInfo>();
		const TOptional<FString> Normal = Setting.IsSet() && Setting->Scope == Scope ? VeyraSettings::Normalize(*Setting, Entry.Value) : TOptional<FString>();
		if (!Normal.IsSet())
		{
			++Dropped;
			continue;
		}
		if (*Normal != Setting->Default)
		{
			Loaded.Add(*Id, *Normal);
		}
	}
	for (const FVeyraSettingInfo& Setting : VeyraSettings::All(*Registry))
	{
		if (Setting.Scope == Scope && Store(Setting.Id, Loaded.Contains(Setting.Id) ? TOptional<FString>(Loaded[Setting.Id]) : TOptional<FString>()))
		{
			OnChanged.Broadcast(Setting.Id);
		}
	}
	return Dropped;
}

bool FVeyraSettingsStore::Store(const FVeyraContentId& Id, const TOptional<FString>& Value)
{
	const FString* Current = Values.Find(Id);
	if (Value.IsSet() ? Current && *Current == *Value : !Current)
	{
		return false;
	}
	if (Value.IsSet())
	{
		Values.Add(Id, *Value);
	}
	else
	{
		Values.Remove(Id);
	}
	return true;
}
