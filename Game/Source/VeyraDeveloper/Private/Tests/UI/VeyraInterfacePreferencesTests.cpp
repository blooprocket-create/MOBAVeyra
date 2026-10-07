// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Greybox/VeyraGreyboxSettings.h"
#include "Settings/VeyraInterfacePreferences.h"
#include "Settings/VeyraSettingsModels.h"
#include "VeyraSettingsStore.h"
#include "VeyraSettingsSubsystem.h"

namespace VeyraInterfacePreferencesTests
{
	using namespace VeyraInterfacePreferences;

	FVeyraSettingsRegistry InterfaceRegistry()
	{
		FVeyraSettingsRegistry Registry;
		UVeyraSettingsSubsystem::LoadRegistry(Registry);
		return Registry;
	}

	// Veyra.UI.InterfacePreferences.*: the player's HUD, minimap and in-match controls (ADR-024 §6; Settings Bible §3, SET-56, SET-83).
	TEST_CLASS(InterfacePreferences, "Veyra.UI")
	{
		/** Loaded before each test: CQTest builds its classes while registering them, which in a packaged client is before the engine starts. */
		FVeyraSettingsRegistry Registry;

		BEFORE_EACH()
		{
			Registry = InterfaceRegistry();
		}
		static const UVeyraGreyboxSettings& HudSettings() { return *GetDefault<UVeyraGreyboxSettings>(); }

		TEST_METHOD(TheDefaultsAreTheDevelopersInterface)
		{
			const FVeyraInterfacePreferences Plain = Resolve(HudSettings(), nullptr);
			const FVeyraSettingsStore Defaults(Registry);
			const FVeyraInterfacePreferences Untouched = Resolve(HudSettings(), &Defaults);
			for (const FVeyraInterfacePreferences* Preferences : { &Plain, &Untouched })
			{
				ASSERT_THAT(IsTrue(Preferences->HudScale == 1.0f && Preferences->MinimapSize == HudSettings().MinimapSize && Preferences->MinimapUnitIcon == HudSettings().MinimapUnitIcon));
				ASSERT_THAT(IsTrue(Preferences->PingSeconds == HudSettings().PingSeconds && Preferences->bMinimapClickMovesCamera && Preferences->bMinimapRightClickMoves));
				ASSERT_THAT(IsTrue(!Preferences->bShowFps && !Preferences->bShowPing && !Preferences->bScoreboardToggles && Preferences->bConfineCursor));
				ASSERT_THAT(IsTrue(Preferences->IndicatorThickness == HudSettings().IndicatorThickness, TEXT("Standard boundaries")));
				ASSERT_THAT(IsTrue(Preferences->bClickMarkers, TEXT("click markers show")));
			}
		}

		TEST_METHOD(ThePlayersSettingsTakeTheirPlace)
		{
			FVeyraSettingsStore Store(Registry);
			Store.Set(HudScale(), TEXT("120"));
			Store.Set(MinimapScale(), TEXT("150"));
			Store.Set(MinimapIconScale(), TEXT("50"));
			Store.Set(PingSeconds(), TEXT("6"));
			Store.Set(MinimapRightClickMoves(), VeyraSettings::Off());
			Store.Set(ScoreboardMode(), TEXT("Toggle"));
			Store.Set(ConfineCursor(), VeyraSettings::Off());
			Store.Set(ShowFps(), VeyraSettings::On());
			Store.Set(IndicatorBoundary(), TEXT("Thick"));
			Store.Set(ClickMarkers(), VeyraSettings::Off());
			Store.Set(EffectsVolume(), TEXT("40"));
			const FVeyraInterfacePreferences Preferences = Resolve(HudSettings(), &Store);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Preferences.EffectsVolume, 0.4f), TEXT("the fight's sounds at 40%")));
			ASSERT_THAT(IsTrue(Preferences.IndicatorThickness == HudSettings().IndicatorThickThickness && HudSettings().IndicatorThickThickness > HudSettings().IndicatorThickness));
			ASSERT_THAT(IsFalse(Preferences.bClickMarkers));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Preferences.HudScale, 1.2f) && FMath::IsNearlyEqual(Preferences.MinimapSize, HudSettings().MinimapSize * 1.5f)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Preferences.MinimapVanguardIcon, HudSettings().MinimapVanguardIcon * 0.5f) && Preferences.PingSeconds == 6.0f));
			ASSERT_THAT(IsTrue(!Preferences.bMinimapRightClickMoves && Preferences.bScoreboardToggles && !Preferences.bConfineCursor && Preferences.bShowFps));
		}

		// ADR-059 §1–§2: each component's scale on top of HUD Scale, and the safe area's margins as fractions of the screen.
		TEST_METHOD(EachHudComponentTakesItsOwnScaleAndTheSafeAreaItsMargins)
		{
			FVeyraSettingsStore Store(Registry);
			const FVeyraInterfacePreferences Designed = Resolve(HudSettings(), &Store);
			ASSERT_THAT(IsTrue(Designed.HudScales == FVeyraHudScales() && Designed.SafeArea.IsZero(), TEXT("each at its designed size, with no margin")));
			Store.Set(AbilityBarScale(), TEXT("150"));
			Store.Set(VitalsScale(), TEXT("125"));
			Store.Set(ItemScale(), TEXT("75"));
			Store.Set(SpellScale(), TEXT("90"));
			Store.Set(TeamPanelScale(), TEXT("110"));
			Store.Set(ChatScale(), TEXT("80"));
			Store.Set(CombatTextScale(), TEXT("140"));
			Store.Set(OverheadBarScale(), TEXT("120"));
			Store.Set(SafeAreaHorizontal(), TEXT("4"));
			Store.Set(SafeAreaVertical(), TEXT("10"));
			const FVeyraHudScales Scales = Resolve(HudSettings(), &Store).HudScales;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Scales.AbilityBar, 1.5f) && FMath::IsNearlyEqual(Scales.Vitals, 1.25f) && FMath::IsNearlyEqual(Scales.Items, 0.75f)
				&& FMath::IsNearlyEqual(Scales.Spells, 0.9f) && FMath::IsNearlyEqual(Scales.TeamPanels, 1.1f) && FMath::IsNearlyEqual(Scales.Chat, 0.8f)
				&& FMath::IsNearlyEqual(Scales.CombatText, 1.4f) && FMath::IsNearlyEqual(Scales.OverheadBars, 1.2f)));
			ASSERT_THAT(IsTrue(Resolve(HudSettings(), &Store).SafeArea.Equals(FVector2D(0.04, 0.1))));
			// The registry keeps each within its tested limits.
			ASSERT_THAT(IsTrue(Store.Set(ItemScale(), TEXT("200")) == EVeyraSettingChange::InvalidValue && FMath::IsNearlyEqual(Resolve(HudSettings(), &Store).HudScales.Items, 0.75f)));
			ASSERT_THAT(IsTrue(Store.Set(SafeAreaVertical(), TEXT("20")) == EVeyraSettingChange::InvalidValue));
		}

		// ADR-059 §3: numbers and the sweep on, tenths below ten seconds, until the player chooses otherwise.
		TEST_METHOD(TheCooldownDisplayFollowsThePlayersChoices)
		{
			FVeyraSettingsStore Store(Registry);
			ASSERT_THAT(IsTrue(Resolve(HudSettings(), &Store).Cooldowns == FVeyraCooldownDisplay()));
			Store.Set(CooldownNumbers(), VeyraSettings::Off());
			Store.Set(CooldownSweep(), VeyraSettings::Off());
			Store.Set(CooldownPrecision(), TEXT("Whole"));
			const FVeyraCooldownDisplay Display = Resolve(HudSettings(), &Store).Cooldowns;
			ASSERT_THAT(IsTrue(!Display.bNumbers && !Display.bSweep && !Display.bTenths));
		}

		// ADR-059 §4: By Category, durations on and the standard chips, until the player chooses otherwise.
		TEST_METHOD(TheStatusRowFollowsThePlayersChoices)
		{
			FVeyraSettingsStore Store(Registry);
			ASSERT_THAT(IsTrue(Resolve(HudSettings(), &Store).Statuses == FVeyraStatusDisplay()));
			Store.Set(StatusSort(), TEXT("ByRemainingDuration"));
			Store.Set(StatusHighContrast(), VeyraSettings::On());
			Store.Set(StatusDurations(), VeyraSettings::Off());
			const FVeyraStatusDisplay Display = Resolve(HudSettings(), &Store).Statuses;
			ASSERT_THAT(IsTrue(Display.Sort == EVeyraStatusSort::ByRemainingDuration && Display.bHighContrast && !Display.bDurations));
			Store.Set(StatusSort(), TEXT("ByApplicationOrder"));
			ASSERT_THAT(IsTrue(Resolve(HudSettings(), &Store).Statuses.Sort == EVeyraStatusSort::ByApplicationOrder));
		}

		TEST_METHOD(TheReadoutsShowOnlyWhatThePlayerAskedFor)
		{
			FVeyraInterfacePreferences Preferences;
			ASSERT_THAT(IsTrue(DescribeReadouts(Preferences, 59.6f, 35.2f).IsEmpty()));
			Preferences.bShowFps = true;
			ASSERT_THAT(AreEqual(FString(TEXT("60 FPS")), DescribeReadouts(Preferences, 59.6f, 35.2f)));
			Preferences.bShowPing = true;
			ASSERT_THAT(AreEqual(FString(TEXT("60 FPS   35 ms")), DescribeReadouts(Preferences, 59.6f, 35.2f)));
			ASSERT_THAT(AreEqual(FString(TEXT("60 FPS")), DescribeReadouts(Preferences, 59.6f, TOptional<float>()), TEXT("no ping until one is known")));
		}

		// ADR-053 §1–§2: Leave Match asks first, and a match found plays its sound and asks for attention, unless turned off.
		TEST_METHOD(TheLeaveConfirmationAndTheMatchFoundAlertFollowTheirSettings)
		{
			const FVeyraSettingsStore Defaults(Registry);
			const FVeyraInterfacePreferences Untouched = Resolve(HudSettings(), &Defaults);
			ASSERT_THAT(IsTrue(Untouched.bConfirmLeaveMatch && Untouched.bMatchReadySound && Untouched.bBackgroundMatchNotification, TEXT("each is On by default")));
			FVeyraSettingsStore Store(Registry);
			Store.Set(ConfirmLeaveMatch(), VeyraSettings::Off());
			Store.Set(MatchReadySound(), VeyraSettings::Off());
			Store.Set(BackgroundMatchNotification(), VeyraSettings::Off());
			const FVeyraInterfacePreferences Off = Resolve(HudSettings(), &Store);
			ASSERT_THAT(IsTrue(!Off.bConfirmLeaveMatch && !Off.bMatchReadySound && !Off.bBackgroundMatchNotification));
		}

		// ADR-052 §2: Fluxborn bars by side, jungle creatures' by engagement; every other bar whenever its unit is seen.
		TEST_METHOD(BarsShowAsThePlayerSetThem)
		{
			const FVeyraSettingsStore Defaults(Registry);
			const FVeyraInterfacePreferences Untouched = Resolve(HudSettings(), &Defaults);
			ASSERT_THAT(IsTrue(Untouched.AlliedFluxbornBars == EVeyraBarVisibility::Always && Untouched.EnemyFluxbornBars == EVeyraBarVisibility::Always
				&& Untouched.JungleBars == EVeyraBarVisibility::Always, TEXT("every bar shows by default")));
			// Every option the registry offers is one the HUD reads, and no two read alike.
			for (const FVeyraContentId* Id : { &AlliedFluxbornBars(), &EnemyFluxbornBars(), &JungleBars() })
			{
				const TOptional<FVeyraSettingInfo> Setting = VeyraSettings::Find(Registry, *Id);
				ASSERT_THAT(IsTrue(Setting.IsSet() && Setting->Choice));
				TSet<EVeyraBarVisibility> Seen;
				for (const FString& Option : Setting->Choice->Options)
				{
					const EVeyraBarVisibility Visibility = ParseBars(Option);
					ASSERT_THAT(IsTrue(Visibility != EVeyraBarVisibility::Always || Option == TEXT("Always"), FString::Printf(TEXT("the HUD cannot read %s"), *Option)));
					ASSERT_THAT(IsFalse(Seen.Contains(Visibility)));
					Seen.Add(Visibility);
				}
			}

			FVeyraSettingsStore Store(Registry);
			Store.Set(AlliedFluxbornBars(), TEXT("WhenDamaged"));
			Store.Set(EnemyFluxbornBars(), TEXT("WhenTargeted"));
			Store.Set(JungleBars(), TEXT("WhenEngaged"));
			const FVeyraInterfacePreferences Preferences = Resolve(HudSettings(), &Store);
			FVeyraBarFacts Ally;
			Ally.Kind = EVeyraUnitKind::Fluxborn;
			Ally.bAllied = true;
			Ally.bTargeted = true;
			ASSERT_THAT(IsFalse(ShowsBar(Preferences, Ally), TEXT("an ally at full Health")));
			Ally.bDamaged = true;
			ASSERT_THAT(IsTrue(ShowsBar(Preferences, Ally)));

			FVeyraBarFacts Enemy;
			Enemy.Kind = EVeyraUnitKind::Fluxborn;
			Enemy.bDamaged = true;
			ASSERT_THAT(IsFalse(ShowsBar(Preferences, Enemy), TEXT("a hurt enemy no one targets")));
			Enemy.bTargeted = true;
			ASSERT_THAT(IsTrue(ShowsBar(Preferences, Enemy)));

			FVeyraBarFacts Creature;
			Creature.Kind = EVeyraUnitKind::Wildlife;
			ASSERT_THAT(IsFalse(ShowsBar(Preferences, Creature), TEXT("a creature at rest")));
			Creature.bFighting = true;
			ASSERT_THAT(IsTrue(ShowsBar(Preferences, Creature), TEXT("fighting at full Health")));
			Creature.bFighting = false;
			Creature.bDamaged = true;
			ASSERT_THAT(IsTrue(ShowsBar(Preferences, Creature), TEXT("hurt after a fight")));

			for (const EVeyraUnitKind Kind : { EVeyraUnitKind::Vanguard, EVeyraUnitKind::Structure, EVeyraUnitKind::Companion, EVeyraUnitKind::Echo })
			{
				FVeyraBarFacts Other;
				Other.Kind = Kind;
				ASSERT_THAT(IsTrue(ShowsBar(Preferences, Other), TEXT("no setting hides a Vanguard's or a structure's bar")));
			}
		}
		TEST_METHOD(ColorVisionGivesEverySideItsColours)
		{
			const UVeyraGreyboxSettings& Hud = HudSettings();
			FVeyraSettingsStore Store(Registry);
			const FVeyraSideColors Standard = Resolve(Hud, &Store).SideColors;
			ASSERT_THAT(IsTrue(Standard.Own.Equals(Hud.OwnColor) && Standard.Ally.Equals(Hud.AllyColor) && Standard.Enemy.Equals(Hud.EnemyColor)
				&& Standard.Neutral.Equals(Hud.NeutralColor), TEXT("Standard by default")));
			// A preset gives all four (SET-8).
			Store.Set(ColorVision(), TEXT("Deuteranopia"));
			const FVeyraSideColors Preset = Resolve(Hud, &Store).SideColors;
			const FVeyraSideColorSet& Expected = Hud.ColorVisionPresets.FindChecked(TEXT("Deuteranopia"));
			ASSERT_THAT(IsTrue(Preset.Ally.Equals(Expected.Ally) && Preset.Enemy.Equals(Expected.Enemy) && Preset.Own.Equals(Expected.Own) && Preset.Neutral.Equals(Expected.Neutral)));
			// Custom: each side its named colour, the player a lighter shade of their allies'.
			Store.Set(ColorVision(), TEXT("Custom"));
			Store.Set(AllyColor(), TEXT("Teal"));
			Store.Set(EnemyColor(), TEXT("Orange"));
			Store.Set(NeutralColor(), TEXT("White"));
			const FVeyraSideColors Custom = Resolve(Hud, &Store).SideColors;
			ASSERT_THAT(IsTrue(Custom.Ally.Equals(Hud.SideColorPalette.FindChecked(TEXT("Teal"))) && Custom.Enemy.Equals(Hud.SideColorPalette.FindChecked(TEXT("Orange")))
				&& Custom.Neutral.Equals(Hud.SideColorPalette.FindChecked(TEXT("White")))));
			FLinearColor Lighter = FMath::Lerp(Custom.Ally, FLinearColor::White, Hud.CustomOwnLightening);
			Lighter.A = Custom.Ally.A;
			ASSERT_THAT(IsTrue(Custom.Own.Equals(Lighter), TEXT("own follows its allies' family")));
			// Anything it does not know is Standard.
			const FVeyraSideColors Unknown = SideColorsFor(Hud, TEXT("NoSuchPalette"), TEXT("Teal"), TEXT("Orange"), TEXT("White"));
			ASSERT_THAT(IsTrue(Unknown.Ally.Equals(Hud.AllyColor) && Unknown.Enemy.Equals(Hud.EnemyColor)));
		}

		TEST_METHOD(TheColourSettingsPreviewTheColoursThePlayerWouldSee)
		{
			const UVeyraGreyboxSettings& Hud = HudSettings();
			FVeyraSettingsStore Store(Registry);
			Store.Set(ColorVision(), TEXT("Tritanopia"));
			Store.Set(EnemyColor(), TEXT("Magenta"));
			const FVeyraSettingsModel Model = VeyraSettingsModels::Describe(Store, EVeyraSettingCategory::Accessibility, FString(), /*bInLiveMatch*/ false);
			const auto RowOf = [&Model](const FVeyraContentId& Id) { return Model.Rows.FindByPredicate([&Id](const FVeyraSettingRowModel& Row) { return Row.Id == Id; }); };
			// Color Vision previews all four sides as they now resolve (SET-8).
			const FVeyraSettingRowModel* Vision = RowOf(ColorVision());
			ASSERT_THAT(IsNotNull(Vision));
			const FVeyraSideColorSet& Preset = Hud.ColorVisionPresets.FindChecked(TEXT("Tritanopia"));
			ASSERT_THAT(IsTrue(Vision->Swatches.Num() == 4 && Vision->Swatches[0].Equals(Preset.Own) && Vision->Swatches[1].Equals(Preset.Ally)
				&& Vision->Swatches[2].Equals(Preset.Enemy) && Vision->Swatches[3].Equals(Preset.Neutral), TEXT("own, ally, enemy and neutral")));
			// A custom side colour previews the colour it holds.
			const FVeyraSettingRowModel* Enemy = RowOf(EnemyColor());
			ASSERT_THAT(IsTrue(Enemy && Enemy->Swatches.Num() == 1 && Enemy->Swatches[0].Equals(Hud.SideColorPalette.FindChecked(TEXT("Magenta")))));
			const FVeyraSettingRowModel* Flashing = RowOf(ReduceFlashing());
			ASSERT_THAT(IsTrue(Flashing && Flashing->Swatches.IsEmpty(), TEXT("other settings preview nothing")));
		}

		TEST_METHOD(EveryColourOptionHasItsColoursAndEachPaletteKeepsTheSidesApart)
		{
			const UVeyraGreyboxSettings& Hud = HudSettings();
			const TOptional<FVeyraSettingInfo> Vision = VeyraSettings::Find(Registry, ColorVision());
			ASSERT_THAT(IsTrue(Vision.IsSet() && Vision->Choice));
			for (const FString& Option : Vision->Choice->Options)
			{
				ASSERT_THAT(IsTrue(Option == TEXT("Standard") || Option == TEXT("Custom") || Hud.ColorVisionPresets.Contains(Option), Option));
			}
			for (const FVeyraContentId& Side : { AllyColor(), EnemyColor(), NeutralColor() })
			{
				const TOptional<FVeyraSettingInfo> Setting = VeyraSettings::Find(Registry, Side);
				ASSERT_THAT(IsTrue(Setting.IsSet() && Setting->Choice));
				for (const FString& Option : Setting->Choice->Options)
				{
					ASSERT_THAT(IsTrue(Hud.SideColorPalette.Contains(Option), Option));
				}
			}
			for (const TPair<FString, FVeyraSideColorSet>& Preset : Hud.ColorVisionPresets)
			{
				const FLinearColor Sides[] = { Preset.Value.Own, Preset.Value.Ally, Preset.Value.Enemy, Preset.Value.Neutral };
				for (int32 First = 0; First < UE_ARRAY_COUNT(Sides); ++First)
				{
					for (int32 Second = First + 1; Second < UE_ARRAY_COUNT(Sides); ++Second)
					{
						ASSERT_THAT(IsFalse(Sides[First].Equals(Sides[Second]), Preset.Key));
					}
				}
			}
		}

		TEST_METHOD(TheAccessibilityOptionsAndWarningsResolve)
		{
			FVeyraSettingsStore Store(Registry);
			const FVeyraInterfacePreferences Defaults = Resolve(HudSettings(), &Store);
			ASSERT_THAT(IsTrue(Defaults.bConnectionWarning && Defaults.bPerformanceWarning, TEXT("both warnings On by default (SET-21, SET-110)")));
			ASSERT_THAT(IsTrue(!Defaults.bEnhancedFocus && !Defaults.bReduceTransparency && !Defaults.bReduceUiAnimation && !Defaults.bReduceFlashing
				&& Defaults.TextSize == TEXT("Standard")));
			Store.Set(ConnectionWarning(), VeyraSettings::Off());
			Store.Set(PerformanceWarning(), VeyraSettings::Off());
			Store.Set(FocusIndicator(), TEXT("Enhanced"));
			Store.Set(ReduceTransparency(), VeyraSettings::On());
			Store.Set(ReduceUiAnimation(), VeyraSettings::On());
			Store.Set(ReduceFlashing(), VeyraSettings::On());
			Store.Set(TextSize(), TEXT("ExtraLarge"));
			const FVeyraInterfacePreferences Changed = Resolve(HudSettings(), &Store);
			ASSERT_THAT(IsTrue(!Changed.bConnectionWarning && !Changed.bPerformanceWarning && Changed.bEnhancedFocus && Changed.bReduceTransparency
				&& Changed.bReduceUiAnimation && Changed.bReduceFlashing && Changed.TextSize == TEXT("ExtraLarge")));
		}

		TEST_METHOD(ScreenShakeKicksFullyAtFullLessAtReducedAndNotAtAllOff)
		{
			// The camera's kick on a heavy hit or a fall (SET-9; ADR-068 §4): Full by default.
			FVeyraSettingsStore Store(Registry);
			ASSERT_THAT(IsNear(Resolve(HudSettings(), &Store).ScreenShakeScale, 1.0f, 1e-6f));
			Store.Set(ScreenShake(), TEXT("Reduced"));
			ASSERT_THAT(IsNear(Resolve(HudSettings(), &Store).ScreenShakeScale, HudSettings().ReducedShakeShare, 1e-6f));
			Store.Set(ScreenShake(), TEXT("Off"));
			ASSERT_THAT(IsNear(Resolve(HudSettings(), &Store).ScreenShakeScale, 0.0f, 1e-6f));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
