// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraSettingsRegistry.h"

namespace VeyraSettings
{
namespace
{
	// How near a number must lie to a step, or to a bound, to count as on it: rounding, not tuning.
	constexpr double StepTolerance = 1e-6;

	template <typename SettingType>
	FVeyraSettingInfo Describe(const FVeyraContentId& Id, EVeyraSettingKind Kind, const SettingType& Setting)
	{
		FVeyraSettingInfo Info;
		Info.Id = Id;
		Info.Kind = Kind;
		Info.Category = Setting.Category;
		Info.Scope = Setting.Scope;
		Info.Availability = Setting.Availability;
		Info.Applies = Setting.Applies;
		return Info;
	}

	FVeyraSettingInfo DescribeToggle(const FVeyraContentId& Id, const FVeyraToggleSetting& Setting)
	{
		FVeyraSettingInfo Info = Describe(Id, EVeyraSettingKind::Toggle, Setting);
		Info.Default = Setting.Default == EVeyraToggleValue::On ? On() : Off();
		return Info;
	}

	FVeyraSettingInfo DescribeRange(const FVeyraContentId& Id, const FVeyraRangeSetting& Setting)
	{
		FVeyraSettingInfo Info = Describe(Id, EVeyraSettingKind::Range, Setting);
		Info.Default = NumberText(Setting.Default);
		Info.Range = &Setting;
		return Info;
	}

	FVeyraSettingInfo DescribeChoice(const FVeyraContentId& Id, const FVeyraChoiceSetting& Setting)
	{
		FVeyraSettingInfo Info = Describe(Id, EVeyraSettingKind::Choice, Setting);
		Info.Default = Setting.Default;
		Info.Choice = &Setting;
		return Info;
	}

	bool IsOnStep(double Value, const FVeyraRangeSetting& Range)
	{
		const double Steps = (Value - Range.Minimum) / Range.Step;
		return FMath::Abs(Steps - FMath::RoundToDouble(Steps)) <= StepTolerance;
	}

