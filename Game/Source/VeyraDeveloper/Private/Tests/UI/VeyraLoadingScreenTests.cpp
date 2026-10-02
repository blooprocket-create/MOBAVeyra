// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Components/ActorTestSpawner.h"
#include "Loading/VeyraLoadingModel.h"
#include "Loading/VeyraLoadingScreen.h"
#include "Settings/VeyraInterfacePreferences.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "Text/VeyraContentText.h"
#include "VeyraSettingsStore.h"
#include "VeyraSettingsSubsystem.h"

namespace VeyraLoadingScreenTests
{
	using namespace VeyraLoadingModel;

	// Veyra.UI.LoadingScreen.*: the match loading screen (Match Flow Bible §13; SET-114–120; ADR-053 §3).
	TEST_CLASS(LoadingScreen, "Veyra.UI")
	{
		// Fixture timing, independent of the committed style.
		static constexpr double Minimum = 8.0;
		static constexpr int32 Base = 120;
		static constexpr int32 PerSecond = 20;

		FActorTestSpawner Spawner;

		static FVeyraLoadingTiming TimingOf()
		{
			FVeyraLoadingTiming Timing;
			Timing.MinimumSeconds = Minimum;
			Timing.BaseCharacters = Base;
			Timing.CharactersPerSecond = PerSecond;
			return Timing;
		}

		static TArray<FVeyraLoadingEntry> EntriesOf(int32 Count)
		{
			TArray<FVeyraLoadingEntry> Entries;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Entries.Add(FVeyraLoadingEntry{ FText::FromString(FString::Printf(TEXT("Entry %d"), Index)), Index % 2 == 1 });
			}
			return Entries;
		}

		TEST_METHOD(TheStageIsHonestAndTheScreenEndsWithLoading)
		{
			ASSERT_THAT(IsTrue(StageOf({}, false) == EVeyraLoadingStage::LoadingMatch, TEXT("before the match's state arrives")));
			ASSERT_THAT(IsTrue(StageOf(EVeyraMatchPhase::Loading, false) == EVeyraLoadingStage::LoadingMatch, TEXT("before the player's own Vanguard")));
			ASSERT_THAT(IsTrue(StageOf(EVeyraMatchPhase::Loading, true) == EVeyraLoadingStage::WaitingForPlayers));
			for (const EVeyraMatchPhase Phase : { EVeyraMatchPhase::Preparation, EVeyraMatchPhase::Live, EVeyraMatchPhase::Ended })
			{
				ASSERT_THAT(IsFalse(StageOf(Phase, true).IsSet(), TEXT("done: the screen never waits for a tip")));
			}
		}

		TEST_METHOD(TheEntriesFollowThePlayersCategories)
		{
			const TArray<FText> Tips = { FText::FromString(TEXT("tip")) };
			const TArray<FText> Lore = { FText::FromString(TEXT("lore a")), FText::FromString(TEXT("lore b")) };
			ASSERT_THAT(IsTrue(EntriesFor(EVeyraLoadingContent::Both, Tips, Lore).Num() == 3));
			const TArray<FVeyraLoadingEntry> TipsOnly = EntriesFor(EVeyraLoadingContent::TipsOnly, Tips, Lore);
			ASSERT_THAT(IsTrue(TipsOnly.Num() == 1 && !TipsOnly[0].bLore));
			const TArray<FVeyraLoadingEntry> LoreOnly = EntriesFor(EVeyraLoadingContent::LoreOnly, Tips, Lore);
			ASSERT_THAT(IsTrue(LoreOnly.Num() == 2 && LoreOnly[0].bLore && LoreOnly[1].bLore));
			ASSERT_THAT(IsTrue(EntriesFor(EVeyraLoadingContent::Off, Tips, Lore).IsEmpty()));

			// Every option the registry offers is one the screen reads, Both by default.
			FVeyraSettingsRegistry Registry;
			UVeyraSettingsSubsystem::LoadRegistry(Registry);
			const TOptional<FVeyraSettingInfo> Setting = VeyraSettings::Find(Registry, VeyraInterfacePreferences::LoadingContent());
			ASSERT_THAT(IsTrue(Setting.IsSet() && Setting->Choice && Setting->Choice->Default == TEXT("Both")));
			TSet<EVeyraLoadingContent> Seen;
			for (const FString& Option : Setting->Choice->Options)
			{
				Seen.Add(ParseContent(Option));
			}
			ASSERT_THAT(IsTrue(Seen.Num() == Setting->Choice->Options.Num(), TEXT("each option reads differently")));
		}

		TEST_METHOD(TheTableHoldsTipsAndLore)
		{
			const TArray<FText> Tips = VeyraContentText::LoadingTips();
			const TArray<FText> Lore = VeyraContentText::LoadingLore();
			ASSERT_THAT(IsTrue(Tips.Num() >= 8 && Lore.Num() >= 8, FString::Printf(TEXT("%d tips, %d lore facts"), Tips.Num(), Lore.Num())));
			for (const TArray<FText>* Rows : { &Tips, &Lore })
			{
				for (const FText& Row : *Rows)
				{
					ASSERT_THAT(IsFalse(Row.IsEmpty()));
				}
			}
		}

