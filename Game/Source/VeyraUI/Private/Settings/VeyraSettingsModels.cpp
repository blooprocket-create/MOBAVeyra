// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Settings/VeyraSettingsModels.h"

#include "Input/VeyraInputSettings.h"
#include "Shell/VeyraUIInputSettings.h"
#include "Text/VeyraContentText.h"
#include "VeyraSettingsStore.h"

#define LOCTEXT_NAMESPACE "VeyraSettingsScreen"

namespace VeyraSettingsModels
{
namespace
{
	/** A range's value one step from Number, Direction -1 or +1; unset past its bounds. */
	TOptional<FString> Stepped(const FVeyraSettingInfo& Setting, double Number, double Direction)
	{
		const FVeyraRangeSetting& Range = *Setting.Range;
		const double Next = Number + Direction * Range.Step;
		// Half a step of slack, so a value between steps still reaches its bound.
		if (Next < Range.Minimum - Range.Step * 0.5 || Next > Range.Maximum + Range.Step * 0.5)
		{
			return {};
		}
		return VeyraSettings::Normalize(Setting, VeyraSettings::NumberText(FMath::Clamp(Next, Range.Minimum, Range.Maximum)));
	}

	FVeyraSettingRowModel DescribeRow(const FVeyraSettingsStore& Store, const FVeyraSettingInfo& Setting, bool bInLiveMatch)
	{
		FVeyraSettingRowModel Row;
		Row.Id = Setting.Id;
		Row.Kind = Setting.Kind;
		Row.Category = Setting.Category;
		Row.Name = VeyraContentText::SettingName(Setting.Id);
		Row.Description = VeyraContentText::SettingDescription(Setting.Id);
		Row.bChanged = Store.IsChanged(Setting.Id);
		Row.bAfterRestart = Setting.Applies == EVeyraSettingApplies::AfterRestart;
		Row.bLocked = !VeyraSettings::IsChangeable(Setting, bInLiveMatch);
		const FString Value = Store.Get(Setting.Id);
		switch (Setting.Kind)
		{
		case EVeyraSettingKind::Toggle:
			Row.Options.Add({ VeyraSettings::On(), LOCTEXT("On", "On"), Value == VeyraSettings::On() });
			Row.Options.Add({ VeyraSettings::Off(), LOCTEXT("Off", "Off"), Value == VeyraSettings::Off() });
			break;
		case EVeyraSettingKind::Choice:
			for (const FString& Option : Setting.Choice->Options)
			{
				Row.Options.Add({ Option, VeyraContentText::SettingOption(Setting.Id, Option), Value == Option });
			}
			break;
		case EVeyraSettingKind::Binding:
			Row.Key = BindingKey(Store, Setting.Id);
			Row.bEssential = Setting.Binding && Setting.Binding->Need == EVeyraBindingNeed::Essential;
			Row.ValueText = Row.Key.IsValid() ? Row.Key.GetDisplayName() : LOCTEXT("NoKey", "No key");
			break;
		case EVeyraSettingKind::Range:
		{
			const double Number = Store.GetNumber(Setting.Id);
			FNumberFormattingOptions Format;
			Format.SetUseGrouping(false).SetMaximumFractionalDigits(2);
			Row.ValueText = FText::AsNumber(Number, &Format);
			Row.Lower = Stepped(Setting, Number, -1.0);
			Row.Higher = Stepped(Setting, Number, 1.0);
			break;
		}
		}
		if (const FVeyraSettingOptionModel* Selected = Row.Options.FindByPredicate([](const FVeyraSettingOptionModel& Option) { return Option.bSelected; }))
		{
			Row.ValueText = Selected->Label;
		}
		return Row;
	}
}

const UObject* BindingDefaults(const FVeyraBindingSetting& Binding)
{
	for (const UObject* Defaults : { static_cast<const UObject*>(GetDefault<UVeyraInputSettings>()), static_cast<const UObject*>(GetDefault<UVeyraUIInputSettings>()) })
	{
		if (VeyraSettings::FindKeyProperty(*Defaults->GetClass(), Binding.Property))
		{
			return Defaults;
		}
	}
	return nullptr;
}

FKey BindingKey(const FVeyraSettingsStore& Store, const FVeyraContentId& Id)
{
	const FVeyraBindingSetting* Binding = Store.GetRegistry().Bindings.Find(Id);
	const UObject* Defaults = Binding ? BindingDefaults(*Binding) : nullptr;
	return Defaults ? VeyraSettings::BindingKey(*Defaults, Store, Id).Get(EKeys::Invalid) : EKeys::Invalid;
}

TOptional<FVeyraContentId> FindConflict(const FVeyraSettingsStore& Store, const FVeyraContentId& Id, const FKey& Key)
{
	const FVeyraBindingSetting* Binding = Store.GetRegistry().Bindings.Find(Id);
	if (!Binding || Binding->Sharing == EVeyraBindingSharing::Shared || !Key.IsValid())
	{
		return {};
	}
	for (const TPair<FVeyraContentId, FVeyraBindingSetting>& Other : Store.GetRegistry().Bindings)
	{
		if (Other.Key != Id && Other.Value.Sharing == EVeyraBindingSharing::Exclusive && BindingKey(Store, Other.Key) == Key)
		{
			return Other.Key;
		}
	}
	return {};
}

FText CategoryName(EVeyraSettingCategory Category)
{
	switch (Category)
	{
	case EVeyraSettingCategory::Controls:
		return LOCTEXT("Controls", "Controls");
	case EVeyraSettingCategory::Camera:
		return LOCTEXT("Camera", "Camera");
	case EVeyraSettingCategory::Interface:
		return LOCTEXT("Interface", "Interface");
	case EVeyraSettingCategory::Accessibility:
		return LOCTEXT("Accessibility", "Accessibility");
	case EVeyraSettingCategory::Audio:
		return LOCTEXT("Audio", "Audio");
	case EVeyraSettingCategory::GraphicsDisplay:
		return LOCTEXT("GraphicsDisplay", "Graphics & Display");
	case EVeyraSettingCategory::Communication:
		return LOCTEXT("Communication", "Communication");
	case EVeyraSettingCategory::LanguageAccount:
		return LOCTEXT("LanguageAccount", "Language & Account");
	}
	return FText::GetEmpty();
}

bool Matches(const FVeyraSettingInfo& Setting, const FString& Search)
{
	const FString Wanted = Search.TrimStartAndEnd();
	if (Wanted.IsEmpty())
	{
		return false;
	}
	if (VeyraContentText::SettingName(Setting.Id).ToString().Contains(Wanted))
	{
		return true;
	}
	TArray<FString> Terms;
	VeyraContentText::SettingTerms(Setting.Id).ToString().ParseIntoArray(Terms, TEXT(","));
	return Terms.ContainsByPredicate([&Wanted](const FString& Term) { return Term.TrimStartAndEnd().Contains(Wanted); });
}

FVeyraSettingsModel Describe(const FVeyraSettingsStore& Store, EVeyraSettingCategory Category, const FString& Search, bool bInLiveMatch)
{
	FVeyraSettingsModel Model;
	const TArray<FVeyraSettingsSectionInfo> Sections = VeyraSettings::Sections(Store.GetRegistry());
	for (const FVeyraSettingsSectionInfo& Section : Sections)
	{
		Model.Categories.Add(Section.Category);
	}
	Model.Category = Model.Categories.Contains(Category) || Model.Categories.IsEmpty() ? Category : Model.Categories[0];
	Model.bSearching = !Search.TrimStartAndEnd().IsEmpty();
	Model.bCanUndo = Store.CanUndo();
	for (const FVeyraSettingsSectionInfo& Section : Sections)
	{
		for (const FVeyraSettingInfo& Setting : Section.Settings)
		{
			const bool bChangeable = Store.IsChanged(Setting.Id) && VeyraSettings::IsChangeable(Setting, bInLiveMatch);
			Model.bAnyChanged |= bChangeable;
			Model.bCategoryChanged |= bChangeable && Section.Category == Model.Category;
			if (Model.bSearching ? Matches(Setting, Search) : Section.Category == Model.Category)
			{
				Model.Rows.Add(DescribeRow(Store, Setting, bInLiveMatch));
			}
			if (Setting.Binding && Setting.Binding->Need == EVeyraBindingNeed::Essential && !BindingKey(Store, Setting.Id).IsValid())
			{
				Model.UnboundEssentials.Add(VeyraContentText::SettingName(Setting.Id));
			}
		}
	}
	return Model;
}
}

#undef LOCTEXT_NAMESPACE
