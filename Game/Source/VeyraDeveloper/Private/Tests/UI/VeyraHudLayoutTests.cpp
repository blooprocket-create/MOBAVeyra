// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "Greybox/VeyraGreyboxSettings.h"
#include "Hud/VeyraHudLayout.h"
#include "Settings/VeyraInterfacePreferences.h"

namespace VeyraHudLayoutTests
{
	/** The four abilities every deck shows. */
	constexpr int32 Abilities = 4;
	const FVector2D Screen(1920.0, 1080.0);

	/** The developer's HUD with the player's settings at their defaults and a minimap of Size pixels. */
	FVeyraInterfacePreferences Defaults(float MinimapSize = 200.0f)
	{
		FVeyraInterfacePreferences Preferences;
		Preferences.MinimapSize = MinimapSize;
		return Preferences;
	}

	// Veyra.UI.HudLayout.*: component scales, the safe area and how the deck, the minimap and the chat keep clear of each
	// other (Settings Bible §3.1; ADR-059 §1–§2).
	TEST_CLASS(HudLayout, "Veyra.UI")
	{
		const UVeyraGreyboxSettings& Hud() const { return *GetDefault<UVeyraGreyboxSettings>(); }

		TEST_METHOD(TheDesignedHudKeepsItsAnchors)
		{
			const FVeyraHudArrangement Layout = VeyraHudLayout::Arrange(Screen, Hud(), Defaults(), Abilities);
			const float Gap = Hud().DeckGap * Layout.Base;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Layout.DeckFit, 1.0f)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Layout.DeckTopLeft.X, (Screen.X - Layout.Deck.Size.X) / 2.0, 0.01), TEXT("centred along the bottom")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Layout.DeckTopLeft.Y + Layout.Deck.Size.Y, Screen.Y - Gap, 0.01)));
			ASSERT_THAT(IsTrue(Layout.Minimap.Origin.Equals(FVector2D(Screen.X - Hud().HudMargin - 200.0, Screen.Y - Hud().HudMargin - 200.0)), TEXT("bottom right")));
			ASSERT_THAT(IsTrue(Layout.Inset.IsZero()));
		}

		TEST_METHOD(EachDeckSectionScalesOnItsOwn)
		{
			FVeyraHudScales Scales;
			const FVeyraDeckGeometry Designed = VeyraHudLayout::MeasureDeck(Hud(), VeyraHudLayout::DeckScales(1.0f, Scales, 1.0f), Abilities);
			Scales.Items = 1.5f;
			const FVeyraDeckGeometry Larger = VeyraHudLayout::MeasureDeck(Hud(), VeyraHudLayout::DeckScales(1.0f, Scales, 1.0f), Abilities);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Larger.ItemsWidth, Designed.ItemsWidth * 1.5f) && FMath::IsNearlyEqual(Larger.AbilityRow, Designed.AbilityRow)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Larger.Size.X - Designed.Size.X, Designed.ItemsWidth * 0.5, 0.01), TEXT("only the items grew")));
			// A section's columns start where the ones before them end.
			ASSERT_THAT(IsTrue(Larger.ItemsAt.X > Larger.SpellsAt.X + Larger.SpellsWidth && Larger.SpellsAt.X > Larger.AbilitiesAt.X + Larger.AbilityRow));
			ASSERT_THAT(IsTrue(Larger.VitalsAt.Y > Larger.AbilitiesAt.Y + Larger.Ability, TEXT("the vitals under the abilities")));
		}

		TEST_METHOD(TheDeckMovesLeftOfTheMinimapThenShrinksToFit)
		{
			const FBox2D Minimap(FVector2D(800.0, 400.0), FVector2D(980.0, 580.0));
			FVector2D At;
			// It fits once moved left: its right edge stops a gap short of the minimap.
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraHudLayout::PlaceDeck(FVector2D(1000.0, 600.0), FVector2D::ZeroVector, FVector2D(600.0, 100.0), Minimap, 10.0, 0.5, At), 1.0f)));
			ASSERT_THAT(IsTrue(At.Equals(FVector2D(190.0, 490.0)), At.ToString()));
			// Too wide to fit beside it: it shrinks to the room between the safe area's edge and the minimap.
			const float Fit = VeyraHudLayout::PlaceDeck(FVector2D(1000.0, 600.0), FVector2D::ZeroVector, FVector2D(900.0, 100.0), Minimap, 10.0, 0.5, At);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Fit, 780.0f / 900.0f, 0.001f) && FMath::IsNearlyEqual(At.X, 10.0) && FMath::IsNearlyEqual(At.Y, 590.0 - 100.0 * Fit, 0.01)));
			// Never below the tested limit.
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraHudLayout::PlaceDeck(FVector2D(1000.0, 600.0), FVector2D::ZeroVector, FVector2D(3000.0, 100.0), Minimap, 10.0, 0.5, At), 0.5f)));
			// A minimap above the deck's height leaves the bottom free.
			const FBox2D High(FVector2D(800.0, 50.0), FVector2D(980.0, 200.0));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraHudLayout::PlaceDeck(FVector2D(1000.0, 600.0), FVector2D::ZeroVector, FVector2D(900.0, 100.0), High, 10.0, 0.5, At), 1.0f)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(At.X, 50.0)));
		}

		TEST_METHOD(TheSafeAreaMovesEdgeComponentsInward)
		{
			FVeyraInterfacePreferences Preferences = Defaults();
			Preferences.SafeArea = FVector2D(0.05, 0.1);
			const FVeyraHudArrangement Layout = VeyraHudLayout::Arrange(Screen, Hud(), Preferences, Abilities);
			const FVeyraHudArrangement Designed = VeyraHudLayout::Arrange(Screen, Hud(), Defaults(), Abilities);
			ASSERT_THAT(IsTrue(Layout.Inset.Equals(FVector2D(96.0, 108.0)), Layout.Inset.ToString()));
			ASSERT_THAT(IsTrue(Layout.Minimap.Origin.Equals(Designed.Minimap.Origin - FVector2D(96.0, 108.0))));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Layout.DeckTopLeft.Y, Designed.DeckTopLeft.Y - 108.0, 0.01)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Layout.Chat.InputTopLeft.X, Designed.Chat.InputTopLeft.X + 96.0, 0.01)));
			ASSERT_THAT(IsTrue(Layout.Chat.InputTopLeft.Y <= Designed.Chat.InputTopLeft.Y - 108.0 + 0.01));
		}

		TEST_METHOD(NoComponentCoversAnotherAtTheLargestScales)
		{
			FVeyraInterfacePreferences Preferences = Defaults(/*MinimapSize*/ 300.0f);
			Preferences.HudScale = 1.5f;
			Preferences.HudScales.AbilityBar = Preferences.HudScales.Vitals = Preferences.HudScales.Items = Preferences.HudScales.Spells = Preferences.HudScales.Chat = 1.5f;
			for (const FVector2D& Viewport : { FVector2D(1920.0, 1080.0), FVector2D(1280.0, 720.0), FVector2D(2560.0, 1080.0) })
			{
				const FVeyraHudArrangement Layout = VeyraHudLayout::Arrange(Viewport, Hud(), Preferences, Abilities);
				const FBox2D Deck(Layout.DeckTopLeft, Layout.DeckTopLeft + Layout.Deck.Size);
				const FBox2D Minimap(Layout.Minimap.Origin, Layout.Minimap.Origin + FVector2D(Layout.Minimap.Size));
				const FBox2D Chat(Layout.Chat.InputTopLeft, Layout.Chat.InputTopLeft + Layout.Chat.InputSize);
				const bool bFits = Layout.DeckFit > Hud().DeckMinimumFit;
				ASSERT_THAT(IsTrue(!bFits || !Deck.Intersect(Minimap), Viewport.ToString()));
				ASSERT_THAT(IsTrue(!Deck.Intersect(Chat), Viewport.ToString()));
				ASSERT_THAT(IsTrue(Deck.Min.X >= 0.0 && Deck.Max.X <= Viewport.X && Deck.Max.Y <= Viewport.Y, Viewport.ToString()));
			}
		}
	};
}

#endif
