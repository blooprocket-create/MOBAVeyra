// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Greybox/VeyraGreyboxSettings.h"
#include "Settings/VeyraInterfacePreferences.h"
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
			const FVeyraInterfacePreferences Preferences = Resolve(HudSettings(), &Store);
			ASSERT_THAT(IsTrue(Preferences.IndicatorThickness == HudSettings().IndicatorThickThickness && HudSettings().IndicatorThickThickness > HudSettings().IndicatorThickness));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Preferences.HudScale, 1.2f) && FMath::IsNearlyEqual(Preferences.MinimapSize, HudSettings().MinimapSize * 1.5f)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Preferences.MinimapVanguardIcon, HudSettings().MinimapVanguardIcon * 0.5f) && Preferences.PingSeconds == 6.0f));
			ASSERT_THAT(IsTrue(!Preferences.bMinimapRightClickMoves && Preferences.bScoreboardToggles && !Preferences.bConfineCursor && Preferences.bShowFps));
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
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
