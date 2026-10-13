// Copyright © 2026 Wayfinder Studios. All rights reserved.

// The developer commands for captures of the match HUD (Smoke.ps1 -ScreenshotCommands; ADR-080 §6): a deck tile's
// tooltip shown as if the cursor rested on it, and the stats panel held open. Presentation only, on the typing machine.

#include "DevCommands/VeyraDevCommands.h"

#include "Engine/World.h"
#include "Misc/OutputDevice.h"

#if WITH_VEYRA_UI
#include "Greybox/VeyraGreyboxSubsystem.h"
#endif

namespace VeyraDevHudCommands
{
	const TCHAR* const Category = TEXT("HUD");

#if WITH_VEYRA_UI
	UVeyraGreyboxSubsystem* HudOf(UWorld* World, FOutputDevice& Output)
	{
		UVeyraGreyboxSubsystem* Hud = World ? World->GetSubsystem<UVeyraGreyboxSubsystem>() : nullptr;
		if (!Hud)
		{
			Output.Log(TEXT("This world draws no match HUD."));
		}
		return Hud;
	}
#endif

	void RunHudHover(UWorld* World, TConstArrayView<FString> Args, FOutputDevice& Output)
	{
		if (Args.Num() > 1)
		{
			Output.Log(VeyraDevCommands::UsageReply(TEXT("HudHover")));
			return;
		}
#if WITH_VEYRA_UI
		if (UVeyraGreyboxSubsystem* Hud = HudOf(World, Output))
		{
			Hud->PinHover(Args.IsEmpty() ? FString() : Args[0]);
			Output.Log(Args.IsEmpty() ? FString(TEXT("The deck's tooltips follow the cursor again.")) : FString::Printf(TEXT("The deck shows %s's tooltip."), *Args[0]));
		}
#else
		Output.Log(TEXT("This machine draws no HUD."));
#endif
	}

	void RunStatsPanelOpen(UWorld* World, TConstArrayView<FString> Args, FOutputDevice& Output)
	{
#if WITH_VEYRA_UI
		UVeyraGreyboxSubsystem* Hud = HudOf(World, Output);
		if (!Hud)
		{
			return;
		}
		const TOptional<bool> bOpen = Args.Num() > 1 ? TOptional<bool>() : VeyraDevCommands::ParseSwitch(Args, Hud->IsStatsPanelOpen());
		if (!bOpen.IsSet())
		{
			Output.Log(VeyraDevCommands::UsageReply(TEXT("StatsPanelOpen")));
			return;
		}
		Hud->SetStatsPanelOpen(bOpen.GetValue());
		Output.Log(bOpen.GetValue() ? TEXT("The stats panel is open.") : TEXT("The stats panel is closed."));
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
	Out.Add(FVeyraDevCommand::Local(TEXT("StatsPanelOpen"), Category, TEXT("[on|off]"), TEXT("Opens or closes the stats panel beside the portrait."),
		&RunStatsPanelOpen));
}
