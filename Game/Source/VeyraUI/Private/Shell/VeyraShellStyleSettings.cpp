// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraShellStyleSettings.h"

TArray<FString> UVeyraShellStyleSettings::Validate() const
{
	TArray<FString> Problems;
	const auto Require = [&Problems](bool bValid, const TCHAR* Field, const TCHAR* Message) {
		if (!bValid)
		{
			Problems.Add(FString::Printf(TEXT("%s: %s"), Field, Message));
		}
	};
	// A colour of zero alpha draws nothing, so every colour must be visible.
	struct FNamedColor
	{
		const TCHAR* Field;
		const FLinearColor& Color;
	};
	const FNamedColor Colors[] = {
		{ TEXT("BackgroundColor"), BackgroundColor },
		{ TEXT("PanelColor"), PanelColor },
		{ TEXT("TextColor"), TextColor },
		{ TEXT("MutedTextColor"), MutedTextColor },
		{ TEXT("AccentColor"), AccentColor },
		{ TEXT("ProblemColor"), ProblemColor },
		{ TEXT("ButtonColor"), ButtonColor },
		{ TEXT("ButtonHoveredColor"), ButtonHoveredColor },
		{ TEXT("ButtonPressedColor"), ButtonPressedColor },
		{ TEXT("ButtonDisabledColor"), ButtonDisabledColor },
		{ TEXT("SelectedColor"), SelectedColor },
		{ TEXT("MenuScrimColor"), MenuScrimColor },
	};
	for (const FNamedColor& Named : Colors)
	{
		Require(Named.Color.A > 0.0f, Named.Field, TEXT("the colour must not be fully transparent."));
	}
	const TPair<const TCHAR*, int32> FontSizes[] = {
		{ TEXT("TitleFontSize"), TitleFontSize },
		{ TEXT("HeadingFontSize"), HeadingFontSize },
		{ TEXT("BodyFontSize"), BodyFontSize },
		{ TEXT("CountdownFontSize"), CountdownFontSize },
	};
	for (const TPair<const TCHAR*, int32>& Size : FontSizes)
	{
		Require(Size.Value >= 1, Size.Key, TEXT("must be at least 1."));
	}
	const TPair<const TCHAR*, float> Lengths[] = {
		{ TEXT("ScreenPadding"), ScreenPadding },
		{ TEXT("Spacing"), Spacing },
		{ TEXT("ButtonPadding"), ButtonPadding },
		{ TEXT("CardWidth"), CardWidth },
		{ TEXT("CardHeight"), CardHeight },
		{ TEXT("MenuWidth"), MenuWidth },
	};
	for (const TPair<const TCHAR*, float>& Length : Lengths)
	{
		Require(Length.Value > 0.0f, Length.Key, TEXT("must be above 0."));
	}
	return Problems;
}
