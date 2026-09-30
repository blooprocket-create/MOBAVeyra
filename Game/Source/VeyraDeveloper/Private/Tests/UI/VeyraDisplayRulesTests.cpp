// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Settings/VeyraDisplayRules.h"
#include "Shell/VeyraDisplaySettings.h"
#include "VeyraSettingsStore.h"
#include "VeyraSettingsSubsystem.h"

namespace VeyraDisplayRulesTests
{
	using namespace VeyraDisplayRules;

	FVeyraSettingsRegistry LoadedRegistry()
	{
		FVeyraSettingsRegistry Registry;
		UVeyraSettingsSubsystem::LoadRegistry(Registry);
		return Registry;
	}

	// Veyra.UI.SettingsDisplay.*: the Graphics & Display settings' rules (ADR-024 §6; Settings Bible §8, SET-92, SET-109).
	TEST_CLASS(SettingsDisplay, "Veyra.UI")
	{
		const FVeyraSettingsRegistry Registry = LoadedRegistry();

		TEST_METHOD(TheDefaultsAreWhatTheEngineShows)
		{
			const FVeyraSettingsStore Store(Registry);
			const FVeyraDisplayState Front = Resolve(Store, /*bForeground*/ true);
			ASSERT_THAT(IsTrue(Front.FrameCap == 0.0f && !Front.bVSync && Front.RenderScale == 100.0f, TEXT("uncapped in front, no VSync, the full resolution")));
			ASSERT_THAT(IsTrue(Front.TextureQuality == 2 && Front.ShadowQuality == 2 && Front.EffectsQuality == 2, TEXT("High is the engine's level 2")));
			ASSERT_THAT(IsTrue(Front.WindowSize == FIntPoint(1280, 720) && Front.MatchMode == EVeyraDisplayMode::BorderlessFullscreen));
			ASSERT_THAT(IsTrue(Resolve(Store, /*bForeground*/ false).FrameCap == 30.0f, TEXT("30 behind other windows (SET-109)")));
		}

		TEST_METHOD(SizesParseOnlyWhenWhole)
		{
			ASSERT_THAT(IsTrue(ParseSize(TEXT("1600x900")).Get(FIntPoint::ZeroValue) == FIntPoint(1600, 900)));
			ASSERT_THAT(IsFalse(ParseSize(TEXT("1600")).IsSet()));
			ASSERT_THAT(IsFalse(ParseSize(TEXT("0x900")).IsSet()));
			ASSERT_THAT(IsFalse(ParseSize(TEXT("wide x tall")).IsSet()));
		}

		TEST_METHOD(APresetSetsItsGroupsAndAGroupAloneMakesItCustom)
		{
			FVeyraSettingsStore Store(Registry);
			Store.Set(Quality(), TEXT("Low"));
			FollowQualityPreset(Store, Quality());
			ASSERT_THAT(IsTrue(Store.Get(TextureQuality()) == TEXT("Low") && Store.Get(ShadowQuality()) == TEXT("Low") && Store.Get(EffectsQuality()) == TEXT("Low")));
			ASSERT_THAT(IsTrue(Resolve(Store, true).ShadowQuality == 0));

			Store.Set(ShadowQuality(), TEXT("High"));
			FollowQualityPreset(Store, ShadowQuality());
			ASSERT_THAT(AreEqual(FString(TEXT("Custom")), Store.Get(Quality()), TEXT("a group alone makes it Custom")));
			Store.Set(TextureQuality(), TEXT("High"));
			FollowQualityPreset(Store, TextureQuality());
			Store.Set(EffectsQuality(), TEXT("High"));
			FollowQualityPreset(Store, EffectsQuality());
			ASSERT_THAT(AreEqual(FString(TEXT("High")), Store.Get(Quality()), TEXT("every group at one level names that preset")));

			// The groups follow without taking the Undo step: Undo takes back the player's own change.
			Store.Set(Quality(), TEXT("Medium"));
			FollowQualityPreset(Store, Quality());
			ASSERT_THAT(IsTrue(Store.Undo() && Store.Get(Quality()) == TEXT("High")));
			FollowQualityPreset(Store, Quality());
			ASSERT_THAT(IsTrue(Store.Get(TextureQuality()) == TEXT("High")));

			Store.Set(Quality(), TEXT("Custom"));
			FollowQualityPreset(Store, Quality());
			ASSERT_THAT(IsTrue(Store.Get(ShadowQuality()) == TEXT("High"), TEXT("Custom leaves each group as it is")));
		}

		TEST_METHOD(ADisruptiveChangeRevertsUnlessKept)
		{
			FVeyraSettingsStore Store(Registry);
			FVeyraDisplayConfirmation Confirmation;
			Store.Set(WindowSize(), TEXT("1600x900"));
			Confirmation.Await(WindowSize(), TEXT("1280x720"), /*Deadline*/ 15.0);
			ASSERT_THAT(IsFalse(Confirmation.Tick(14.9, Store, false)));
			ASSERT_THAT(IsTrue(Confirmation.IsPending() && FMath::IsNearlyEqual(Confirmation.SecondsLeft(10.0), 5.0)));
			// A second change within the countdown still goes back to the one that worked.
			Store.Set(WindowSize(), TEXT("1024x576"));
			Confirmation.Await(WindowSize(), TEXT("1600x900"), /*Deadline*/ 25.0);
			ASSERT_THAT(IsTrue(Confirmation.Tick(25.0, Store, false) && Store.Get(WindowSize()) == TEXT("1280x720") && !Confirmation.IsPending()));

			Store.Set(WindowSize(), TEXT("1600x900"));
			Confirmation.Await(WindowSize(), TEXT("1280x720"), 40.0);
			Confirmation.Keep();
			ASSERT_THAT(IsFalse(Confirmation.Tick(60.0, Store, false)));
			ASSERT_THAT(AreEqual(FString(TEXT("1600x900")), Store.Get(WindowSize()), TEXT("kept")));
		}

		TEST_METHOD(TheMatchTakesThePlayersDisplayMode)
		{
			FVeyraSettingsStore Store(Registry);
			const UVeyraDisplaySettings& Display = *GetDefault<UVeyraDisplaySettings>();
			ASSERT_THAT(IsTrue(Display.GetMatchDisplayMode(nullptr) == Display.MatchDisplayMode, TEXT("without settings, the developer default")));
			Store.Set(MatchMode(), TEXT("Windowed"));
			ASSERT_THAT(IsTrue(Display.GetMatchDisplayMode(&Store) == EVeyraDisplayMode::Windowed));
			ASSERT_THAT(IsTrue(Display.KeepChangesSeconds > 0.0f, TEXT("SET-92's countdown is data")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
