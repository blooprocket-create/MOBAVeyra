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
		const FVeyraSettingsRegistry Registry = InterfaceRegistry();
		const UVeyraGreyboxSettings& Hud = *GetDefault<UVeyraGreyboxSettings>();

		TEST_METHOD(TheDefaultsAreTheDevelopersInterface)
		{
			const FVeyraInterfacePreferences Plain = Resolve(Hud, nullptr);
			const FVeyraSettingsStore Defaults(Registry);
			const FVeyraInterfacePreferences Untouched = Resolve(Hud, &Defaults);
			for (const FVeyraInterfacePreferences* Preferences : { &Plain, &Untouched })
			{
				ASSERT_THAT(IsTrue(Preferences->HudScale == 1.0f && Preferences->MinimapSize == Hud.MinimapSize && Preferences->MinimapUnitIcon == Hud.MinimapUnitIcon));
				ASSERT_THAT(IsTrue(Preferences->PingSeconds == Hud.PingSeconds && Preferences->bMinimapClickMovesCamera && Preferences->bMinimapRightClickMoves));
				ASSERT_THAT(IsTrue(!Preferences->bShowFps && !Preferences->bShowPing && !Preferences->bScoreboardToggles && Preferences->bConfineCursor));
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
			const FVeyraInterfacePreferences Preferences = Resolve(Hud, &Store);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Preferences.HudScale, 1.2f) && FMath::IsNearlyEqual(Preferences.MinimapSize, Hud.MinimapSize * 1.5f)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Preferences.MinimapVanguardIcon, Hud.MinimapVanguardIcon * 0.5f) && Preferences.PingSeconds == 6.0f));
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
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
