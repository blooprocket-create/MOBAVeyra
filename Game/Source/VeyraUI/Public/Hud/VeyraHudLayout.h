// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Hud/VeyraChatLogModel.h"
#include "Hud/VeyraMinimapModel.h"
#include "Math/Box2D.h"
#include "Math/Vector2D.h"
#include "Settings/VeyraInterfacePreferences.h"

class UVeyraGreyboxSettings;

/** The deck's own spacing and each of its sections' scales, in pixels per designed unit. */
struct FVeyraDeckScales
{
	float Base = 1.0f;
	float AbilityBar = 1.0f;
	float Vitals = 1.0f;
	float Spells = 1.0f;
	float Items = 1.0f;
};

/**
 * The deck measured at its sections' scales: every length the deck draws with, in pixels, and where each column starts
 * from the deck's top-left corner. Drawing and arranging read the same numbers, so they never disagree.
 */
struct FVeyraDeckGeometry
{
	FVeyraDeckScales Scales;
	FVector2D Size = FVector2D::ZeroVector;
	/** The deck's padding and the gap between its columns, at its base scale. */
	float Pad = 0.0f;
	float Gap = 0.0f;
	/** The ability bar: the portrait's side, the passive's tile, each ability's tile, the gap between them and the rank pips. */
	float Portrait = 0.0f;
	float Passive = 0.0f;
	float Ability = 0.0f;
	float AbilityGap = 0.0f;
	float Pip = 0.0f;
	/** The passive and the abilities across, which the vitals under them span too. */
	float AbilityRow = 0.0f;
	/** The vitals: health's and the resource's heights, and the space between them. */
	float Health = 0.0f;
	float Resource = 0.0f;
	float BarSpacing = 0.0f;
	/** The Flux Spells: a tile's side, the gap between the two, both across, and the vision tool's line under them. */
	float Spell = 0.0f;
	float SpellGap = 0.0f;
	float SpellsWidth = 0.0f;
	float ToolLine = 0.0f;
	/** The items: a tile's side, the gap between tiles, the grid across, and the Gold line under it. */
	float Item = 0.0f;
	float ItemGap = 0.0f;
	float ItemsWidth = 0.0f;
	float GoldLine = 0.0f;
	/** Where each column starts, from the deck's top-left corner. */
	FVector2D PortraitAt = FVector2D::ZeroVector;
	FVector2D AbilitiesAt = FVector2D::ZeroVector;
	FVector2D VitalsAt = FVector2D::ZeroVector;
	FVector2D SpellsAt = FVector2D::ZeroVector;
	FVector2D ToolAt = FVector2D::ZeroVector;
	FVector2D ItemsAt = FVector2D::ZeroVector;
};

/** Where the HUD's edge-anchored components sit on one viewport, inside the player's safe area (ADR-059 §1–§2). */
struct FVeyraHudArrangement
{
	/** The safe area's inset from the left and right edges (X) and from the top and bottom (Y), in pixels. */
	FVector2D Inset = FVector2D::ZeroVector;
	/** HUD Scale's pixels per designed unit on this viewport. */
	float Base = 1.0f;
	/** The team panels', combat text's and overhead bars' pixels per designed unit. */
	float TeamPanels = 1.0f;
	float CombatText = 1.0f;
	float OverheadBars = 1.0f;
	/** The deck, measured as it is drawn, and its top-left corner. */
	FVeyraDeckGeometry Deck;
	FVector2D DeckTopLeft = FVector2D::ZeroVector;
	/** How much the deck shrank to fit beside the minimap inside the safe area; 1 when it did not have to. */
	float DeckFit = 1.0f;
	FVeyraMinimapFrame Minimap;
	/** The chat log and composer, raised above the deck where the deck reaches under them. */
	FVeyraChatFrame Chat;
};

namespace VeyraHudLayout
{
	/** The safe area's inset on Viewport for the player's margins, each a fraction of the viewport on its axis (ADR-059 §2). */
	VEYRAUI_API FVector2D SafeInset(const FVector2D& Viewport, const FVector2D& SafeArea);

	/** The deck's scales at Base pixels per designed unit, each section's own scale times Fit. */
	VEYRAUI_API FVeyraDeckScales DeckScales(float Base, const FVeyraHudScales& Scales, float Fit);

	/** The deck with AbilityCount abilities, measured at Scales. */
	VEYRAUI_API FVeyraDeckGeometry MeasureDeck(const UVeyraGreyboxSettings& Settings, const FVeyraDeckScales& Scales, int32 AbilityCount);

	/**
	 * How much a deck of Size (at its designed fit) must shrink to fit along the bottom inside Inset and clear of Avoid
	 * (the minimap) by Gap, never below MinimumFit; and, at that fit, its top-left corner: centred, moved left as far as
	 * it must to clear Avoid, and never past the safe area's left edge.
	 */
	VEYRAUI_API float PlaceDeck(const FVector2D& Viewport, const FVector2D& Inset, const FVector2D& Size, const FBox2D& Avoid, double Gap, double MinimumFit,
		FVector2D& OutTopLeft);

	/**
	 * The HUD on Viewport as Preferences set it: the safe area, the minimap (its frame reaching HalfExtent units), the deck
	 * with AbilityCount abilities placed beside it, and the chat raised clear of the deck.
	 */
	VEYRAUI_API FVeyraHudArrangement Arrange(const FVector2D& Viewport, const UVeyraGreyboxSettings& Settings, const FVeyraInterfacePreferences& Preferences,
		int32 AbilityCount, double HalfExtent = 0.0);
}
