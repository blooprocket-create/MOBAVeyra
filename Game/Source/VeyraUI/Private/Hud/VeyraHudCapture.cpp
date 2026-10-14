// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Hud/VeyraHudCapture.h"

namespace VeyraHudCapture
{
	namespace
	{
		FString& Hover()
		{
			static FString Tile;
			return Tile;
		}

		bool& StatsOpen()
		{
			static bool bOpen = false;
			return bOpen;
		}
	}

	const FString& PinnedHover()
	{
		return Hover();
	}

	void PinHover(const FString& Tile)
	{
		Hover() = Tile;
	}

	bool IsStatsPanelHeldOpen()
	{
		return StatsOpen();
	}

	void HoldStatsPanelOpen(bool bOpen)
	{
		StatsOpen() = bOpen;
	}
}
