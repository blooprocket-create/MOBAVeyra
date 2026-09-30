// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "Camera/VeyraCameraPreferences.h"
#include "Input/VeyraCameraSettings.h"
#include "VeyraSettingsStore.h"
#include "VeyraSettingsSubsystem.h"

namespace VeyraCameraPreferencesTests
{
	using namespace VeyraCameraPreferences;

	FVeyraSettingsRegistry CameraRegistry()
	{
		FVeyraSettingsRegistry Registry;
		UVeyraSettingsSubsystem::LoadRegistry(Registry);
		return Registry;
	}

	// Veyra.Match.CameraPreferences.*: the player's camera settings over the developer's (ADR-024 §6, §9; Settings Bible §2, §12.3).
	TEST_CLASS(CameraPreferences, "Veyra.Match")
	{
		/** Loaded before each test: CQTest builds its classes while registering them, which in a packaged client is before the engine starts. */
		FVeyraSettingsRegistry Registry;

		BEFORE_EACH()
		{
			Registry = CameraRegistry();
		}
		static const UVeyraCameraSettings& ViewSettings() { return *GetDefault<UVeyraCameraSettings>(); }

		TEST_METHOD(ASpeedSliderScalesTheDevelopersSpeedEvenlyInRatio)
		{
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(SpeedScale(50.0, 0.0, 50.0, 100.0, 0.5, 2.0), 1.0), TEXT("the default is the developer's speed")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(SpeedScale(100.0, 0.0, 50.0, 100.0, 0.5, 2.0), 2.0)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(SpeedScale(0.0, 0.0, 50.0, 100.0, 0.5, 2.0), 0.5)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(SpeedScale(75.0, 0.0, 50.0, 100.0, 0.5, 2.0), FMath::Sqrt(2.0)), TEXT("halfway up is halfway in ratio")));
			ASSERT_THAT(IsTrue(ViewSettings().SpeedSettingSlowest > 0.0f && ViewSettings().SpeedSettingSlowest <= 1.0f && ViewSettings().SpeedSettingFastest >= 1.0f));
		}

		TEST_METHOD(WithoutSettingsTheCameraIsTheDevelopers)
		{
			const FVeyraCameraPreferences Plain = Resolve(ViewSettings(), nullptr);
			ASSERT_THAT(IsTrue(Plain.PanSpeed == ViewSettings().PanSpeed && Plain.EdgeScrollPixels == ViewSettings().EdgeScrollPixels && Plain.DefaultMode == ViewSettings().DefaultMode));
			ASSERT_THAT(IsTrue(Plain.EdgeDelaySeconds == 0.0 && Plain.bReturnOnRespawn && Plain.bFreeWhileDead));

			const FVeyraSettingsStore Defaults(Registry);
			const FVeyraCameraPreferences Untouched = Resolve(ViewSettings(), &Defaults);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Untouched.PanSpeed, static_cast<double>(ViewSettings().PanSpeed)) && Untouched.EdgeScrollPixels == ViewSettings().EdgeScrollPixels,
				TEXT("the settings' defaults are the developer's camera")));
		}

		TEST_METHOD(ThePlayersSettingsTakeTheirPlace)
		{
			FVeyraSettingsStore Store(Registry);
			Store.Set(MoveSpeed(), TEXT("100"));
			Store.Set(DragSensitivity(), TEXT("0"));
			Store.Set(EdgeZone(), TEXT("Wide"));
			Store.Set(EdgeDelay(), TEXT("Long"));
			Store.Set(DefaultMode(), TEXT("SemiLocked"));
			Store.Set(ReturnOnRespawn(), VeyraSettings::Off());
			Store.Set(FreeWhileDead(), VeyraSettings::Off());
			Store.Set(EdgeScroll(), VeyraSettings::Off());
			const FVeyraCameraPreferences Preferences = Resolve(ViewSettings(), &Store);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Preferences.PanSpeed, ViewSettings().PanSpeed * static_cast<double>(ViewSettings().SpeedSettingFastest))));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Preferences.DragUnitsPerPixel, ViewSettings().DragUnitsPerPixel * static_cast<double>(ViewSettings().SpeedSettingSlowest))));
			ASSERT_THAT(IsTrue(Preferences.EdgeScrollPixels == ViewSettings().EdgeZonePixels.FindRef(TEXT("Wide")) && Preferences.EdgeDelaySeconds == ViewSettings().EdgeDelaySeconds.FindRef(TEXT("Long"))));
			ASSERT_THAT(IsTrue(Preferences.DefaultMode == EVeyraCameraMode::SemiLocked && !Preferences.bReturnOnRespawn && !Preferences.bFreeWhileDead && !Preferences.bEdgeScroll));
		}

		TEST_METHOD(EveryOptionHasItsMeasureAndEveryModeItsName)
		{
			const TOptional<FVeyraSettingInfo> Zone = VeyraSettings::Find(Registry, EdgeZone());
			const TOptional<FVeyraSettingInfo> Delay = VeyraSettings::Find(Registry, EdgeDelay());
			const TOptional<FVeyraSettingInfo> Mode = VeyraSettings::Find(Registry, DefaultMode());
			ASSERT_THAT(IsTrue(Zone.IsSet() && Zone->Choice && Delay.IsSet() && Delay->Choice && Mode.IsSet() && Mode->Choice));
			for (const FString& Option : Zone->Choice->Options)
			{
				ASSERT_THAT(IsTrue(ViewSettings().EdgeZonePixels.Contains(Option), FString::Printf(TEXT("EdgeZonePixels lacks %s"), *Option)));
			}
			for (const FString& Option : Delay->Choice->Options)
			{
				ASSERT_THAT(IsTrue(ViewSettings().EdgeDelaySeconds.Contains(Option), FString::Printf(TEXT("EdgeDelaySeconds lacks %s"), *Option)));
			}
			for (const FString& Option : Mode->Choice->Options)
			{
				const TOptional<EVeyraCameraMode> Parsed = ParseMode(Option);
				ASSERT_THAT(IsTrue(Parsed.IsSet() && ModeName(Parsed.GetValue()) == Option, FString::Printf(TEXT("no camera mode is %s"), *Option)));
			}
			ASSERT_THAT(IsFalse(ParseMode(TEXT("Orbit")).IsSet()));
		}

		TEST_METHOD(TheEdgesWaitForTheCursorToRest)
		{
			double Held = 0.0;
			const FVector2D Left(-1.0, 0.0);
			ASSERT_THAT(IsTrue(VeyraCamera::DelayEdgePan(Left, 0.1, 0.3, Held).IsZero()));
			ASSERT_THAT(IsTrue(VeyraCamera::DelayEdgePan(Left, 0.1, 0.3, Held).IsZero()));
			ASSERT_THAT(IsTrue(VeyraCamera::DelayEdgePan(Left, 0.15, 0.3, Held) == Left, TEXT("once it has rested long enough")));
			ASSERT_THAT(IsTrue(VeyraCamera::DelayEdgePan(FVector2D::ZeroVector, 0.1, 0.3, Held).IsZero() && Held == 0.0, TEXT("leaving the zone cancels (SET-87)")));
			ASSERT_THAT(IsTrue(VeyraCamera::DelayEdgePan(Left, 0.016, 0.0, Held) == Left, TEXT("Immediate pans at once")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
