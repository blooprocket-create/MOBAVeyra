// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraShellFonts.h"

#include "Fonts/CompositeFont.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"

namespace VeyraShellFonts
{
namespace
{
	/** The built families, by EVeyraFontFamily. */
	TMap<EVeyraFontFamily, TSharedRef<const FCompositeFont>>& Built()
	{
		static TMap<EVeyraFontFamily, TSharedRef<const FCompositeFont>> Families;
		return Families;
	}

	TSharedRef<const FCompositeFont> Build(EVeyraFontFamily Family)
	{
		const TArray<FVeyraFontFace>& Faces = GetDefault<UVeyraShellStyleSettings>()->FacesOf(Family);
		const TSharedRef<const FCompositeFont> Engine = FCoreStyle::GetDefaultFont();
		if (Faces.IsEmpty())
		{
			return Engine;
		}
		// The engine's other scripts and fallback stay; only the default typeface is Veyra's.
		const TSharedRef<FCompositeFont> Font = MakeShared<FCompositeFont>(*Engine);
		Font->DefaultTypeface.Fonts.Reset();
		for (const FVeyraFontFace& Face : Faces)
		{
			// Read whole on first use; the files are small, and a streamed font would hold its file open.
			Font->DefaultTypeface.AppendFont(Face.Typeface, FPaths::ProjectContentDir() / Face.File, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		}
		return Font;
	}
}

TSharedRef<const FCompositeFont> FamilyFont(EVeyraFontFamily Family)
{
	if (const TSharedRef<const FCompositeFont>* Found = Built().Find(Family))
	{
		return *Found;
	}
	return Built().Add(Family, Build(Family));
}

FSlateFontInfo FontOf(const FVeyraTextFace& Face, float Size)
{
	return FSlateFontInfo(FamilyFont(Face.Family), Size, Face.Typeface);
}

void Reset()
{
	Built().Reset();
}
}
