// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraDisplaySettings.h"

#include "Client/VeyraClientFlowTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

EVeyraDisplayMode UVeyraDisplaySettings::GetMatchDisplayMode() const
{
	FString Override;
	if (FParse::Value(FCommandLine::Get(), TEXT("VeyraMatchDisplay="), Override))
	{
		if (const TOptional<EVeyraDisplayMode> Mode = VeyraMatchDisplay::ParseDisplayMode(Override))
		{
			return Mode.GetValue();
		}
	}
	return MatchDisplayMode;
}

namespace VeyraMatchDisplay
{
bool TakesTheScreen(EVeyraClientState State)
{
	switch (State)
	{
	case EVeyraClientState::MatchStarting:
	case EVeyraClientState::Connecting:
	case EVeyraClientState::InMatch:
		return true;
	case EVeyraClientState::SigningIn:
	case EVeyraClientState::SignInFailed:
	case EVeyraClientState::Loading:
	case EVeyraClientState::StarterChoice:
	case EVeyraClientState::Shell:
	case EVeyraClientState::Lobby:
	case EVeyraClientState::MatchFound:
	case EVeyraClientState::Selecting:
	case EVeyraClientState::Returning:
	case EVeyraClientState::AwaitingResults:
	case EVeyraClientState::Results:
	case EVeyraClientState::ReconnectOnly:
	case EVeyraClientState::SessionEnded:
		return false;
	}
	return false;
}

TOptional<EVeyraDisplayMode> ParseDisplayMode(const FString& Text)
{
	const TPair<const TCHAR*, EVeyraDisplayMode> Names[] = {
		{ TEXT("Windowed"), EVeyraDisplayMode::Windowed },
		{ TEXT("BorderlessFullscreen"), EVeyraDisplayMode::BorderlessFullscreen },
		{ TEXT("Fullscreen"), EVeyraDisplayMode::Fullscreen },
	};
	for (const TPair<const TCHAR*, EVeyraDisplayMode>& Name : Names)
	{
		if (Text.Equals(Name.Key, ESearchCase::IgnoreCase))
		{
			return Name.Value;
		}
	}
	return {};
}

FIntPoint MonitorSizeAt(TConstArrayView<FMonitorInfo> Monitors, const FVector2D& Point, const FIntPoint& Fallback)
{
	for (const FMonitorInfo& Monitor : Monitors)
	{
		const FPlatformRect& Rect = Monitor.DisplayRect;
		if (Point.X >= Rect.Left && Point.X < Rect.Right && Point.Y >= Rect.Top && Point.Y < Rect.Bottom)
		{
			return FIntPoint(Rect.Right - Rect.Left, Rect.Bottom - Rect.Top);
		}
	}
	return Fallback;
}

EWindowMode::Type ToWindowMode(EVeyraDisplayMode Mode)
{
	switch (Mode)
	{
	case EVeyraDisplayMode::Windowed:
		return EWindowMode::Windowed;
	case EVeyraDisplayMode::BorderlessFullscreen:
		return EWindowMode::WindowedFullscreen;
	case EVeyraDisplayMode::Fullscreen:
		return EWindowMode::Fullscreen;
	}
	return EWindowMode::Windowed;
}
}
