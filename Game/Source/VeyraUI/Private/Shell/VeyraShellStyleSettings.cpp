// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraShellStyleSettings.h"

#include "Containers/Set.h"

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
		{ TEXT("BackdropTint"), BackdropTint },
		{ TEXT("FrameColor"), FrameColor },
		{ TEXT("AllyColor"), AllyColor },
		{ TEXT("EnemyColor"), EnemyColor },
		{ TEXT("SurfaceColor"), SurfaceColor },
		{ TEXT("SurfaceRaisedColor"), SurfaceRaisedColor },
		{ TEXT("HairlineColor"), HairlineColor },
		{ TEXT("PrimaryColor"), PrimaryColor },
		{ TEXT("ItemDimTint"), ItemDimTint },
		{ TEXT("PrimaryHoveredColor"), PrimaryHoveredColor },
		{ TEXT("PrimaryTextColor"), PrimaryTextColor },
		{ TEXT("ScrimColor"), ScrimColor },
		{ TEXT("ShowcaseTint"), ShowcaseTint },
		{ TEXT("VictoryColor"), VictoryColor },
		{ TEXT("DefeatColor"), DefeatColor },
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
		{ TEXT("SmallFontSize"), SmallFontSize },
		{ TEXT("DisplayFontSize"), DisplayFontSize },
		{ TEXT("EyebrowFontSize"), EyebrowFontSize },
		{ TEXT("ButtonFontSize"), ButtonFontSize },
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
		{ TEXT("ShopWidth"), ShopWidth },
		{ TEXT("ShopHeight"), ShopHeight },
		{ TEXT("ScoreboardWidth"), ScoreboardWidth },
		{ TEXT("TilePadding"), TilePadding },
		{ TEXT("TileCornerRadius"), TileCornerRadius },
		{ TEXT("FrameWidth"), FrameWidth },
		{ TEXT("RosterTileSize"), RosterTileSize },
		{ TEXT("PickBarWidth"), PickBarWidth },
		{ TEXT("PickBarHeight"), PickBarHeight },
		{ TEXT("SeatColumnWidth"), SeatColumnWidth },
		{ TEXT("PortraitSize"), PortraitSize },
		{ TEXT("SeatSpellSize"), SeatSpellSize },
		{ TEXT("SplashWidth"), SplashWidth },
		{ TEXT("SplashHeight"), SplashHeight },
		{ TEXT("SplashCornerRadius"), SplashCornerRadius },
		{ TEXT("SpellTileSize"), SpellTileSize },
		{ TEXT("AbilityIconSize"), AbilityIconSize },
		{ TEXT("LockInWidth"), LockInWidth },
		{ TEXT("PickerWidth"), PickerWidth },
		{ TEXT("PickerTileWidth"), PickerTileWidth },
		{ TEXT("PrimaryButtonWidth"), PrimaryButtonWidth },
		{ TEXT("HomeColumnWidth"), HomeColumnWidth },
		{ TEXT("ModeCardWidth"), ModeCardWidth },
		{ TEXT("ModeCardHeight"), ModeCardHeight },
		{ TEXT("DialogWidth"), DialogWidth },
		{ TEXT("FriendsPanelWidth"), FriendsPanelWidth },
		{ TEXT("LobbySeatWidth"), LobbySeatWidth },
		{ TEXT("ShopTileSize"), ShopTileSize },
		{ TEXT("ShopMarkSize"), ShopMarkSize },
		{ TEXT("ShopQuickWidth"), ShopQuickWidth },
		{ TEXT("ShopDetailsWidth"), ShopDetailsWidth },
		{ TEXT("ReportLabelWidth"), ReportLabelWidth },
		{ TEXT("ReportColumnWidth"), ReportColumnWidth },
	};
	for (const TPair<const TCHAR*, float>& Length : Lengths)
	{
		Require(Length.Value > 0.0f, Length.Key, TEXT("must be above 0."));
	}
	// The art is imported as packages under the game's content (VeyraShellArt).
	Require(VanguardArtFolder.StartsWith(TEXT("/Game/")) && !VanguardArtFolder.EndsWith(TEXT("/")), TEXT("VanguardArtFolder"),
		TEXT("must be a folder under /Game, without a trailing slash."));
	Require(ItemArtFolder.StartsWith(TEXT("/Game/")) && !ItemArtFolder.EndsWith(TEXT("/")), TEXT("ItemArtFolder"),
		TEXT("must be a folder under /Game, without a trailing slash."));
	Require(AbilityArtFolder.StartsWith(TEXT("/Game/")) && !AbilityArtFolder.EndsWith(TEXT("/")), TEXT("AbilityArtFolder"),
		TEXT("must be a folder under /Game, without a trailing slash."));
	const auto CheckPortrait = [&Require](const FVeyraVanguardPortrait& Portrait, const TCHAR* Field) {
		const bool bInside = Portrait.Focus.X >= 0.0 && Portrait.Focus.X <= 1.0 && Portrait.Focus.Y >= 0.0 && Portrait.Focus.Y <= 1.0;
		Require(bInside, Field, TEXT("Focus must be from 0 to 1 across and down."));
		Require(Portrait.CropHeight > 0.0f && Portrait.CropHeight <= 1.0f, Field, TEXT("CropHeight must be above 0 and at most 1."));
	};
	CheckPortrait(DefaultPortrait, TEXT("DefaultPortrait"));
	TSet<FString> Seen;
	for (const FVeyraVanguardPortrait& Portrait : VanguardPortraits)
	{
		CheckPortrait(Portrait, TEXT("VanguardPortraits"));
		bool bAlreadySeen = false;
		Seen.Add(Portrait.Vanguard, &bAlreadySeen);
		Require(!Portrait.Vanguard.IsEmpty() && !bAlreadySeen, TEXT("VanguardPortraits"), TEXT("each entry names a different Vanguard."));
	}
	Require(!HomeVanguard.IsEmpty(), TEXT("HomeVanguard"), TEXT("names the Vanguard whose art fills Home."));
	Require(!LobbyStartingGoldChoices.IsEmpty() && !LobbyStartingGoldChoices.ContainsByPredicate([](float Gold) { return !(Gold >= 0.0f); }),
		TEXT("LobbyStartingGoldChoices"), TEXT("lists at least one amount, none of them negative."));
	TSet<FString> Modes;
	for (const FVeyraModeArt& Art : ModeArt)
	{
		bool bAlreadySeen = false;
		Modes.Add(Art.Mode, &bAlreadySeen);
		Require(!Art.Mode.IsEmpty() && !Art.Vanguard.IsEmpty() && !bAlreadySeen, TEXT("ModeArt"), TEXT("each entry names a different mode and a Vanguard."));
	}
	return Problems;
}

FString UVeyraShellStyleSettings::ModeArtOf(const FString& Mode) const
{
	const FVeyraModeArt* Art = ModeArt.FindByPredicate([&Mode](const FVeyraModeArt& Entry) { return Entry.Mode == Mode; });
	return Art ? Art->Vanguard : HomeVanguard;
}
