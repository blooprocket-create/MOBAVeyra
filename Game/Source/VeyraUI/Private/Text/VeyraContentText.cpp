// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Text/VeyraContentText.h"

#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Misc/Paths.h"
#include "Tuning/VeyraItemsTuning.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraSettingsRegistry.h"

namespace VeyraContentText
{
namespace
{
	// The key's parts: vanguard.<id>.name and so on.
	const TCHAR* const VanguardKind = TEXT("vanguard");
	const TCHAR* const AbilityKind = TEXT("ability");
	const TCHAR* const PassiveKind = TEXT("passive");
	const TCHAR* const ItemKind = TEXT("item");
	const TCHAR* const ModeKind = TEXT("mode");
	const TCHAR* const NameField = TEXT("name");
	const TCHAR* const TitleField = TEXT("title");
	const TCHAR* const DescriptionField = TEXT("description");
	const TCHAR* const SettingKind = TEXT("setting");
	const TCHAR* const TermsField = TEXT("terms");

	/** setting.<id>.option.<option> */
	FString OptionField(const FString& Option)
	{
		return TEXT("option.") + Option;
	}

	FString KeyOf(const TCHAR* Kind, const FVeyraContentId& Id, const TCHAR* Field)
	{
		return FString::Printf(TEXT("%s.%s.%s"), Kind, *Id.ToString(), Field);
	}

	bool HasKey(const FString& Key)
	{
		const FStringTableConstPtr Table = FStringTableRegistry::Get().FindStringTable(TableName);
		FString Source;
		return Table.IsValid() && Table->GetSourceString(FTextKey(Key), Source);
	}

	/** Every row whose key starts with Prefix, in key order. */
	TArray<FText> RowsStartingWith(const TCHAR* Prefix)
	{
		TArray<FString> Keys;
		if (const FStringTableConstPtr Table = FStringTableRegistry::Get().FindStringTable(TableName))
		{
			Table->EnumerateSourceStrings([&Keys, Prefix](const FString& Key, const FString& /*Source*/) {
				if (Key.StartsWith(Prefix))
				{
					Keys.Add(Key);
				}
				return true;
			});
		}
		Keys.Sort();
		TArray<FText> Rows;
		for (const FString& Key : Keys)
		{
			Rows.Add(FText::FromStringTable(TableName, FTextKey(Key)));
		}
		return Rows;
	}

	/** The table's text for the key, or Fallback when the table has none. */
	FText TextOr(const TCHAR* Kind, const FVeyraContentId& Id, const TCHAR* Field, const FString& Fallback)
	{
		const FString Key = KeyOf(Kind, Id, Field);
		return HasKey(Key) ? FText::FromStringTable(TableName, FTextKey(Key)) : FText::FromString(Fallback);
	}
}

FString TablePath()
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Text"), TEXT("VeyraText.csv"));
}

void Register()
{
	// What LOCTABLE_FROMFILE_GAME does, from the project folder rather than Content/, where the
	// editor would offer to import the file as an asset. Game/Tuning is read the same way.
	FStringTableRegistry::Get().Internal_LocTableFromFile(TableName, TableName, TEXT("Text/VeyraText.csv"), FPaths::ProjectDir());
}

FText VanguardName(const FVeyraContentId& Vanguard)
{
	return TextOr(VanguardKind, Vanguard, NameField, Vanguard.ToString());
}

TArray<FText> LoadingTips()
{
	return RowsStartingWith(TEXT("loading.tip."));
}

TArray<FText> LoadingLore()
{
	return RowsStartingWith(TEXT("loading.lore."));
}

FText ModeName(const FVeyraContentId& Mode, const FString& Fallback)
{
	return TextOr(ModeKind, Mode, NameField, Fallback);
}

FText VanguardTitle(const FVeyraContentId& Vanguard)
{
	return TextOr(VanguardKind, Vanguard, TitleField, FString());
}

FText AbilityName(const FVeyraContentId& Ability)
{
	return TextOr(AbilityKind, Ability, NameField, Ability.ToString());
}

FText AbilityDescription(const FVeyraContentId& Ability)
{
	return TextOr(AbilityKind, Ability, DescriptionField, FString());
}