		TEST_METHOD(EachEntryShowsOnceBeforeAnyRepeatsAndTheStartVaries)
		{
			constexpr int32 Count = 5;
			const TArray<FVeyraLoadingEntry> Entries = EntriesOf(Count);
			FVeyraLoadingRotation Rotation = Start(Count, /*Seed*/ 7, 0.0);
			TArray<int32> Shown = { Rotation.Shown().GetValue() };
			double Now = 0.0;
			for (int32 Step = 1; Step < Count; ++Step)
			{
				ASSERT_THAT(IsFalse(Advance(Rotation, Entries, TimingOf(), Now + Minimum - 0.1), TEXT("not before its 8 seconds")));
				Now += Minimum;
				ASSERT_THAT(IsTrue(Advance(Rotation, Entries, TimingOf(), Now)));
				Shown.Add(Rotation.Shown().GetValue());
			}
			ASSERT_THAT(IsTrue(TSet<int32>(Shown).Num() == Count, TEXT("every entry once in the first pass")));
			Now += Minimum;
			ASSERT_THAT(IsTrue(Advance(Rotation, Entries, TimingOf(), Now) && Rotation.Shown().GetValue() == Shown[0], TEXT("then round again")));

			TSet<int32> Firsts;
			for (int32 Seed = 1; Seed <= 20; ++Seed)
			{
				Firsts.Add(Start(Count, Seed, 0.0).Shown().GetValue());
			}
			ASSERT_THAT(IsTrue(Firsts.Num() > 1, TEXT("the starting entry varies from match to match")));
		}

		TEST_METHOD(LongerEntriesStayLonger)
		{
			ASSERT_THAT(IsTrue(SecondsFor(FText::FromString(TEXT("Short.")), TimingOf()) == Minimum, TEXT("never under 8 seconds")));
			const FString Long = FString::ChrN(Base + 2 * PerSecond, TEXT('a'));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(SecondsFor(FText::FromString(Long), TimingOf()), Minimum + 2.0)));
		}

		TEST_METHOD(BrowsingIsImmediateAndStopsTheRotation)
		{
			constexpr int32 Count = 4;
			const TArray<FVeyraLoadingEntry> Entries = EntriesOf(Count);
			FVeyraLoadingRotation Rotation = Start(Count, /*Seed*/ 3, 0.0);
			const int32 First = Rotation.Position;
			Browse(Rotation, 1, 1.0);
			ASSERT_THAT(IsTrue(Rotation.Position == (First + 1) % Count && Rotation.bManual));
			Browse(Rotation, -1, 2.0);
			Browse(Rotation, -1, 3.0);
			ASSERT_THAT(IsTrue(Rotation.Position == (First + Count - 1) % Count, TEXT("Previous wraps round")));
			ASSERT_THAT(IsFalse(Advance(Rotation, Entries, TimingOf(), 1000.0), TEXT("no automatic rotation after browsing")));
		}

		TEST_METHOD(TheScreenSaysItsStageAndBrowsesItsEntries)
		{
			UVeyraLoadingScreen* Screen = CreateWidget<UVeyraLoadingScreen>(&Spawner.GetWorld());
			ASSERT_THAT(IsNotNull(Screen));
			Screen->Show(EntriesOf(3), /*Seed*/ 11, TimingOf(), 0.0);
			ASSERT_THAT(IsFalse(Screen->GetShownText().IsEmpty()));
			ASSERT_THAT(IsTrue(UVeyraLoadingScreen::StageText(EVeyraLoadingStage::LoadingMatch).ToString() == TEXT("Loading Match")));
			Screen->SetStage(EVeyraLoadingStage::WaitingForPlayers);
			ASSERT_THAT(IsTrue(Screen->GetStage() == EVeyraLoadingStage::WaitingForPlayers));

			const FString Before = Screen->GetShownText().ToString();
			UVeyraShellButton* const* Next = Screen->GetButtons().FindByPredicate([](const UVeyraShellButton* Button) { return Button->GetLabel().ToString() == TEXT("Next"); });
			ASSERT_THAT(IsTrue(Next && *Next));
			(*Next)->Press();
			ASSERT_THAT(IsTrue(Screen->GetShownText().ToString() != Before && Screen->GetRotation().bManual, TEXT("Next shows the next entry at once")));

			// Off shows the stage alone.
			Screen->Show({}, /*Seed*/ 11, TimingOf(), 0.0);
			ASSERT_THAT(IsTrue(Screen->GetShownText().IsEmpty()));
		}

		TEST_METHOD(TheCommittedTimingKeepsEightSeconds)
		{
			const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
			ASSERT_THAT(IsTrue(Style.LoadingEntryMinimumSeconds >= 8.0f && Style.LoadingEntryCharactersPerSecond >= 1 && Style.LoadingPanelWidth >= 1.0f));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
