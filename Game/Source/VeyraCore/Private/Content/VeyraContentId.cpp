// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Content/VeyraContentId.h"

namespace
{
	bool IsLowercaseLetter(TCHAR Character)
	{
		return Character >= TEXT('a') && Character <= TEXT('z');
	}

	bool IsDigit(TCHAR Character)
	{
		return Character >= TEXT('0') && Character <= TEXT('9');
	}
}

bool FVeyraContentId::IsValidText(FStringView Text)
{
	// The same language as Pattern, checked by hand so no regular-expression engine is involved.
	if (Text.IsEmpty() || !IsLowercaseLetter(Text[0]))
	{
		return false;
	}
	bool bAfterUnderscore = false;
	for (const TCHAR Character : Text.RightChop(1))
	{
		if (Character == TEXT('_'))
		{
			if (bAfterUnderscore)
			{
				return false;
			}
			bAfterUnderscore = true;
		}
		else if (IsLowercaseLetter(Character) || IsDigit(Character))
		{
			bAfterUnderscore = false;
		}
		else
		{
			return false;
		}
	}
	return !bAfterUnderscore;
}

TOptional<FVeyraContentId> FVeyraContentId::FromText(FStringView Text)
{
	if (!IsValidText(Text))
	{
		return {};
	}
	FVeyraContentId Id;
	Id.Name = FName(Text);
	return Id;
}