FText PassiveName(const FVeyraContentId& Passive)
{
	return TextOr(PassiveKind, Passive, NameField, Passive.ToString());
}

FText PassiveDescription(const FVeyraContentId& Passive)
{
	return TextOr(PassiveKind, Passive, DescriptionField, FString());
}

FText ItemName(const FVeyraContentId& Item)
{
	return TextOr(ItemKind, Item, NameField, Item.ToString());
}

FText ItemDescription(const FVeyraContentId& Item)
{
	return TextOr(ItemKind, Item, DescriptionField, FString());
}

TArray<FString> FindMissingItemText()
{
	TArray<FString> Missing;
	for (const TPair<FVeyraContentId, FVeyraItemDefinition>& Pair : UVeyraItemsTuningSubsystem::Get().Items)
	{
		const FVeyraItemDefinition& Item = Pair.Value;
		TArray<const TCHAR*> Fields = { NameField };
		if (!Item.Active.IsEmpty() || !Item.Attunement.IsEmpty() || Item.Category == EVeyraItemCategory::Consumable)
		{
			Fields.Add(DescriptionField);
		}
		for (const TCHAR* Field : Fields)
		{
			const FString Key = KeyOf(ItemKind, Pair.Key, Field);
			if (!HasKey(Key))
			{
				Missing.Add(Key);
			}
		}
	}
	return Missing;
}

FText SettingName(const FVeyraContentId& Setting)
{
	return TextOr(SettingKind, Setting, NameField, Setting.ToString());
}

FText SettingDescription(const FVeyraContentId& Setting)
{
	return TextOr(SettingKind, Setting, DescriptionField, FString());
}

FText SettingTerms(const FVeyraContentId& Setting)
{
	return TextOr(SettingKind, Setting, TermsField, FString());
}

FText SettingOption(const FVeyraContentId& Setting, const FString& Option)
{
	return TextOr(SettingKind, Setting, *OptionField(Option), Option);
}

TArray<FString> FindMissingSettingText(const FVeyraSettingsRegistry& Registry)
{
	TArray<FString> Missing;
	for (const FVeyraSettingInfo& Setting : VeyraSettings::All(Registry))
	{
		TArray<FString> Fields = { NameField, DescriptionField, TermsField };
		if (Setting.Choice)
		{
			for (const FString& Option : Setting.Choice->Options)
			{
				Fields.Add(OptionField(Option));
			}
		}
		for (const FString& Field : Fields)
		{
			const FString Key = KeyOf(SettingKind, Setting.Id, *Field);
			if (!HasKey(Key))
			{
				Missing.Add(Key);
			}
		}
	}
	return Missing;
}

TArray<FString> FindMissingPlayableText()
{
	TArray<FString> Missing;
	const auto Require = [&Missing](const TCHAR* Kind, const FVeyraContentId& Id, const TCHAR* Field) {
		const FString Key = KeyOf(Kind, Id, Field);
		if (!HasKey(Key))
		{
			Missing.Add(Key);
		}
	};
	for (const TPair<FVeyraContentId, FVeyraVanguardDefinition>& Pair : UVeyraVanguardsTuningSubsystem::Get().Vanguards)
	{
		const FVeyraVanguardDefinition& Definition = Pair.Value;
		if (Definition.Availability != EVeyraVanguardAvailability::Playable)
		{
			continue;
		}
		Require(VanguardKind, Pair.Key, NameField);
		Require(VanguardKind, Pair.Key, TitleField);
		for (const TArray<FVeyraContentId>* Slot : { &Definition.Abilities.Q, &Definition.Abilities.W, &Definition.Abilities.E, &Definition.Abilities.R })
		{
			for (const FVeyraContentId& Ability : *Slot)
			{
				Require(AbilityKind, Ability, NameField);
				Require(AbilityKind, Ability, DescriptionField);
			}
		}
		for (const FVeyraContentId& Passive : Definition.Passive)
		{
			Require(PassiveKind, Passive, NameField);
			Require(PassiveKind, Passive, DescriptionField);
		}
	}
	return Missing;
}
}
