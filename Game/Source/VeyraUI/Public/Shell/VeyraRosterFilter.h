// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Internationalization/Text.h"

/** The tabs that narrow a roster of Vanguards (Pre-Game Client UX Bible 19, 29 and 30; ADR-058 §2). */
enum class EVeyraRosterTab : uint8
{
	All,
	Owned,
	/** Lent by this week's rotation and not owned: an owned Vanguard is Owned, whatever the rotation. */
	FreeRotation,
	/** Champion select's only. */
	Favorites,
};

/** What a roster filter reads of one Vanguard. */
struct FVeyraRosterEntry
{
	FText Name;
	bool bOwned = false;
	bool bRotation = false;
	bool bFavorite = false;
};

/** One filter for the Collection and champion select: it only narrows what shows, never ownership, eligibility, bans or picks. */
namespace VeyraRosterFilter
{
	/** Whether Entry shows under Tab and Search: its name contains Search, ignoring case and the space around it, and it is what Tab asks for. */
	VEYRAUI_API bool Shows(const FVeyraRosterEntry& Entry, EVeyraRosterTab Tab, const FString& Search);

	/** A tab's name, as its button shows it. */
	VEYRAUI_API FText TabName(EVeyraRosterTab Tab);
}
