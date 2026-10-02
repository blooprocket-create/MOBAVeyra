// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraShellFocusSubsystem.h"

#include "Framework/Application/SlateApplication.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Settings/VeyraInterfacePreferences.h"
#include "Shell/VeyraShellButton.h"

void UVeyraShellFocusSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Not in a server: nobody there looks at a menu.
	if (!IsRunningDedicatedServer())
	{
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVeyraShellFocusSubsystem::Tick));
	}
}

void UVeyraShellFocusSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	Light.Follow(nullptr, false);
	Super::Deinitialize();
}

void FVeyraFocusLight::Follow(UVeyraShellButton* Focused, bool bEnhanced)
{
	UVeyraShellButton* Now = bEnhanced ? Focused : nullptr;
	if (Now == Lit.Get())
	{
		return;
	}
	if (UVeyraShellButton* Before = Lit.Get())
	{
		Before->ShowEnhancedFocus(false);
	}
	Lit = Now;
	if (Now)
	{
		Now->ShowEnhancedFocus(true);
	}
}

bool UVeyraShellFocusSubsystem::Tick(float /*DeltaSeconds*/)
{
	if (!FSlateApplication::IsInitialized())
	{
		return true;
	}
	const bool bEnhanced = VeyraInterfacePreferences::Resolve(*GetDefault<UVeyraGreyboxSettings>(), VeyraInterfacePreferences::StoreOf(GetGameInstance())).bEnhancedFocus;
	Light.Follow(bEnhanced ? UVeyraShellButton::FindBySlate(FSlateApplication::Get().GetKeyboardFocusedWidget()) : nullptr, bEnhanced);
	return true;
}