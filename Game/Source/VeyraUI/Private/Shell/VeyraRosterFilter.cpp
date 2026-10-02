// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraRosterFilter.h"

#define LOCTEXT_NAMESPACE "VeyraRosterFilter"

namespace VeyraRosterFilter
{
bool Shows(const FVeyraRosterEntry& Entry, EVeyraRosterTab Tab, const FString& Search)
{
	const FString Wanted = Search.TrimStartAndEnd();
	if (!Wanted.IsEmpty() && !Entry.Name.ToString().Contains(Wanted, ESearchCase::IgnoreCase))
	{
		return false;
	}
	switch (Tab)
	{
	case EVeyraRosterTab::Owned:
		return Entry.bOwned;
	case EVeyraRosterTab::FreeRotation:
		return Entry.bRotation && !Entry.bOwned;
	case EVeyraRosterTab::Favorites:
		return Entry.bFavorite;
	case EVeyraRosterTab::All:
		break;
	}
	return true;
}

FText TabName(EVeyraRosterTab Tab)
{
	switch (Tab)
	{
	case EVeyraRosterTab::Owned:
		return LOCTEXT("Owned", "Owned");
	case EVeyraRosterTab::FreeRotation:
		return LOCTEXT("FreeRotation", "Free Rotation");
	case EVeyraRosterTab::Favorites:
		return LOCTEXT("Favorites", "Favorites");
	case EVeyraRosterTab::All:
		break;
	}
	return LOCTEXT("All", "All");
}
}

#undef LOCTEXT_NAMESPACE
