// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "VeyraSettingsDocument.h"
#include "VeyraSettingsRegistry.h"
#include "VeyraSettingsStore.h"
#include "VeyraSettingsSubsystem.h"
#include "VeyraUserSettings.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraSettingsTests
{
	FVeyraContentId SettingId(const TCHAR* Text)
	{
		return FVeyraContentId::FromText(Text).GetValue();
	}

	/** A registry in the committed one's shape, with fixture values. */
	FVeyraSettingsRegistry FixtureRegistry()
	{
		FVeyraSettingsRegistry Registry;
		FVeyraToggleSetting& Toggle = Registry.Toggles.Add(SettingId(TEXT("test_toggle")));
		Toggle.Category = EVeyraSettingCategory::Camera;
		Toggle.Default = EVeyraToggleValue::On;
		FVeyraRangeSetting& Range = Registry.Ranges.Add(SettingId(TEXT("test_range")));
		Range.Category = EVeyraSettingCategory::Interface;
		Range.Minimum = 0.0;
		Range.Maximum = 100.0;
		Range.Step = 5.0;
		Range.Default = 50.0;
		FVeyraChoiceSetting& Choice = Registry.Choices.Add(SettingId(TEXT("test_choice")));
		Choice.Category = EVeyraSettingCategory::Controls;
		Choice.Availability = EVeyraSettingAvailability::OutsideMatches;
		Choice.Options = { TEXT("Hold"), TEXT("Toggle") };
		Choice.Default = TEXT("Hold");
		FVeyraChoiceSetting& Mode = Registry.Choices.Add(SettingId(TEXT("test_mode")));
		Mode.Category = EVeyraSettingCategory::GraphicsDisplay;
		Mode.Scope = EVeyraSettingScope::Device;
		Mode.Options = { TEXT("Windowed"), TEXT("Borderless") };
		Mode.Default = TEXT("Borderless");
		return Registry;
	}

	bool HasProblem(const TArray<FString>& Problems, const TCHAR* Pointer)
	{
		return Problems.ContainsByPredicate([Pointer](const FString& Problem) { return Problem.StartsWith(Pointer); });
	}

	// Veyra.Settings.SettingsRegistry.*: the registry of every player setting (ADR-024 §2).
	TEST_CLASS(SettingsRegistry, "Veyra.Settings")
	{
		TEST_METHOD(TheCommittedRegistryLoadsAndEveryDefaultIsAValue)
		{
			FVeyraSettingsRegistry Registry;
			const TArray<FString> Problems = UVeyraSettingsSubsystem::LoadRegistry(Registry);
			ASSERT_THAT(IsTrue(Problems.IsEmpty(), FString::Join(Problems, TEXT(" | "))));
			const TArray<FVeyraSettingInfo> Settings = VeyraSettings::All(Registry);
			ASSERT_THAT(IsFalse(Settings.IsEmpty()));
			for (const FVeyraSettingInfo& Setting : Settings)
			{
				const TOptional<FString> Normal = VeyraSettings::Normalize(Setting, Setting.Default);
				ASSERT_THAT(IsTrue(Normal.IsSet() && *Normal == Setting.Default, Setting.Id.ToString()));
			}
			// The match's display mode is the player's, Borderless Fullscreen by default (SET-166).
			const TOptional<FVeyraSettingInfo> Display = VeyraSettings::Find(Registry, SettingId(TEXT("display_match_mode")));
			ASSERT_THAT(IsTrue(Display.IsSet() && Display->Scope == EVeyraSettingScope::Device && Display->Default == TEXT("BorderlessFullscreen")));
		}

		TEST_METHOD(ValidationCatchesWhatTheSchemaCannot)
		{
			FVeyraSettingsRegistry Broken = FixtureRegistry();
			FVeyraRangeSetting& Range = Broken.Ranges[SettingId(TEXT("test_range"))];
			Range.Default = 52.0;
			FVeyraChoiceSetting& Choice = Broken.Choices[SettingId(TEXT("test_choice"))];
			Choice.Options.Add(TEXT("Hold"));
			Choice.Default = TEXT("Press");
			FVeyraRangeSetting Flat = Range;
			Flat.Maximum = Flat.Minimum;
			Broken.Ranges.Add(SettingId(TEXT("test_toggle")), Flat);
			const TArray<FString> Problems = VeyraSettings::Validate(Broken);
			ASSERT_THAT(IsTrue(HasProblem(Problems, TEXT("/ranges/test_range/default")), FString::Join(Problems, TEXT(" | "))));
			ASSERT_THAT(IsTrue(HasProblem(Problems, TEXT("/choices/test_choice/options"))));
			ASSERT_THAT(IsTrue(HasProblem(Problems, TEXT("/choices/test_choice/default"))));
			ASSERT_THAT(IsTrue(HasProblem(Problems, TEXT("/ranges/test_toggle")), TEXT("one ID, one setting")));
			ASSERT_THAT(IsTrue(HasProblem(Problems, TEXT("/ranges/test_toggle/maximum"))));
			ASSERT_THAT(IsTrue(VeyraSettings::Validate(FixtureRegistry()).IsEmpty()));
		}

		TEST_METHOD(ASettingTakesOnlyItsOwnValues)
		{
			const FVeyraSettingsRegistry Registry = FixtureRegistry();
			const FVeyraSettingInfo Toggle = VeyraSettings::Find(Registry, SettingId(TEXT("test_toggle"))).GetValue();
			const FVeyraSettingInfo Range = VeyraSettings::Find(Registry, SettingId(TEXT("test_range"))).GetValue();
			const FVeyraSettingInfo Choice = VeyraSettings::Find(Registry, SettingId(TEXT("test_choice"))).GetValue();
			ASSERT_THAT(IsTrue(VeyraSettings::Normalize(Toggle, TEXT("off")) == TOptional<FString>(VeyraSettings::Off())));
			ASSERT_THAT(IsFalse(VeyraSettings::Normalize(Toggle, TEXT("maybe")).IsSet()));
			ASSERT_THAT(IsTrue(VeyraSettings::Normalize(Range, TEXT("52")) == TOptional<FString>(TEXT("50")), TEXT("snaps to the nearest step")));
			ASSERT_THAT(IsTrue(VeyraSettings::Normalize(Range, TEXT("53")) == TOptional<FString>(TEXT("55"))));
			ASSERT_THAT(IsFalse(VeyraSettings::Normalize(Range, TEXT("101")).IsSet(), TEXT("beyond the bounds")));
			ASSERT_THAT(IsFalse(VeyraSettings::Normalize(Range, TEXT("fast")).IsSet()));
			ASSERT_THAT(IsTrue(VeyraSettings::Normalize(Choice, TEXT("toggle")) == TOptional<FString>(TEXT("Toggle")), TEXT("the option's own spelling")));
			ASSERT_THAT(IsFalse(VeyraSettings::Normalize(Choice, TEXT("Press")).IsSet()));
			ASSERT_THAT(AreEqual(FString(TEXT("4.5")), VeyraSettings::NumberText(4.5)));
		}
	};

	// Veyra.Settings.SettingsStore.*: the player's values over the registry (ADR-024 §5; Settings Bible §6.1).
	TEST_CLASS(SettingsStore, "Veyra.Settings")
	{
		const FVeyraSettingsRegistry Registry = FixtureRegistry();

		TEST_METHOD(DefaultsHoldUntilChangedAndOnlyChangesAreKept)
		{
			FVeyraSettingsStore Store(Registry);
			const FVeyraContentId Range = SettingId(TEXT("test_range"));
			int32 Changes = 0;
			Store.OnChanged.AddLambda([&Changes](const FVeyraContentId&) { ++Changes; });
			ASSERT_THAT(AreEqual(FString(TEXT("50")), Store.Get(Range)));
			ASSERT_THAT(IsTrue(Store.Set(Range, TEXT("70")) == EVeyraSettingChange::Changed));
			ASSERT_THAT(IsTrue(Store.Set(Range, TEXT("70")) == EVeyraSettingChange::Unchanged));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Store.GetNumber(Range), 70.0) && Store.IsChanged(Range)));
			ASSERT_THAT(IsTrue(Store.SaveScope(EVeyraSettingScope::Account).FindRef(TEXT("test_range")) == TEXT("70")));
			ASSERT_THAT(IsTrue(Store.Set(Range, TEXT("50")) == EVeyraSettingChange::Changed));
			ASSERT_THAT(IsTrue(Store.SaveScope(EVeyraSettingScope::Account).IsEmpty(), TEXT("a default is not kept")));
			ASSERT_THAT(AreEqual(2, Changes));
			ASSERT_THAT(IsTrue(Store.IsOn(SettingId(TEXT("test_toggle")))));
		}

		TEST_METHOD(ItRefusesUnknownSettingsValuesTheyDoNotTakeAndMatchOnlyLimits)
		{
			FVeyraSettingsStore Store(Registry);
			ASSERT_THAT(IsTrue(Store.Set(SettingId(TEXT("test_nothing")), TEXT("On")) == EVeyraSettingChange::UnknownSetting));
			ASSERT_THAT(IsTrue(Store.Set(SettingId(TEXT("test_toggle")), TEXT("Sometimes")) == EVeyraSettingChange::InvalidValue));
			ASSERT_THAT(IsTrue(Store.Set(SettingId(TEXT("test_choice")), TEXT("Toggle"), /*bInLiveMatch*/ true) == EVeyraSettingChange::NotInMatch));
			ASSERT_THAT(IsTrue(Store.Set(SettingId(TEXT("test_choice")), TEXT("Toggle")) == EVeyraSettingChange::Changed, TEXT("outside a match it may")));
		}

		TEST_METHOD(UndoTakesBackTheLastSingleChangeOnce)
		{
			FVeyraSettingsStore Store(Registry);
			const FVeyraContentId Range = SettingId(TEXT("test_range"));
			const FVeyraContentId Toggle = SettingId(TEXT("test_toggle"));
			Store.Set(Range, TEXT("60"));
			Store.Set(Range, TEXT("80"));
			ASSERT_THAT(IsTrue(Store.Undo()));
			ASSERT_THAT(AreEqual(FString(TEXT("60")), Store.Get(Range), TEXT("the value before the last change")));
			ASSERT_THAT(IsFalse(Store.Undo(), TEXT("one step only")));
			Store.Reset(Range);
			ASSERT_THAT(IsTrue(Store.Undo() && Store.Get(Range) == TEXT("60"), TEXT("a single reset is a change too")));

			Store.Set(Toggle, TEXT("Off"));
			Store.ResetCategory(EVeyraSettingCategory::Interface);
			ASSERT_THAT(IsTrue(Store.Get(Range) == TEXT("50") && Store.Get(Toggle) == VeyraSettings::Off(), TEXT("only its category")));
			ASSERT_THAT(IsFalse(Store.CanUndo(), TEXT("a category reset is not undone")));
			Store.ResetAll();
			ASSERT_THAT(IsTrue(Store.IsOn(Toggle) && Store.SaveScope(EVeyraSettingScope::Account).IsEmpty()));
		}

		TEST_METHOD(LoadingDropsWhatThisBuildDoesNotOffer)
		{
			FVeyraSettingsStore Store(Registry);
			const TMap<FString, FString> Saved = {
				{ TEXT("test_range"), TEXT("30") },
				{ TEXT("test_retired"), TEXT("On") },
				{ TEXT("test_mode"), TEXT("Windowed") },
				{ TEXT("test_toggle"), TEXT("Sometimes") },
			};
			ASSERT_THAT(AreEqual(3, Store.LoadScope(EVeyraSettingScope::Account, Saved), TEXT("a retired setting, one of the other scope, a bad value")));
			ASSERT_THAT(AreEqual(FString(TEXT("30")), Store.Get(SettingId(TEXT("test_range")))));
			ASSERT_THAT(AreEqual(FString(TEXT("Borderless")), Store.Get(SettingId(TEXT("test_mode")))));
		}
	};

	// Veyra.Settings.SettingsDocument.*: the account document the backend keeps (ADR-024 §1).
	TEST_CLASS(SettingsDocument, "Veyra.Settings")
	{
		TEST_METHOD(ItRoundTripsAndTheCacheAloneKeepsUnsent)
		{
			FVeyraAccountSettingsDocument Document;
			Document.Revision = 7;
			Document.Values = { { TEXT("test_range"), TEXT("70") }, { TEXT("test_choice"), TEXT("Toggle") } };
			Document.bUnsent = true;
			FVeyraAccountSettingsDocument Read;
			FString Problem;
			ASSERT_THAT(IsTrue(VeyraSettingsDocument::Read(VeyraSettingsDocument::Write(Document, /*bForCache*/ true), Read, Problem), Problem));
			ASSERT_THAT(IsTrue(Read.Revision == 7 && Read.bUnsent && Read.Values.OrderIndependentCompareEqual(Document.Values)));
			ASSERT_THAT(IsTrue(VeyraSettingsDocument::Read(VeyraSettingsDocument::Write(Document, /*bForCache*/ false), Read, Problem)));
			ASSERT_THAT(IsFalse(Read.bUnsent, TEXT("the backend's copy has nothing unsent")));
		}

		TEST_METHOD(ItRefusesAnotherVersionOrShape)
		{
			FVeyraAccountSettingsDocument Read;
			FString Problem;
			ASSERT_THAT(IsFalse(VeyraSettingsDocument::Read(TEXT("{\"schemaVersion\":2,\"revision\":1,\"values\":{}}"), Read, Problem)));
			ASSERT_THAT(IsFalse(VeyraSettingsDocument::Read(TEXT("{\"schemaVersion\":1,\"revision\":1,\"values\":{\"test_range\":70}}"), Read, Problem)));
			ASSERT_THAT(IsFalse(VeyraSettingsDocument::Read(TEXT("[]"), Read, Problem)));
		}
	};

	// Veyra.Settings.SettingsPersistence.*: where each scope is kept (ADR-024 §1; Settings Bible §7).
	TEST_CLASS(SettingsPersistence, "Veyra.Settings")
	{
		const FVeyraSettingsRegistry Registry = FixtureRegistry();
		FString CacheDirectory;
		UVeyraUserSettings* Device = nullptr;

		// Fixture account IDs, as the backend spells them.
		const FString First = TEXT("11111111-aaaa-4bbb-8ccc-dddddddddddd");
		const FString Second = TEXT("22222222-aaaa-4bbb-8ccc-dddddddddddd");

		BEFORE_EACH()
		{
			CacheDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("VeyraSettings"), FGuid::NewGuid().ToString());
			Device = NewObject<UVeyraUserSettings>();
			UVeyraSettingsSubsystem::SetTestRegistry(&Registry);
			UVeyraSettingsSubsystem::SetTestCacheDirectory(CacheDirectory);
			UVeyraSettingsSubsystem::SetTestDeviceSettings(Device);
		}

		AFTER_EACH()
		{
			UVeyraSettingsSubsystem::SetTestRegistry(nullptr);
			UVeyraSettingsSubsystem::SetTestCacheDirectory(FString());
			UVeyraSettingsSubsystem::SetTestDeviceSettings(nullptr);
			IFileManager::Get().DeleteDirectory(*CacheDirectory, /*RequireExists*/ false, /*Tree*/ true);
		}

		UVeyraSettingsSubsystem& StartSettings() const
		{
			// A game instance subsystem lives in one; this one is never initialized, so nothing else starts.
			UVeyraSettingsSubsystem* Settings = NewObject<UVeyraSettingsSubsystem>(NewObject<UGameInstance>());
			Settings->Start();
			return *Settings;
		}

		TEST_METHOD(DeviceSettingsStayOnTheMachine)
		{
			UVeyraSettingsSubsystem& Settings = StartSettings();
			ASSERT_THAT(IsTrue(Settings.IsReady()));
			Settings.GetStore().Set(SettingId(TEXT("test_mode")), TEXT("Windowed"));
			Settings.GetStore().Set(SettingId(TEXT("test_range")), TEXT("70"));
			ASSERT_THAT(IsTrue(Device->DeviceSettings.FindRef(TEXT("test_mode")) == TEXT("Windowed")));
			ASSERT_THAT(IsFalse(Device->DeviceSettings.Contains(TEXT("test_range")), TEXT("an account setting is not the machine's")));
			ASSERT_THAT(AreEqual(FString(TEXT("Windowed")), StartSettings().GetStore().Get(SettingId(TEXT("test_mode"))), TEXT("the next start finds it")));
		}

		TEST_METHOD(AccountSettingsFollowTheirAccountAndTheBackendsDocument)
		{
			const FVeyraContentId Range = SettingId(TEXT("test_range"));
			UVeyraSettingsSubsystem& Settings = StartSettings();
			int32 Sends = 0;
			Settings.OnAccountChanged.AddLambda([&Sends] { ++Sends; });
			Settings.UseAccount(First);
			Settings.GetStore().Set(Range, TEXT("70"));
			ASSERT_THAT(IsTrue(Sends == 1 && Settings.HasUnsentAccountChanges()));

			// Another start on this machine finds the account's cache, unsent changes and all.
			UVeyraSettingsSubsystem& Later = StartSettings();
			Later.UseAccount(First);
			ASSERT_THAT(IsTrue(Later.GetStore().Get(Range) == TEXT("70") && Later.HasUnsentAccountChanges()));
			Later.UseAccount(Second);
			ASSERT_THAT(AreEqual(FString(TEXT("50")), Later.GetStore().Get(Range), TEXT("never another account's (SET-121)")));

			// The backend's document takes their place; a send records its revision.
			Later.UseAccount(First);
			FVeyraAccountSettingsDocument FromBackend;
			FromBackend.Revision = 7;
			FromBackend.Values = { { TEXT("test_range"), TEXT("20") } };
			Later.TakeAccountDocument(FromBackend);
			ASSERT_THAT(IsTrue(Later.GetStore().Get(Range) == TEXT("20") && !Later.HasUnsentAccountChanges() && Later.GetAccountDocument().Revision == 7));
			Later.MarkAccountSent(8);
			ASSERT_THAT(AreEqual(int64(8), Later.GetAccountDocument().Revision));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
