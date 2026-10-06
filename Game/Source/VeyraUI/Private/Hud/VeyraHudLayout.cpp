// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Hud/VeyraHudLayout.h"

#include "Greybox/VeyraGreyboxSettings.h"
#include "Settings/VeyraInterfacePreferences.h"

namespace
{
	/** A row of text's height over its type's size, before Slate measures it. */
	constexpr float LineOverType = 1.35f;
	/** The tiles of the item grid across and down. */
	constexpr int32 ItemColumns = 3;
	constexpr int32 ItemRows = 2;
	constexpr int32 SpellTiles = 2;

	/** Whether the spans [MinA, MaxA] and [MinB, MaxB] overlap. */
	bool Overlaps(double MinA, double MaxA, double MinB, double MaxB)
	{
		return MinA < MaxB && MinB < MaxA;
	}
}

namespace VeyraHudLayout
{
FVector2D SafeInset(const FVector2D& Viewport, const FVector2D& SafeArea)
{
	return FVector2D(Viewport.X * FMath::Max(0.0, SafeArea.X), Viewport.Y * FMath::Max(0.0, SafeArea.Y));
}

FVeyraDeckScales DeckScales(float Base, const FVeyraHudScales& Scales, float Fit)
{
	FVeyraDeckScales Out;
	Out.Base = Base * Fit;
	Out.AbilityBar = Base * Scales.AbilityBar * Fit;
	Out.Vitals = Base * Scales.Vitals * Fit;
	Out.Spells = Base * Scales.Spells * Fit;
	Out.Items = Base * Scales.Items * Fit;
	return Out;
}

FVeyraDeckGeometry MeasureDeck(const UVeyraGreyboxSettings& Settings, const FVeyraDeckScales& Scales, int32 AbilityCount)
{
	FVeyraDeckGeometry Deck;
	Deck.Scales = Scales;
	Deck.Pad = Settings.DeckPadding * Scales.Base;
	Deck.Gap = Settings.DeckGap * Scales.Base;

	Deck.Portrait = Settings.PortraitSize * Scales.AbilityBar;
	Deck.Passive = Settings.SmallTileSize * Scales.AbilityBar;
	Deck.Ability = Settings.AbilityTileSize * Scales.AbilityBar;
	Deck.AbilityGap = Settings.DeckGap * Scales.AbilityBar;
	Deck.Pip = Settings.DeckPipHeight * Scales.AbilityBar;
	const int32 Abilities = FMath::Max(0, AbilityCount);
	Deck.AbilityRow = Deck.Passive + Deck.AbilityGap + Abilities * Deck.Ability + FMath::Max(0, Abilities - 1) * Deck.AbilityGap;

	Deck.Health = Settings.DeckHealthHeight * Scales.Vitals;
	Deck.Resource = Settings.DeckResourceHeight * Scales.Vitals;
	Deck.BarSpacing = Settings.DeckBarSpacing * Scales.Vitals;

	Deck.Spell = Settings.SmallTileSize * Scales.Spells;
	Deck.SpellGap = Settings.DeckGap * Scales.Spells;
	Deck.SpellsWidth = SpellTiles * Deck.Spell + (SpellTiles - 1) * Deck.SpellGap;
	Deck.ToolLine = Settings.HudSmallFontSize * LineOverType * Scales.Spells;

	Deck.Item = Settings.ItemTileSize * Scales.Items;
	Deck.ItemGap = Settings.DeckGap * Scales.Items;
	Deck.ItemsWidth = ItemColumns * Deck.Item + (ItemColumns - 1) * Deck.ItemGap;
	Deck.GoldLine = Settings.HudBodyFontSize * LineOverType * Scales.Items;

	// The columns from the left: the portrait, the ability bar over the vitals, the Flux Spells, the items.
	const float AbilityRowHeight = Deck.Ability + Deck.AbilityGap + Deck.Pip;
	const float CentreHeight = AbilityRowHeight + Deck.Gap + Deck.Health + Deck.BarSpacing + Deck.Resource;
	const float SpellsTop = FMath::Max(0.0f, (Deck.Ability - Deck.Spell) / 2.0f);
	const float SpellsHeight = SpellsTop + Deck.Spell + Deck.SpellGap + Deck.ToolLine;
	const float ItemsHeight = ItemRows * Deck.Item + (ItemRows - 1) * Deck.ItemGap + Deck.ItemGap / 2.0f + Deck.GoldLine;
	const float Content = FMath::Max(FMath::Max(Deck.Portrait, CentreHeight), FMath::Max(SpellsHeight, ItemsHeight));

	const float CentreX = Deck.Pad + Deck.Portrait + Deck.Gap * 2.0f;
	const float SpellsX = CentreX + Deck.AbilityRow + Deck.Gap * 2.0f;
	const float ItemsX = SpellsX + Deck.SpellsWidth + Deck.Gap * 2.0f;
	Deck.Size = FVector2D(ItemsX + Deck.ItemsWidth + Deck.Pad, Deck.Pad * 2.0f + Content);
	Deck.PortraitAt = FVector2D(Deck.Pad, (Deck.Size.Y - Deck.Portrait) / 2.0f);
	Deck.AbilitiesAt = FVector2D(CentreX, Deck.Pad);
	Deck.VitalsAt = FVector2D(CentreX, Deck.Pad + AbilityRowHeight + Deck.Gap);
	Deck.SpellsAt = FVector2D(SpellsX, Deck.Pad + SpellsTop);
	Deck.ToolAt = FVector2D(SpellsX, Deck.Pad + SpellsTop + Deck.Spell + Deck.SpellGap);
	Deck.ItemsAt = FVector2D(ItemsX, Deck.Pad);
	return Deck;
}

float PlaceDeck(const FVector2D& Viewport, const FVector2D& Inset, const FVector2D& Size, const FBox2D& Avoid, double Gap, double MinimumFit, FVector2D& OutTopLeft)
{
	const double Bottom = Viewport.Y - Inset.Y - Gap;
	const double Left = Inset.X + Gap;
	double Right = Viewport.X - Inset.X - Gap;
	// The minimap shares the bottom edge: the deck stays clear of it wherever their heights meet.
	if (Avoid.bIsValid && Overlaps(Bottom - Size.Y, Bottom, Avoid.Min.Y, Avoid.Max.Y))
	{
		Right = FMath::Min(Right, Avoid.Min.X - Gap);
	}
	const double Room = FMath::Max(0.0, Right - Left);
	const double Fit = Size.X > 0.0 ? FMath::Clamp(Room / Size.X, MinimumFit, 1.0) : 1.0;
	const FVector2D Fitted = Size * Fit;
	double X = (Viewport.X - Fitted.X) / 2.0;
	X = FMath::Min(X, Right - Fitted.X);
	X = FMath::Max(X, Left);
	OutTopLeft = FVector2D(X, Bottom - Fitted.Y);
	return static_cast<float>(Fit);
}

FVeyraHudArrangement Arrange(const FVector2D& Viewport, const UVeyraGreyboxSettings& Settings, const FVeyraInterfacePreferences& Preferences, int32 AbilityCount,
	double HalfExtent)
{
	FVeyraHudArrangement Out;
	const float Reference = Settings.HudReferenceHeight > 0.0f ? static_cast<float>(Viewport.Y) / Settings.HudReferenceHeight : 1.0f;
	Out.Base = Reference * Preferences.HudScale;
	Out.TeamPanels = Out.Base * Preferences.HudScales.TeamPanels;
	Out.CombatText = Preferences.HudScales.CombatText;
	Out.OverheadBars = Preferences.HudScales.OverheadBars;
	Out.Inset = SafeInset(Viewport, Preferences.SafeArea);

	// The minimap keeps its own size, in the bottom-right corner of the safe area.
	Out.Minimap = VeyraMinimap::FrameFor(Viewport, Preferences.MinimapSize, Settings.HudMargin, HalfExtent);
	Out.Minimap.Origin -= Out.Inset;
	const FBox2D MinimapBox(Out.Minimap.Origin, Out.Minimap.Origin + FVector2D(Out.Minimap.Size));

	// The deck along the bottom, beside the minimap, shrinking only when it cannot fit at the player's scales.
	const FVeyraDeckGeometry Designed = MeasureDeck(Settings, DeckScales(Out.Base, Preferences.HudScales, 1.0f), AbilityCount);
	FVector2D TopLeft;
	Out.DeckFit = PlaceDeck(Viewport, Out.Inset, Designed.Size, MinimapBox, Settings.DeckGap * Out.Base, Settings.DeckMinimumFit, TopLeft);
	Out.Deck = Out.DeckFit < 1.0f ? MeasureDeck(Settings, DeckScales(Out.Base, Preferences.HudScales, Out.DeckFit), AbilityCount) : Designed;
	Out.DeckTopLeft = TopLeft;

	// The chat at the bottom left of the safe area; where the deck reaches under it, the chat rises above the deck.
	Out.Chat = VeyraChatLog::FrameFor(Viewport, Settings, Preferences.HudScale * Preferences.HudScales.Chat);
	Out.Chat.InputTopLeft += FVector2D(Out.Inset.X, -Out.Inset.Y);
	Out.Chat.LogBottomLeft += FVector2D(Out.Inset.X, -Out.Inset.Y);
	const double ChatBottom = Out.Chat.InputTopLeft.Y + Out.Chat.InputSize.Y;
	const double Clear = Out.DeckTopLeft.Y - Settings.DeckGap * Out.Base;
	if (Overlaps(Out.Chat.InputTopLeft.X, Out.Chat.InputTopLeft.X + Out.Chat.Width, Out.DeckTopLeft.X, Out.DeckTopLeft.X + Out.Deck.Size.X) && ChatBottom > Clear)
	{
		const double Raise = ChatBottom - Clear;
		Out.Chat.InputTopLeft.Y -= Raise;
		Out.Chat.LogBottomLeft.Y -= Raise;
	}

	// The selected unit's frame in the top left, under Team Flux, at the team panels' scale (ADR-066 §3).
	const FVector2D FrameTopLeft(Out.Inset.X + Settings.DeckGap * Out.TeamPanels, Out.Inset.Y + Settings.TargetFrameTop * Out.TeamPanels);
	Out.TargetFrame = FBox2D(FrameTopLeft, FrameTopLeft + FVector2D(Settings.TargetFrameWidth, Settings.TargetFrameHeight) * Out.TeamPanels);
	return Out;
}
}
