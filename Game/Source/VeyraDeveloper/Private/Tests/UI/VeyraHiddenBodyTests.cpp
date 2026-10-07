// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Greybox/VeyraGreyboxSettings.h"
#include "Greybox/VeyraHiddenBody.h"

namespace VeyraHiddenBodyTests
{
	// Veyra.UI.HiddenBody.*: a body hidden from its enemies wears a veil its own side sees (ADR-068 §6).
	TEST_CLASS(HiddenBody, "Veyra.UI")
	{
		// Fixture values: how long the veil takes to come, and a frame.
		static constexpr double FadeSeconds = 0.2;
		static constexpr double Frame = 0.05;

		TEST_METHOD(TheStrongestReasonWinsAndOnlyItsOwnSideSeesIt)
		{
			using namespace VeyraHiddenBody;
			ASSERT_THAT(IsTrue(KindOf(true, true, true, true) == EVeyraHiddenKind::Invisible));
			ASSERT_THAT(IsTrue(KindOf(true, false, true, true) == EVeyraHiddenKind::Camouflage));
			ASSERT_THAT(IsTrue(KindOf(true, false, false, true) == EVeyraHiddenKind::DenseFog));
			ASSERT_THAT(IsTrue(KindOf(true, false, false, false) == EVeyraHiddenKind::None));
			ASSERT_THAT(IsTrue(KindOf(false, true, true, true) == EVeyraHiddenKind::None, TEXT("the other side sees no veil")));
		}

		TEST_METHOD(TheVeilComesAndGoesOverItsFade)
		{
			double Veil = 0.0;
			Veil = VeyraHiddenBody::StepVeil(Veil, true, Frame, FadeSeconds);
			ASSERT_THAT(IsNear(Veil, Frame / FadeSeconds, 1e-9, TEXT("a frame's share of the fade")));
			for (int32 Step = 0; Step < 10; ++Step)
			{
				Veil = VeyraHiddenBody::StepVeil(Veil, true, Frame, FadeSeconds);
			}
			ASSERT_THAT(IsNear(Veil, 1.0, 1e-9, TEXT("and no further than veiled")));
			Veil = VeyraHiddenBody::StepVeil(Veil, false, FadeSeconds, FadeSeconds);
			ASSERT_THAT(IsNear(Veil, 0.0, 1e-9, TEXT("gone in one fade")));
		}

		TEST_METHOD(EachReasonHasItsOwnColour)
		{
			const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
			using namespace VeyraHiddenBody;
			ASSERT_THAT(IsTrue(TintOf(EVeyraHiddenKind::DenseFog, Settings).Equals(Settings.FogVeilColor)));
			ASSERT_THAT(IsTrue(TintOf(EVeyraHiddenKind::Camouflage, Settings).Equals(Settings.CamouflageVeilColor)));
			ASSERT_THAT(IsTrue(TintOf(EVeyraHiddenKind::Invisible, Settings).Equals(Settings.InvisibleVeilColor)));
			ASSERT_THAT(IsFalse(Settings.FogVeilColor.Equals(Settings.CamouflageVeilColor) || Settings.CamouflageVeilColor.Equals(Settings.InvisibleVeilColor),
				TEXT("a player tells them apart")));
		}
	};
}

#endif
