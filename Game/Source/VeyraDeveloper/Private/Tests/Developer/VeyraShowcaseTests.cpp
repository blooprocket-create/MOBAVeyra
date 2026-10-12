// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "DevCommands/VeyraDevCommands.h"
#include "Greybox/VeyraGreyboxLayout.h"
#include "Kit/VeyraKitOrders.h"
#include "Showcase/VeyraShowcaseSettings.h"
#include "UObject/Package.h"

namespace VeyraShowcaseTests
{
	// Veyra.Developer.Showcase.*: the skill showcase recorder's settings, its stage and its command (ADR-074).
	TEST_CLASS(Showcase, "Veyra.Developer")
	{
		/** The shipped settings with Change made to a copy of them; the copy's problems. */
		static TArray<FString> ProblemsWith(TFunctionRef<void(UVeyraShowcaseSettings&)> Change)
		{
			UVeyraShowcaseSettings* Settings = DuplicateObject(GetDefault<UVeyraShowcaseSettings>(), GetTransientPackage());
			Change(*Settings);
			return Settings->Validate();
		}

		TEST_METHOD(TheShippedSettingsAreWhole)
		{
			const TArray<FString> Problems = GetDefault<UVeyraShowcaseSettings>()->Validate();
			ASSERT_THAT(IsTrue(Problems.IsEmpty(), *FString::Join(Problems, TEXT("; "))));
		}

		TEST_METHOD(EachBrokenSettingIsNamed)
		{
			const auto Names = [](const TArray<FString>& Problems, const TCHAR* Field) {
				return Problems.ContainsByPredicate([Field](const FString& Problem) { return Problem.StartsWith(FString(Field) + TEXT(":")); });
			};
			ASSERT_THAT(IsTrue(Names(ProblemsWith([](UVeyraShowcaseSettings& Settings) { Settings.FrameStride = 0; }), TEXT("FrameStride"))));
			ASSERT_THAT(IsTrue(Names(ProblemsWith([](UVeyraShowcaseSettings& Settings) { Settings.CaptureSize = FIntPoint(0, 720); }), TEXT("CaptureSize"))));
			ASSERT_THAT(IsTrue(Names(ProblemsWith([](UVeyraShowcaseSettings& Settings) { Settings.MaxClipSeconds = Settings.TailSeconds; }),
				TEXT("MaxClipSeconds")), TEXT("a clip runs longer than its tail")));
			ASSERT_THAT(IsTrue(Names(ProblemsWith([](UVeyraShowcaseSettings& Settings) { Settings.ApproachShareOfReach = 1.5; }),
				TEXT("ApproachShareOfReach"))));
			ASSERT_THAT(IsTrue(Names(ProblemsWith([](UVeyraShowcaseSettings& Settings) { Settings.DefaultDummy.Empty(); }), TEXT("DefaultDummy"))));
		}

		TEST_METHOD(TheStageLayoutLoadsWithItsStartsCloseEnoughToFrameTogether)
		{
			FVeyraGreyboxLayout Stage;
			FVeyraGreyboxLayout TestMap;
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Stage, TEXT("Showcase.json")).IsEmpty()));
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(TestMap).IsEmpty()));
			ASSERT_THAT(IsTrue(Stage.TeamStarts.DistanceFromCenterX < TestMap.TeamStarts.DistanceFromCenterX,
				TEXT("the stage's starts stand closer than the test map's lane ends")));
			ASSERT_THAT(IsTrue(GetDefault<UVeyraShowcaseSettings>()->StandDistanceFromCenter < Stage.TeamStarts.DistanceFromCenterX,
				TEXT("the bodies stand inside the starts, so the start markers are behind them")));
		}

		TEST_METHOD(TheStageIsResetThroughOneServerCommand)
		{
			const FVeyraDevCommand* Reset = VeyraDevCommands::Find(TEXT("ResetShowcase"));
			ASSERT_THAT(IsNotNull(Reset));
			ASSERT_THAT(IsNotNull(Reset->ServerRun, TEXT("it runs on the server, with its authority")));
			ASSERT_THAT(AreEqual(FString(Reset->Category), FString(TEXT("Showcase"))));
		}

		TEST_METHOD(AScriptedCastWaitsOutWhatPassesAndStopsAtWhatDoesNot)
		{
			ASSERT_THAT(IsTrue(VeyraKitOrders::IsPassingRejection(EVeyraCastRejection::CrowdControlled)));
			ASSERT_THAT(IsTrue(VeyraKitOrders::IsPassingRejection(EVeyraCastRejection::OutOfRange)));
			ASSERT_THAT(IsFalse(VeyraKitOrders::IsPassingRejection(EVeyraCastRejection::InvalidTarget)));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
