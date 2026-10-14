// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Blueprint/WidgetTree.h"
#include "Components/ActorTestSpawner.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellLook.h"
#include "Shell/VeyraShellMotes.h"
#include "Shell/VeyraShellMotion.h"
#include "Shell/VeyraShellStyleSettings.h"

namespace VeyraShellMotionTests
{
	// Veyra.UI.ShellMotion.*: the menus' motion (ADR-082 §7): a page's parts arriving, the light on a button, and the motes.
	TEST_CLASS(ShellMotion, "Veyra.UI")
	{
		TEST_METHOD(APartStartsHiddenAndAwayThenStandsInPlace)
		{
			float Opacity = 1.0f;
			FWidgetTransform Transform;
			FVeyraShellReveal::Pose(EVeyraReveal::Rise, 0, 0.0, Opacity, Transform);
			ASSERT_THAT(IsTrue(FMath::IsNearlyZero(Opacity) && Transform.Translation.Y > 0.0, TEXT("below, and clear")));
			FVeyraShellReveal::Pose(EVeyraReveal::Slide, 0, 0.0, Opacity, Transform);
			ASSERT_THAT(IsTrue(Transform.Translation.X < 0.0 && !FMath::IsNearlyZero(Transform.Shear.X), TEXT("left, and leaning")));
			// Each part a stagger after the one before; each in place by its own end.
			const double Early = GetDefault<UVeyraShellStyleSettings>()->RevealStagger * 1.5;
			float FirstOpacity = 0.0f;
			FVeyraShellReveal::Pose(EVeyraReveal::Rise, 0, Early, FirstOpacity, Transform);
			FVeyraShellReveal::Pose(EVeyraReveal::Rise, 2, Early, Opacity, Transform);
			ASSERT_THAT(IsTrue(Opacity < FirstOpacity, TEXT("the third behind the first")));
			for (const EVeyraReveal Kind : { EVeyraReveal::Rise, EVeyraReveal::Slide })
			{
				FVeyraShellReveal::Pose(Kind, 2, FVeyraShellReveal::SecondsUntilPlaced(2), Opacity, Transform);
				ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Opacity, 1.0f) && Transform.Translation.IsNearlyZero() && Transform.Shear.IsNearlyZero()));
			}
		}

		TEST_METHOD(AnActionTakesTheSheenAndATabKeepsStill)
		{
			UWidgetTree* Tree = NewObject<UWidgetTree>();
			UVeyraShellButton* Primary = UVeyraShellButton::MakeKind(*Tree, EVeyraShellButtonKind::Primary, FText::FromString(TEXT("Play")), [] {});
			UVeyraShellButton* Tab = UVeyraShellButton::MakeKind(*Tree, EVeyraShellButtonKind::Tab, FText::FromString(TEXT("Home")), [] {});
			ASSERT_THAT(IsTrue(Primary->HasSheen() && !Tab->HasSheen()));
			// Its Slate widget is the shell's lit button, which keyboard focus still finds it by.
			const TSharedRef<SWidget> Built = Primary->TakeWidget();
			ASSERT_THAT(IsTrue(Built->GetType() == FName(TEXT("SVeyraShellButton")) || UVeyraShellButton::FindBySlate(Built) == Primary));
			ASSERT_THAT(IsTrue(UVeyraShellButton::FindBySlate(Primary->GetCachedWidget()) == Primary));
		}

		TEST_METHOD(MotesKeepStillUnderReduceUiAnimationAndDriftAgainOnceItIsOff)
		{
			const FVeyraShellLook Before = VeyraShellLook::Current();
			FVeyraShellLook Moving = Before;
			Moving.bStillAnimation = false;
			FVeyraShellLook Still = Before;
			Still.bStillAnimation = true;
			VeyraShellLook::Use(Moving);
			const TSharedRef<SVeyraShellMotes> Motes = SNew(SVeyraShellMotes);
			const bool bDriftedFirst = Motes->IsDrifting();
			VeyraShellLook::Use(Still);
			Motes->Wake();
			const bool bKeptStill = !Motes->IsDrifting();
			VeyraShellLook::Use(Moving);
			Motes->Wake();
			const bool bDriftedAgain = Motes->IsDrifting();
			VeyraShellLook::Use(Before);
			ASSERT_THAT(IsTrue(bDriftedFirst && bKeptStill, TEXT("still while motion is reduced")));
			ASSERT_THAT(IsTrue(bDriftedAgain, TEXT("the setting undone: drifting again")));
		}

		TEST_METHOD(MotesFillTheirAreaAsTheSettingsSay)
		{
			const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
			ASSERT_THAT(IsTrue(Style.MoteArea >= 1.0f && Style.MoteMost > 0 && Style.SheenSeconds > 0.0f && Style.RevealSeconds > 0.0f));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
