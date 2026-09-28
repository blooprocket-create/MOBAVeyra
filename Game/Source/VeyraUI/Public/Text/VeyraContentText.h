// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Content/VeyraContentId.h"
#include "Internationalization/Text.h"

/**
 * What players read about Vanguards, their abilities and passives: names, titles and one-line
 * descriptions (ADR-010 §4). They live in a string table read from Game/Text/VeyraText.csv, so the
 * text is reviewable, localisable data and no screen shows a content ID. Its keys are
 * vanguard.<id>.name and .title, ability.<id>.name and .description, and passive.<id>.name and
 * .description. Developer content without text shows its content ID.
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

	/**
	 * The keys a Playable Vanguard needs that the table lacks: its name and title, and each of its
	 * abilities' and its passive's name and description. Empty when every released Vanguard has its text.
	 */
	VEYRAUI_API TArray<FString> FindMissingPlayableText();
}