	template <typename MapType>
	TArray<FVeyraContentId> SortedIds(const MapType& Map)
	{
		TArray<FVeyraContentId> Ids;
		Map.GetKeys(Ids);
		Ids.Sort([](const FVeyraContentId& A, const FVeyraContentId& B) { return A.ToString() < B.ToString(); });
		return Ids;
	}
}

const FString& On()
{
	static const FString Text(TEXT("On"));
	return Text;
}

const FString& Off()
{
	static const FString Text(TEXT("Off"));
	return Text;
}

TArray<FString> Validate(const FVeyraSettingsRegistry& Registry)
{
	TArray<FString> Problems;
	TSet<FVeyraContentId> Seen;
	const auto Once = [&Problems, &Seen](const TCHAR* Map, const FVeyraContentId& Id) {
		bool bAlready = false;
		Seen.Add(Id, &bAlready);
		if (bAlready)
		{
			Problems.Add(FString::Printf(TEXT("/%s/%s: another kind of setting uses this ID"), Map, *Id.ToString()));
		}
	};
	for (const TPair<FVeyraContentId, FVeyraToggleSetting>& Entry : Registry.Toggles)
	{
		Once(TEXT("toggles"), Entry.Key);
	}
	for (const TPair<FVeyraContentId, FVeyraRangeSetting>& Entry : Registry.Ranges)
	{
		Once(TEXT("ranges"), Entry.Key);
		const FString Pointer = FString::Printf(TEXT("/ranges/%s"), *Entry.Key.ToString());
		const FVeyraRangeSetting& Range = Entry.Value;
		if (!(Range.Maximum > Range.Minimum))
		{
			Problems.Add(Pointer + TEXT("/maximum: must be above the minimum"));
		}
		if (Range.Default < Range.Minimum - StepTolerance || Range.Default > Range.Maximum + StepTolerance)
		{
			Problems.Add(Pointer + TEXT("/default: must lie within the minimum and maximum"));
		}
		else if (Range.Step > 0.0 && !IsOnStep(Range.Default, Range))
		{
			Problems.Add(Pointer + TEXT("/default: must lie on a step from the minimum"));
		}
	}
	for (const TPair<FVeyraContentId, FVeyraChoiceSetting>& Entry : Registry.Choices)
	{
		Once(TEXT("choices"), Entry.Key);
		const FString Pointer = FString::Printf(TEXT("/choices/%s"), *Entry.Key.ToString());
		const FVeyraChoiceSetting& Choice = Entry.Value;
		TSet<FString> Options;
		for (const FString& Option : Choice.Options)
		{
			bool bAlready = false;
			Options.Add(Option, &bAlready);
			if (bAlready)
			{
				Problems.Add(FString::Printf(TEXT("%s/options: \"%s\" is offered twice"), *Pointer, *Option));
			}
		}
		if (!Choice.Options.Contains(Choice.Default))
		{
			Problems.Add(Pointer + TEXT("/default: must be one of the options"));
		}
	}
	TSet<EVeyraSettingCategory> Categories;
	TSet<FVeyraContentId> Listed;
	for (int32 Index = 0; Index < Registry.Layout.Num(); ++Index)
	{
		const FVeyraSettingsSection& Section = Registry.Layout[Index];
		const FString Pointer = FString::Printf(TEXT("/layout/%d"), Index);
		bool bCategoryAlready = false;
		Categories.Add(Section.Category, &bCategoryAlready);
		if (bCategoryAlready)
		{
			Problems.Add(Pointer + TEXT("/category: another section shows this category"));
		}
		for (int32 Entry = 0; Entry < Section.Settings.Num(); ++Entry)
		{
			const FVeyraContentId& Id = Section.Settings[Entry];
			const FString EntryPointer = FString::Printf(TEXT("%s/settings/%d"), *Pointer, Entry);
			const TOptional<FVeyraSettingInfo> Setting = Find(Registry, Id);
			if (!Setting.IsSet())
			{
				Problems.Add(FString::Printf(TEXT("%s: no setting is %s"), *EntryPointer, *Id.ToString()));
				continue;
			}
			if (Setting->Category != Section.Category)
			{
				Problems.Add(FString::Printf(TEXT("%s: %s belongs to another category"), *EntryPointer, *Id.ToString()));
			}
			bool bListedAlready = false;
			Listed.Add(Id, &bListedAlready);
			if (bListedAlready)
			{
				Problems.Add(FString::Printf(TEXT("%s: %s is listed twice"), *EntryPointer, *Id.ToString()));
			}
		}
	}
	for (const FVeyraSettingInfo& Setting : All(Registry))
	{
		if (!Listed.Contains(Setting.Id))
		{
			Problems.Add(FString::Printf(TEXT("/layout: %s is in no section, so the screen would never show it"), *Setting.Id.ToString()));
		}
	}
	return Problems;
}

TOptional<FVeyraSettingInfo> Find(const FVeyraSettingsRegistry& Registry, const FVeyraContentId& Id)
{
	if (const FVeyraToggleSetting* Toggle = Registry.Toggles.Find(Id))
	{
		return DescribeToggle(Id, *Toggle);
	}
	if (const FVeyraRangeSetting* Range = Registry.Ranges.Find(Id))
	{
		return DescribeRange(Id, *Range);
	}
	if (const FVeyraChoiceSetting* Choice = Registry.Choices.Find(Id))
	{
		return DescribeChoice(Id, *Choice);
	}
	return {};
}

TArray<FVeyraSettingInfo> All(const FVeyraSettingsRegistry& Registry)
{
	TArray<FVeyraSettingInfo> Settings;
	for (const FVeyraContentId& Id : SortedIds(Registry.Toggles))
	{
		Settings.Add(DescribeToggle(Id, Registry.Toggles[Id]));
	}
	for (const FVeyraContentId& Id : SortedIds(Registry.Ranges))
	{
		Settings.Add(DescribeRange(Id, Registry.Ranges[Id]));
	}
	for (const FVeyraContentId& Id : SortedIds(Registry.Choices))
	{
		Settings.Add(DescribeChoice(Id, Registry.Choices[Id]));
	}
	return Settings;
}

bool IsChangeable(const FVeyraSettingInfo& Setting, bool bInLiveMatch)
{
	return !bInLiveMatch || Setting.Availability == EVeyraSettingAvailability::Anywhere;
}

TArray<FVeyraSettingsSectionInfo> Sections(const FVeyraSettingsRegistry& Registry)
{
	TArray<FVeyraSettingsSectionInfo> Sections;
	for (const FVeyraSettingsSection& Section : Registry.Layout)
	{
		FVeyraSettingsSectionInfo& Described = Sections.AddDefaulted_GetRef();
		Described.Category = Section.Category;
		for (const FVeyraContentId& Id : Section.Settings)
		{
			if (const TOptional<FVeyraSettingInfo> Setting = Find(Registry, Id))
			{
				Described.Settings.Add(*Setting);
			}
		}
	}
	return Sections;
}

TOptional<FString> Normalize(const FVeyraSettingInfo& Setting, FStringView Value)
{
	const FString Text(Value);
	switch (Setting.Kind)
	{
	case EVeyraSettingKind::Toggle:
		if (Text.Equals(On(), ESearchCase::IgnoreCase))
		{
			return On();
		}
		if (Text.Equals(Off(), ESearchCase::IgnoreCase))
		{
			return Off();
		}
		return {};
	case EVeyraSettingKind::Range:
	{
		double Number = 0.0;
		const FVeyraRangeSetting* Range = Setting.Range;
		if (!Range || !LexTryParseString(Number, *Text) || !FMath::IsFinite(Number) || Number < Range->Minimum - StepTolerance
			|| Number > Range->Maximum + StepTolerance)
		{
			return {};
		}
		const double Snapped = Range->Minimum + FMath::RoundToDouble((Number - Range->Minimum) / Range->Step) * Range->Step;
		return NumberText(FMath::Clamp(Snapped, Range->Minimum, Range->Maximum));
	}
	case EVeyraSettingKind::Choice:
		if (Setting.Choice)
		{
			for (const FString& Option : Setting.Choice->Options)
			{
				if (Option.Equals(Text, ESearchCase::IgnoreCase))
				{
					return Option;
				}
			}
		}
		return {};
	}
	return {};
}

FString NumberText(double Number)
{
	const double Whole = FMath::RoundToDouble(Number);
	if (FMath::Abs(Number - Whole) <= StepTolerance)
	{
		return FString::Printf(TEXT("%lld"), static_cast<long long>(Whole));
	}
	return FString::SanitizeFloat(Number);
}
}
