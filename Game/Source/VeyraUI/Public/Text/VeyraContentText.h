// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Content/VeyraContentId.h"
#include "Internationalization/Text.h"

struct FVeyraSettingsRegistry;

/**
 * What players read about Vanguards, their abilities and passives: names, titles and one-line
 * descriptions (ADR-010 §4). They live in a string table read from Game/Text/VeyraText.csv, so the
 * text is reviewable, localisable data and no screen shows a content ID. Its keys are
 * vanguard.<id>.name and .title, ability.<id>.name and .description, passive.<id>.name and
 * .description, item.<id>.name and .description, and setting.<id>.name, .description, .terms and
 * .option.<option>. Developer content without text shows its content ID.
 */
namespace VeyraContentText
{
	/** The string table's ID and namespace. */
	inline constexpr const TCHAR* TableName = TEXT("VeyraText");

	/** Where the table is, in the project folder or the packaged build (staged by VeyraUI.Build.cs). */
	VEYRAUI_API FString TablePath();

	/** Registers the table. The module does it at startup. */
	void Register();

	VEYRAUI_API FText VanguardName(const FVeyraContentId& Vanguard);
	VEYRAUI_API FText VanguardTitle(const FVeyraContentId& Vanguard);
	VEYRAUI_API FText AbilityName(const FVeyraContentId& Ability);
	VEYRAUI_API FText AbilityDescription(const FVeyraContentId& Ability);
	VEYRAUI_API FText PassiveName(const FVeyraContentId& Passive);
	VEYRAUI_API FText PassiveDescription(const FVeyraContentId& Passive);

	/** A mode's name, as mode.<id>.name gives it, or Fallback when the table has none. */
	VEYRAUI_API FText ModeName(const FVeyraContentId& Mode, const FString& Fallback);

	/** An item's name, and what its Active, Attunement or use does; empty for a plain item. */
	VEYRAUI_API FText ItemName(const FVeyraContentId& Item);
	VEYRAUI_API FText ItemDescription(const FVeyraContentId& Item);

	/**
	 * The keys a Playable Vanguard needs that the table lacks: its name and title, and each of its
	 * abilities' and its passive's name and description. Empty when every released Vanguard has its text.
	 */
	VEYRAUI_API TArray<FString> FindMissingPlayableText();

	/**
	 * The keys the shop's items need that the table lacks: every item's name, and a description for
	 * each whose Active, Attunement or use the shop must explain. Empty when the catalog has its text.
	 */
	VEYRAUI_API TArray<FString> FindMissingItemText();

	/**
	 * The loading screen's gameplay tips (loading.tip.<n>) and lore facts (loading.lore.<n>), each in key order (SET-116;
	 * ADR-053 §3).
	 */
	VEYRAUI_API TArray<FText> LoadingTips();
	VEYRAUI_API TArray<FText> LoadingLore();

	/** A setting's name and its plain-language description (Settings Bible §6.3). */
	VEYRAUI_API FText SettingName(const FVeyraContentId& Setting);
	VEYRAUI_API FText SettingDescription(const FVeyraContentId& Setting);

	/** Other words players may search for it by, comma-separated (SET-102); empty for none. */
	VEYRAUI_API FText SettingTerms(const FVeyraContentId& Setting);

	/** A choice's option as players read it; the option itself where the table has none. */
	VEYRAUI_API FText SettingOption(const FVeyraContentId& Setting, const FString& Option);

	/**
	 * The keys Registry's settings need that the table lacks: each setting's name, description and
	 * search words, and each choice's options. Empty when every setting has its text.
	 */
	VEYRAUI_API TArray<FString> FindMissingSettingText(const FVeyraSettingsRegistry& Registry);
}
