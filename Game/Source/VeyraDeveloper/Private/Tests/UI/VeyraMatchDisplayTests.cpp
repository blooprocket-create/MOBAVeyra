// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Client/VeyraClientFlowTypes.h"
#include "Shell/VeyraDisplaySettings.h"

namespace VeyraMatchDisplayTests
{
	// Veyra.UI.MatchDisplay.*: a match takes the screen from the loading after champion select to its
	// end, and the client's window comes back for the results, as League's client and game do.
	TEST_CLASS(MatchDisplay, "Veyra.UI")
	{
		TEST_METHOD(AMatchHasTheScreenFromItsLoadingToItsEnd)
		{
			for (const EVeyraClientState State : { EVeyraClientState::MatchStarting, EVeyraClientState::Connecting, EVeyraClientState::InMatch })
			{
				ASSERT_THAT(IsTrue(VeyraMatchDisplay::TakesTheScreen(State), LexToString(State)));
			}
			// Champion select and the results are the client's; so is everything before and after.
			for (const EVeyraClientState State : { EVeyraClientState::Shell, EVeyraClientState::MatchFound, EVeyraClientState::Selecting,
					 EVeyraClientState::Returning, EVeyraClientState::AwaitingResults, EVeyraClientState::Results, EVeyraClientState::ReconnectOnly })
			{
				ASSERT_THAT(IsFalse(VeyraMatchDisplay::TakesTheScreen(State), LexToString(State)));
			}
		}

		TEST_METHOD(TheModesAreTheDisplayModeSettingsChoices)
		{
			// Settings & Accessibility Bible 166: Windowed, Borderless Fullscreen, Fullscreen.
			ASSERT_THAT(IsTrue(VeyraMatchDisplay::ParseDisplayMode(TEXT("BorderlessFullscreen")) == EVeyraDisplayMode::BorderlessFullscreen));
			ASSERT_THAT(IsTrue(VeyraMatchDisplay::ParseDisplayMode(TEXT("windowed")) == EVeyraDisplayMode::Windowed));
			ASSERT_THAT(IsTrue(VeyraMatchDisplay::ParseDisplayMode(TEXT("Fullscreen")) == EVeyraDisplayMode::Fullscreen));
			ASSERT_THAT(IsFalse(VeyraMatchDisplay::ParseDisplayMode(TEXT("Maximised")).IsSet()));
			ASSERT_THAT(IsTrue(VeyraMatchDisplay::ToWindowMode(EVeyraDisplayMode::BorderlessFullscreen) == EWindowMode::WindowedFullscreen));
			ASSERT_THAT(IsTrue(VeyraMatchDisplay::ToWindowMode(EVeyraDisplayMode::Fullscreen) == EWindowMode::Fullscreen));
			ASSERT_THAT(IsTrue(VeyraMatchDisplay::ToWindowMode(EVeyraDisplayMode::Windowed) == EWindowMode::Windowed));
			// A match takes the whole screen unless the player chooses otherwise.
			ASSERT_THAT(IsTrue(GetDefault<UVeyraDisplaySettings>()->MatchDisplayMode == EVeyraDisplayMode::BorderlessFullscreen));
		}

		TEST_METHOD(AMatchFillsTheMonitorItsWindowIsOn)
		{
			// A primary 1440p monitor, and a 1080p one to its right (PR #31 review).
			FMonitorInfo Primary;
			Primary.DisplayRect = FPlatformRect(0, 0, 2560, 1440);
			Primary.bIsPrimary = true;
			FMonitorInfo Secondary;
			Secondary.DisplayRect = FPlatformRect(2560, 0, 4480, 1080);
			const FMonitorInfo Monitors[] = { Primary, Secondary };
			const FIntPoint Fallback(1, 1);
			ASSERT_THAT(IsTrue(VeyraMatchDisplay::MonitorSizeAt(Monitors, FVector2D(3200.0, 500.0), Fallback) == FIntPoint(1920, 1080)));
			ASSERT_THAT(IsTrue(VeyraMatchDisplay::MonitorSizeAt(Monitors, FVector2D(100.0, 100.0), Fallback) == FIntPoint(2560, 1440)));
			ASSERT_THAT(IsTrue(VeyraMatchDisplay::MonitorSizeAt(Monitors, FVector2D(-500.0, 0.0), Fallback) == Fallback, TEXT("off every monitor")));
		}
	};
}

#endif
