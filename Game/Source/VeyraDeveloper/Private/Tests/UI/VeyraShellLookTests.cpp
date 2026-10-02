// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Components/ActorTestSpawner.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Loading/VeyraLoadingScreen.h"
#include "Settings/VeyraInterfacePreferences.h"
#include "Shell/VeyraShellLook.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "VeyraSettingsStore.h"
#include "VeyraSettingsSubsystem.h"

namespace VeyraShellLookTests
{
	// Veyra.UI.ShellLook.*: the menus in the player's look: Interface Text Size, Reduce Interface Transparency and Reduce
	// Interface Animation (Settings Bible Proposals 62, 65, 75 and 112; ADR-055 §2–§3).
	TEST_CLASS(ShellLook, "Veyra.UI")
	{
		FActorTestSpawner Spawner;
		FVeyraSettingsRegistry Registry;

		BEFORE_EACH()
		{
			UVeyraSettingsSubsystem::LoadRegistry(Registry);
		}

		AFTER_EACH()
		{
			// One look for the whole client: the next test starts from the standard one.
			VeyraShellLook::Use(FVeyraShellLook());
		}

		TEST_METHOD(TheLookFollowsThePlayersSettings)
		{
			FVeyraSettingsStore Store(Registry);
			const UVeyraGreyboxSettings& Hud = *GetDefault<UVeyraGreyboxSettings>();
			const FVeyraShellLook Standard = VeyraShellLook::For(VeyraInterfacePreferences::Resolve(Hud, &Store));
			ASSERT_THAT(IsTrue(Standard == FVeyraShellLook(), TEXT("the standard look by default")));
			const TMap<FString, float>& Scales = GetDefault<UVeyraShellStyleSettings>()->TextSizeScales;
			for (const TCHAR* Size : { TEXT("Large"), TEXT("ExtraLarge") })
			{
				Store.Set(VeyraInterfacePreferences::TextSize(), Size);
				const FVeyraShellLook Larger = VeyraShellLook::For(VeyraInterfacePreferences::Resolve(Hud, &Store));
				ASSERT_THAT(IsTrue(Scales.Contains(Size) && Larger.TextScale == Scales[Size] && Larger.TextScale > 1.0f, Size));
			}
			Store.Set(VeyraInterfacePreferences::ReduceTransparency(), VeyraSettings::On());
			Store.Set(VeyraInterfacePreferences::ReduceUiAnimation(), VeyraSettings::On());
			const FVeyraShellLook Reduced = VeyraShellLook::For(VeyraInterfacePreferences::Resolve(Hud, &Store));
			ASSERT_THAT(IsTrue(Reduced.bOpaquePanels && Reduced.bStillAnimation));
		}

		TEST_METHOD(EnhancedFocusEdgesTextFieldsAndAScopedLookPassesBack)
		{
			// Under Enhanced focus a focused field wears the thick, high-contrast outline a focused button does (SET-74).
			const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
			const FVeyraFocusEdge Plain = VeyraShellLook::FieldFocusEdge();
			ASSERT_THAT(IsTrue(Plain.Width < Style.FocusOutlineWidth && Plain.Color.Equals(Style.AccentColor)));
			FVeyraShellLook Enhanced;
			Enhanced.bEnhancedFocus = true;
			Enhanced.TextScale = 1.3f;
			VeyraShellLook::Use(Enhanced);
			const FVeyraFocusEdge Lit = VeyraShellLook::FieldFocusEdge();
			ASSERT_THAT(IsTrue(Lit.Width == Style.FocusOutlineWidth && Lit.Color.Equals(Style.FocusOutlineColor)));
			// A HUD surface builds in the standard look, and the menus' look comes back after (ADR-055 §2).
			{
				const FVeyraScopedShellLook Hud{ FVeyraShellLook() };
				ASSERT_THAT(IsTrue(VeyraShellLook::Current() == FVeyraShellLook()));
			}
			ASSERT_THAT(IsTrue(VeyraShellLook::Current() == Enhanced));
		}

		TEST_METHOD(TextScalesAndPanelsGoOpaqueInTheLook)
		{
			const FLinearColor Glass(0.1f, 0.2f, 0.3f, 0.8f);
			ASSERT_THAT(AreEqual(20, VeyraShellLook::ScaledFontSize(20)));
			ASSERT_THAT(IsTrue(VeyraShellLook::Panel(Glass).Equals(Glass), TEXT("panels keep their translucency by default")));
			FVeyraShellLook Look;
			Look.TextScale = 1.3f;
			Look.bOpaquePanels = true;
			ASSERT_THAT(IsTrue(VeyraShellLook::Use(Look) && !VeyraShellLook::Use(Look), TEXT("a change, then none")));
			ASSERT_THAT(AreEqual(26, VeyraShellLook::ScaledFontSize(20)));
			ASSERT_THAT(AreEqual(1, VeyraShellLook::ScaledFontSize(0), TEXT("never under 1")));
			const FLinearColor Opaque = VeyraShellLook::Panel(Glass);
			ASSERT_THAT(IsTrue(Opaque.A == 1.0f && Opaque.R == Glass.R && Opaque.G == Glass.G && Opaque.B == Glass.B, TEXT("the same colour, opaque")));
		}

		TEST_METHOD(ReduceInterfaceAnimationStillsTheLoadingIndicator)
		{
			const UVeyraLoadingScreen* Spinning = CreateWidget<UVeyraLoadingScreen>(&Spawner.GetWorld());
			ASSERT_THAT(IsTrue(Spinning && Spinning->IsSpinning()));
			FVeyraShellLook Still;
			Still.bStillAnimation = true;
			VeyraShellLook::Use(Still);
			const UVeyraLoadingScreen* Steady = CreateWidget<UVeyraLoadingScreen>(&Spawner.GetWorld());
			ASSERT_THAT(IsTrue(Steady && !Steady->IsSpinning(), TEXT("words in place of the spinner")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI