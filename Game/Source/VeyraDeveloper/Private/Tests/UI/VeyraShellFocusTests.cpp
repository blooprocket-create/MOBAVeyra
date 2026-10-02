// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ActorTestSpawner.h"
#include "Loading/VeyraLoadingScreen.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellFocusSubsystem.h"
#include "Shell/VeyraShellStyleSettings.h"

namespace VeyraShellFocusTests
{
	// Veyra.UI.ShellFocus.*: the enhanced keyboard focus indicator (Settings Bible Proposal 74; ADR-055 §3).
	TEST_CLASS(ShellFocus, "Veyra.UI")
	{
		FActorTestSpawner Spawner;

		/** The loading screen's Slate tree, held so its widgets outlive the call that builds them. */
		TSharedPtr<SWidget> Slate;

		/** Two buttons, Previous and Next, on a loading screen built for the test. */
		TArray<UVeyraShellButton*> ButtonsOf(UVeyraLoadingScreen& Screen)
		{
			Slate = Screen.TakeWidget();
			return Screen.GetButtons();
		}

		TEST_METHOD(TheFocusedButtonDrawsAThickHighContrastOutlineAndThenItsOwnAgain)
		{
			UVeyraLoadingScreen* Screen = CreateWidget<UVeyraLoadingScreen>(&Spawner.GetWorld());
			ASSERT_THAT(IsNotNull(Screen));
			const TArray<UVeyraShellButton*> Buttons = ButtonsOf(*Screen);
			ASSERT_THAT(IsTrue(Buttons.Num() == 2));
			UVeyraShellButton& Next = *Buttons[1];
			const float OwnWidth = Next.GetOutlineWidth();
			const FLinearColor OwnFill = Next.GetFillColor();
			const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
			Next.ShowEnhancedFocus(true);
			ASSERT_THAT(IsTrue(Next.IsShowingEnhancedFocus() && Next.GetOutlineWidth() == Style.FocusOutlineWidth && Next.GetOutlineColor().Equals(Style.FocusOutlineColor)
				&& Next.GetOutlineWidth() > OwnWidth, TEXT("a thicker, high-contrast outline")));
			ASSERT_THAT(IsTrue(Next.GetFillColor().Equals(OwnFill), TEXT("the same fill: focus is not hover")));
			Next.ShowEnhancedFocus(false);
			ASSERT_THAT(IsTrue(!Next.IsShowingEnhancedFocus() && Next.GetOutlineWidth() == OwnWidth, TEXT("its own style again")));
		}

		TEST_METHOD(OnlyTheFocusedButtonIsLitAndOnlyUnderEnhanced)
		{
			UVeyraLoadingScreen* Screen = CreateWidget<UVeyraLoadingScreen>(&Spawner.GetWorld());
			ASSERT_THAT(IsNotNull(Screen));
			const TArray<UVeyraShellButton*> Buttons = ButtonsOf(*Screen);
			ASSERT_THAT(IsTrue(UVeyraShellButton::FindBySlate(Buttons[0]->GetCachedWidget()) == Buttons[0], TEXT("found by its Slate widget")));
			FVeyraFocusLight Light;
			FVeyraFocusLight* Focus = &Light;
			Focus->Follow(Buttons[0], /*bEnhanced*/ false);
			ASSERT_THAT(IsTrue(Focus->GetLit() == nullptr && !Buttons[0]->IsShowingEnhancedFocus(), TEXT("Standard lights nothing")));
			Focus->Follow(Buttons[0], /*bEnhanced*/ true);
			ASSERT_THAT(IsTrue(Buttons[0]->IsShowingEnhancedFocus()));
			Focus->Follow(Buttons[1], /*bEnhanced*/ true);
			ASSERT_THAT(IsTrue(!Buttons[0]->IsShowingEnhancedFocus() && Buttons[1]->IsShowingEnhancedFocus(), TEXT("focus moved, the outline with it")));
			Focus->Follow(nullptr, /*bEnhanced*/ true);
			ASSERT_THAT(IsTrue(!Buttons[1]->IsShowingEnhancedFocus()));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI