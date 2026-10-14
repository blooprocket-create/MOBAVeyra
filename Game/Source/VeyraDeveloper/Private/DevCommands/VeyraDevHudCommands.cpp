// Copyright © 2026 Wayfinder Studios. All rights reserved.

// The developer commands for captures of the match HUD (Smoke.ps1 -ScreenshotCommands; ADR-080 §6): a deck tile's
// tooltip shown as if the cursor rested on it, and the stats panel held open. Presentation only, on the typing machine,
// and kept for the whole process, as the commands may run at launch, before any match.

#include "DevCommands/VeyraDevCommands.h"

#include "Misc/OutputDevice.h"

#if WITH_VEYRA_UI
#include "Hud/VeyraHudCapture.h"
#endif

namespace VeyraDevHudCommands
{
	const TCHAR* const Category = TEXT("HUD");

	void RunHudHover(UWorld* World, TConstArrayView<FString> Args, FOutputDevice& Output)
	{
		if (Args.Num() > 1)
		{
			Output.Log(VeyraDevCommands::UsageReply(TEXT("HudHover")));
			return;
		}
#if WITH_VEYRA_UI
		VeyraHudCapture::PinHover(Args.IsEmpty() ? FString() : Args[0]);
		Output.Log(Args.IsEmpty() ? FString(TEXT("The deck's tooltips follow the cursor again.")) : FString::Printf(TEXT("The deck shows %s's tooltip."), *Args[0]));
#else
		Output.Log(TEXT("This machine draws no HUD."));
#endif
	}

	void RunStatsPanelOpen(UWorld* World, TConstArrayView<FString> Args, FOutputDevice& Output)
	{
#if WITH_VEYRA_UI
		const TOptional<bool> bOpen = Args.Num() > 1 ? TOptional<bool>() : VeyraDevCommands::ParseSwitch(Args, VeyraHudCapture::IsStatsPanelHeldOpen());
		if (!bOpen.IsSet())
		{
			Output.Log(VeyraDevCommands::UsageReply(TEXT("StatsPanelOpen")));
			return;
		}
		VeyraHudCapture::HoldStatsPanelOpen(bOpen.GetValue());
		Output.Log(bOpen.GetValue() ? TEXT("The stats panel is held open.") : TEXT("The stats panel opens and closes with a click again."));
#else
		Output.Log(TEXT("This machine draws no HUD."));
#endif
	}
}

void VeyraDevCommands::AddHudCommands(TArray<FVeyraDevCommand>& Out)
{
	using namespace VeyraDevHudCommands;
	Out.Add(FVeyraDevCommand::Local(TEXT("HudHover"), Category, TEXT("[Passive|Q|W|E|R|Item1-Item6]"),
		TEXT("Shows a deck tile's tooltip as if the cursor rested on it; with nothing, the tooltips follow the cursor again."), &RunHudHover));
	Out.Add(FVeyraDevCommand::Local(TEXT("StatsPanelOpen"), Category, TEXT("[on|off]"),
		TEXT("Holds the stats panel beside the portrait open, or lets a click open and close it again."), &RunStatsPanelOpen));
}
