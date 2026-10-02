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

		// ADR-052 §3: one game-wide range around the standard zoom; the level persists and resets to the standard zoom.
		TEST_METHOD(TheZoomLevelSpansTheGameWideRangeAroundTheStandardZoom)
		{
			const UVeyraCameraSettings& View = ViewSettings();
			ASSERT_THAT(IsTrue(View.MinDistance > 0.0f && View.MinDistance <= View.Distance && View.Distance <= View.MaxDistance, TEXT("the standard zoom lies within the range")));
			const FVeyraSettingsStore Defaults(Registry);
			const TOptional<FVeyraZoomScale> Scale = ZoomScaleOf(View, Defaults);
			ASSERT_THAT(IsTrue(Scale.IsSet()));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Resolve(View, &Defaults).Distance, static_cast<double>(View.Distance)), TEXT("the default level is the standard zoom")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Resolve(View, nullptr).Distance, static_cast<double>(View.Distance)), TEXT("and so is no setting at all")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ZoomDistance(*Scale, Scale->Lowest), static_cast<double>(View.MinDistance))
				&& FMath::IsNearlyEqual(ZoomDistance(*Scale, Scale->Highest), static_cast<double>(View.MaxDistance))));
			for (const double Distance : { Scale->Nearest, (Scale->Nearest + Scale->Standard) / 2.0, Scale->Standard, (Scale->Standard + Scale->Farthest) / 2.0, Scale->Farthest })
			{
				ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ZoomDistance(*Scale, ZoomLevel(*Scale, Distance)), Distance), TEXT("ZoomLevel turns ZoomDistance round")));
			}
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ZoomDistance(*Scale, Scale->Highest + 50.0), Scale->Farthest) && FMath::IsNearlyEqual(ZoomDistance(*Scale, Scale->Lowest - 50.0), Scale->Nearest),
				TEXT("never beyond the range")));
		}

		TEST_METHOD(TheZoomPersistsAsALevelAndResetsToTheStandardZoom)
		{
			FVeyraSettingsStore Store(Registry);
			ASSERT_THAT(IsTrue(Store.Set(Zoom(), TEXT("0"), /*bInLiveMatch*/ true) == EVeyraSettingChange::Changed, TEXT("the zoom changes in a live match")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Resolve(ViewSettings(), &Store).Distance, static_cast<double>(ViewSettings().MinDistance))));
			Store.Reset(Zoom());
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Resolve(ViewSettings(), &Store).Distance, static_cast<double>(ViewSettings().Distance)), TEXT("its reset is the standard zoom")));
		}

		TEST_METHOD(TheZoomKeysStepWithinTheRangeAndTheArmEasesThere)
		{
			ASSERT_THAT(IsTrue(VeyraCamera::Zoom(1600.0, 2, 100.0, 1200.0, 2200.0) == 1400.0, TEXT("zooming in shortens the arm")));
			ASSERT_THAT(IsTrue(VeyraCamera::Zoom(1600.0, -3, 100.0, 1200.0, 2200.0) == 1900.0));
			ASSERT_THAT(IsTrue(VeyraCamera::Zoom(1250.0, 5, 100.0, 1200.0, 2200.0) == 1200.0 && VeyraCamera::Zoom(2150.0, -5, 100.0, 1200.0, 2200.0) == 2200.0, TEXT("never past the range")));

			FVeyraZoomScale Scale;
			Scale.Lowest = 0.0;
			Scale.Default = 50.0;
			Scale.Highest = 100.0;
			Scale.Nearest = 1200.0;
			Scale.Standard = 1600.0;
			Scale.Farthest = 2200.0;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ZoomLevelAfter(Scale, 50.0, 2, 100.0), 25.0), TEXT("two presses in from the standard zoom")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ZoomLevelAfter(Scale, 50.0, -3, 100.0), 75.0), TEXT("three presses out")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ZoomLevelAfter(Scale, 0.0, 1, 100.0), 0.0), TEXT("no nearer than the nearest")));

			const double Eased = VeyraCamera::EaseZoom(1600.0, 1200.0, 0.12, 0.12);
			ASSERT_THAT(IsTrue(Eased < 1600.0 && Eased > 1200.0, TEXT("part of the way")));
			// The same time in two frames or in one reaches the same place.
			const double Twice = VeyraCamera::EaseZoom(VeyraCamera::EaseZoom(1600.0, 1200.0, 0.06, 0.12), 1200.0, 0.06, 0.12);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Twice, Eased, 1e-6)));
			ASSERT_THAT(IsTrue(VeyraCamera::EaseZoom(1600.0, 1200.0, 0.016, 0.0) == 1200.0, TEXT("without smoothing it is there at once")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
