// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Blueprint/UserWidget.h"
#include "Components/ActorTestSpawner.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Match/VeyraMatchMenu.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Settings/VeyraDisplayApplier.h"
#include "Settings/VeyraInterfacePreferences.h"
#include "Settings/VeyraSettingsModels.h"
#include "Settings/VeyraSettingsScreen.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellLook.h"
#include "Shell/VeyraShellScreen.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "Tests/Services/VeyraClientFlowTestRig.h"
#include "Text/VeyraContentText.h"
#include "VeyraPlayerController.h"
#include "VeyraSettingsStore.h"
#include "VeyraSettingsSubsystem.h"
#include "VeyraUserSettings.h"
#include "Input/VeyraInputSettings.h"
#include "Shell/VeyraUIInputSettings.h"

namespace VeyraSettingsScreenTests
{
	using namespace VeyraClientFlowTests;

	FVeyraContentId Setting(const TCHAR* Id)
	{
		return FVeyraContentId::FromText(Id).GetValue();
	}

	/** The committed registry, as the game loads it. */
	FVeyraSettingsRegistry CommittedRegistry()
	{
		FVeyraSettingsRegistry Registry;
		UVeyraSettingsSubsystem::LoadRegistry(Registry);
		return Registry;
	}

	const FVeyraSettingRowModel* FindRow(const FVeyraSettingsModel& Model, const TCHAR* Id)
	{
		return Model.Rows.FindByPredicate([Id](const FVeyraSettingRowModel& Row) { return Row.Id == Setting(Id); });
	}

	/** The player's settings on a game instance of their own, kept in memory and a scratch folder rather than this machine's. */
	struct FScopedTestSettings
	{
		FString CacheDirectory;
		UVeyraSettingsSubsystem* Settings = nullptr;

		explicit FScopedTestSettings(const FVeyraSettingsRegistry& Registry)
		{
			CacheDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("VeyraSettings"), FGuid::NewGuid().ToString());
			UVeyraSettingsSubsystem::SetTestRegistry(&Registry);
			UVeyraSettingsSubsystem::SetTestCacheDirectory(CacheDirectory);
			UVeyraSettingsSubsystem::SetTestDeviceSettings(NewObject<UVeyraUserSettings>());
			Settings = NewObject<UVeyraSettingsSubsystem>(NewObject<UGameInstance>());
			Settings->Start();
		}

		~FScopedTestSettings()
		{
			UVeyraSettingsSubsystem::SetTestRegistry(nullptr);
			UVeyraSettingsSubsystem::SetTestCacheDirectory(FString());
			UVeyraSettingsSubsystem::SetTestDeviceSettings(nullptr);
			IFileManager::Get().DeleteDirectory(*CacheDirectory, /*RequireExists*/ false, /*Tree*/ true);
		}

		FVeyraSettingsStore& Store() const { return Settings->GetStore(); }
	};

	// Veyra.UI.SettingsModels.*: what the Settings screen shows (ADR-024 §5; Settings Bible §6, §13).
	TEST_CLASS(SettingsModels, "Veyra.UI")
	{
		/** Loaded before each test: CQTest builds its classes while registering them, which in a packaged client is before the engine starts. */
		FVeyraSettingsRegistry Registry;

		BEFORE_EACH()
		{
			Registry = CommittedRegistry();
		}

		TEST_METHOD(OnlyCategoriesWithSettingsShowInTheLayoutsOrder)
		{
			const FVeyraSettingsStore Store(Registry);
			const FVeyraSettingsModel Camera = VeyraSettingsModels::Describe(Store, EVeyraSettingCategory::Camera, FString(), false);
			ASSERT_THAT(IsTrue(Camera.Categories == (TArray<EVeyraSettingCategory>{ EVeyraSettingCategory::Controls, EVeyraSettingCategory::Camera,
				EVeyraSettingCategory::Interface, EVeyraSettingCategory::Accessibility, EVeyraSettingCategory::Audio, EVeyraSettingCategory::GraphicsDisplay,
				EVeyraSettingCategory::Communication })));
			ASSERT_THAT(IsTrue(!Camera.Rows.IsEmpty() && Camera.Rows[0].Id == Setting(TEXT("camera_default_mode")), TEXT("the layout's order")));
			ASSERT_THAT(IsTrue(Camera.Rows.ContainsByPredicate([](const FVeyraSettingRowModel& Row) { return Row.Category != EVeyraSettingCategory::Camera; }) == false));

			const FVeyraSettingsModel Account = VeyraSettingsModels::Describe(Store, EVeyraSettingCategory::LanguageAccount, FString(), false);
			ASSERT_THAT(IsTrue(Account.Category == EVeyraSettingCategory::Controls, TEXT("a category with nothing in it falls back to the first")));
			ASSERT_THAT(AreEqual(FString(TEXT("Graphics & Display")), VeyraSettingsModels::CategoryName(EVeyraSettingCategory::GraphicsDisplay).ToString()));
		}

		TEST_METHOD(SearchFindsANameOrARelatedWordInAnyCategory)
		{
			const FVeyraSettingsStore Store(Registry);
			const FVeyraSettingsModel Fullscreen = VeyraSettingsModels::Describe(Store, EVeyraSettingCategory::Camera, TEXT("fullscreen"), false);
			ASSERT_THAT(IsTrue(Fullscreen.bSearching && FindRow(Fullscreen, TEXT("display_match_mode")), TEXT("by a related word (SET-102)")));
			const FVeyraSettingsModel Minimap = VeyraSettingsModels::Describe(Store, EVeyraSettingCategory::Camera, TEXT("  MINIMAP "), false);
			ASSERT_THAT(IsTrue(FindRow(Minimap, TEXT("interface_minimap_scale")) && FindRow(Minimap, TEXT("interface_minimap_click_moves_camera"))));
			ASSERT_THAT(IsNull(FindRow(Minimap, TEXT("camera_move_speed"))));
			ASSERT_THAT(IsTrue(VeyraSettingsModels::Describe(Store, EVeyraSettingCategory::Camera, TEXT("zzz"), false).Rows.IsEmpty()));
		}

		TEST_METHOD(RowsShowValuesStepsResetsAndLocks)
		{
			FVeyraSettingsRegistry Locked = Registry;
			Locked.Toggles[Setting(TEXT("controls_confine_cursor"))].Availability = EVeyraSettingAvailability::OutsideMatches;
			FVeyraSettingsStore Store(Locked);
			Store.Set(Setting(TEXT("camera_move_speed")), TEXT("100"));
			Store.Set(Setting(TEXT("camera_edge_scroll")), VeyraSettings::Off());

			const FVeyraSettingsModel Camera = VeyraSettingsModels::Describe(Store, EVeyraSettingCategory::Camera, FString(), false);
			const FVeyraSettingRowModel* Speed = FindRow(Camera, TEXT("camera_move_speed"));
			ASSERT_THAT(IsTrue(Speed && Speed->bChanged && Speed->ValueText.ToString() == TEXT("100") && !Speed->Higher.IsSet() && Speed->Lower.Get(FString()) == TEXT("99")));
			const FVeyraSettingRowModel* Edge = FindRow(Camera, TEXT("camera_edge_scroll"));
			ASSERT_THAT(IsTrue(Edge && Edge->Options.Num() == 2 && !Edge->Options[0].bSelected && Edge->Options[1].bSelected && Edge->ValueText.ToString() == TEXT("Off")));
			const FVeyraSettingRowModel* Mode = FindRow(Camera, TEXT("camera_default_mode"));
			ASSERT_THAT(IsTrue(Mode && !Mode->bChanged && Mode->Options.Num() == 3 && Mode->Options[2].Label.ToString() == TEXT("Semi-Locked")));
			ASSERT_THAT(IsTrue(Camera.bCanUndo && Camera.bCategoryChanged && Camera.bAnyChanged));

			const FVeyraSettingsModel InMatch = VeyraSettingsModels::Describe(Store, EVeyraSettingCategory::Controls, FString(), /*bInLiveMatch*/ true);
			const FVeyraSettingRowModel* Cursor = FindRow(InMatch, TEXT("controls_confine_cursor"));
			ASSERT_THAT(IsTrue(Cursor && Cursor->bLocked, TEXT("a setting for outside matches shows, locked, in a live match (§6.2)")));
			ASSERT_THAT(IsFalse(FindRow(InMatch, TEXT("controls_scoreboard_mode"))->bLocked));

			// A value between steps still reaches its bound.
			Store.Set(Setting(TEXT("interface_ping_seconds")), TEXT("7.5"));
			const FVeyraSettingsModel Interface = VeyraSettingsModels::Describe(Store, EVeyraSettingCategory::Interface, FString(), false);
			const FVeyraSettingRowModel* Ping = FindRow(Interface, TEXT("interface_ping_seconds"));
			ASSERT_THAT(IsTrue(Ping && Ping->Higher.Get(FString()) == TEXT("8") && Ping->ValueText.ToString() == TEXT("7.5")));
		}

		TEST_METHOD(EverySettingHasItsText)
		{
			const TArray<FString> Missing = VeyraContentText::FindMissingSettingText(Registry);
			ASSERT_THAT(IsTrue(Missing.IsEmpty(), FString::Printf(TEXT("Game/Text/VeyraText.csv lacks %s"), *FString::Join(Missing, TEXT(", ")))));
			ASSERT_THAT(AreEqual(FString(TEXT("Borderless Fullscreen")), VeyraContentText::SettingOption(Setting(TEXT("display_match_mode")), TEXT("BorderlessFullscreen")).ToString()));
		}
	};

	// Veyra.UI.SettingsScreen.*: the Settings screen's widgets over the player's store.
	TEST_CLASS(SettingsScreen, "Veyra.UI")
	{
		FActorTestSpawner Spawner;
		/** Loaded before each test: CQTest builds its classes while registering them, which in a packaged client is before the engine starts. */
		FVeyraSettingsRegistry Registry;
		TUniquePtr<FScopedTestSettings> Settings;
		UVeyraSettingsScreen* Screen = nullptr;
		int32 Closes = 0;

		BEFORE_EACH()
		{
			Registry = CommittedRegistry();
			Settings = MakeUnique<FScopedTestSettings>(Registry);
			Screen = CreateWidget<UVeyraSettingsScreen>(&Spawner.GetWorld());
			Screen->Show(*Settings->Settings, /*bInLiveMatch*/ false, [this] { ++Closes; });
		}

		AFTER_EACH()
		{
			Screen->RemoveFromParent();
			Settings.Reset();
		}

		const FVeyraSettingRowModel& Row(const TCHAR* Id) const
		{
			return *FindRow(Screen->GetModel(), Id);
		}

		/** Presses the button named Label; false when there is none. */
		bool Press(const FText& Label) const
		{
			UVeyraShellButton* Button = Screen->FindButton(Label);
			if (!Button)
			{
				return false;
			}
			Button->Press();
			return true;
		}

		TEST_METHOD(AChoiceChangesAtOnceAndUndoTakesItBack)
		{
			Screen->ShowCategory(EVeyraSettingCategory::Camera);
			const FVeyraSettingRowModel Edge = Row(TEXT("camera_edge_scroll"));
			ASSERT_THAT(IsTrue(Press(UVeyraSettingsScreen::OptionLabel(Edge, Edge.Options[1]))));
			ASSERT_THAT(IsFalse(Settings->Store().IsOn(Setting(TEXT("camera_edge_scroll"))), TEXT("kept at once, with no Apply")));
			ASSERT_THAT(IsTrue(Row(TEXT("camera_edge_scroll")).bChanged && Screen->FindButton(UVeyraSettingsScreen::ResetLabel(Edge))));

			ASSERT_THAT(IsTrue(Press(UVeyraSettingsScreen::UndoLabel())));
			ASSERT_THAT(IsTrue(Settings->Store().IsOn(Setting(TEXT("camera_edge_scroll")))));
			ASSERT_THAT(IsFalse(Screen->FindButton(UVeyraSettingsScreen::UndoLabel())->GetIsEnabled(), TEXT("one step only (§6.1)")));

			const FVeyraSettingRowModel Speed = Row(TEXT("camera_move_speed"));
			ASSERT_THAT(IsTrue(Press(UVeyraSettingsScreen::StepLabel(Speed, /*bHigher*/ true))));
			ASSERT_THAT(IsTrue(Settings->Store().GetNumber(Setting(TEXT("camera_move_speed"))) == 51.0));
			ASSERT_THAT(IsTrue(Press(UVeyraSettingsScreen::ResetLabel(Row(TEXT("camera_move_speed"))))));
			ASSERT_THAT(IsFalse(Settings->Store().IsChanged(Setting(TEXT("camera_move_speed")))));
		}

		TEST_METHOD(TheControlsShowInTheirGroupsWithCastingAsAGrid)
		{
			// Each setting in its group; the casting modes, choices sharing their options, as one grid of radios (ADR-083 §2-§3).
			Screen->ShowCategory(EVeyraSettingCategory::Controls);
			const FVeyraSettingRowModel CastQ = Row(TEXT("controls_cast_mode_q"));
			ASSERT_THAT(IsTrue(CastQ.Group == Setting(TEXT("casting")) && Row(TEXT("controls_confine_cursor")).Group == Setting(TEXT("mouse"))));
			ASSERT_THAT(IsNotNull(Screen->FindButton(UVeyraSettingsScreen::GroupLinkLabel(VeyraContentText::SettingGroupName(Setting(TEXT("casting")))))));
			ASSERT_THAT(IsTrue(Press(UVeyraSettingsScreen::OptionLabel(CastQ, CastQ.Options[2]))));
			ASSERT_THAT(IsTrue(Settings->Store().Get(Setting(TEXT("controls_cast_mode_q"))) == CastQ.Options[2].Value));
		}

		TEST_METHOD(ASwitchIsNamedForTheValueItSetsAndALongChoiceIsADropdown)
		{
			// The switch offers only the value the toggle does not have.
			Screen->ShowCategory(EVeyraSettingCategory::Camera);
			const FVeyraSettingRowModel Edge = Row(TEXT("camera_edge_scroll"));
			ASSERT_THAT(IsTrue(Edge.Options[0].bSelected && !Screen->FindButton(UVeyraSettingsScreen::OptionLabel(Edge, Edge.Options[0]))));
			// A choice of many options is a dropdown: its options are there, hidden until it opens, and one closes it.
			Screen->ShowCategory(EVeyraSettingCategory::GraphicsDisplay);
			const FVeyraSettingRowModel Cap = Row(TEXT("display_frame_cap"));
			UVeyraShellButton* Last = Screen->FindButton(UVeyraSettingsScreen::OptionLabel(Cap, Cap.Options.Last()));
			ASSERT_THAT(IsTrue(Last && Last->GetParent() && Last->GetParent()->GetVisibility() == ESlateVisibility::Collapsed));
			ASSERT_THAT(IsTrue(Press(UVeyraSettingsScreen::DropdownLabel(Cap))));
			Last = Screen->FindButton(UVeyraSettingsScreen::OptionLabel(Cap, Cap.Options.Last()));
			ASSERT_THAT(IsTrue(Last->GetParent()->GetVisibility() == ESlateVisibility::Visible, TEXT("open")));
			Last->Press();
			ASSERT_THAT(IsTrue(Settings->Store().Get(Setting(TEXT("display_frame_cap"))) == Cap.Options.Last().Value));
			Last = Screen->FindButton(UVeyraSettingsScreen::OptionLabel(Cap, Cap.Options.Last()));
			ASSERT_THAT(IsTrue(Last->GetParent()->GetVisibility() == ESlateVisibility::Collapsed, TEXT("closed by the choice")));
		}

		TEST_METHOD(TheLayoutFollowsTheWidthItIsGivenAndAboutTheSettingInView)
		{
			// About describes the category's first setting until the pointer or focus moves (ADR-083 §4).
			Screen->ShowCategory(EVeyraSettingCategory::Camera);
			const FVeyraSettingRowModel First = Screen->GetModel().Rows[0];
			ASSERT_THAT(IsTrue(Screen->GetAboutSetting() == TOptional<FVeyraContentId>(First.Id) && Screen->DescribeText().Contains(First.Description.ToString())));
			// Wide, medium and narrow by the width it is given, not the window's (ADR-083 §5); narrow drops the group entries.
			const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
			Screen->ApplyWidth(Style.SettingsWideWidth);
			ASSERT_THAT(IsTrue(Screen->GetLayout() == UVeyraSettingsScreen::ELayout::Wide));
			Screen->ApplyWidth((Style.SettingsWideWidth + Style.SettingsNarrowWidth) / 2.0f);
			ASSERT_THAT(IsTrue(Screen->GetLayout() == UVeyraSettingsScreen::ELayout::Medium));
			Screen->ApplyWidth(Style.SettingsNarrowWidth / 2.0f);
			ASSERT_THAT(IsTrue(Screen->GetLayout() == UVeyraSettingsScreen::ELayout::Narrow));
			ASSERT_THAT(IsNull(Screen->FindButton(UVeyraSettingsScreen::GroupLinkLabel(VeyraContentText::SettingGroupName(First.Group)))));
			ASSERT_THAT(IsTrue(Screen->GetAboutSetting() == TOptional<FVeyraContentId>(First.Id), TEXT("the same setting through a new layout")));
		}

		TEST_METHOD(ResettingACategoryOrEverythingAsksFirst)
		{
			Settings->Store().Set(Setting(TEXT("camera_move_speed")), TEXT("70"));
			Settings->Store().Set(Setting(TEXT("camera_edge_zone")), TEXT("Wide"));
			Settings->Store().Set(Setting(TEXT("interface_hud_scale")), TEXT("120"));
			Screen->ShowCategory(EVeyraSettingCategory::Camera);
			ASSERT_THAT(IsTrue(Press(UVeyraSettingsScreen::ResetCategoryLabel())));
			ASSERT_THAT(IsTrue(Settings->Store().IsChanged(Setting(TEXT("camera_move_speed"))) && Screen->DescribeText().Contains(TEXT("Return every Camera setting"))));
			ASSERT_THAT(IsTrue(Press(UVeyraSettingsScreen::ConfirmResetLabel())));
			ASSERT_THAT(IsTrue(!Settings->Store().IsChanged(Setting(TEXT("camera_move_speed"))) && !Settings->Store().IsChanged(Setting(TEXT("camera_edge_zone")))));
			ASSERT_THAT(IsTrue(Settings->Store().IsChanged(Setting(TEXT("interface_hud_scale"))), TEXT("only the category")));

			ASSERT_THAT(IsTrue(Press(UVeyraSettingsScreen::ResetAllLabel())));
			ASSERT_THAT(IsTrue(Press(UVeyraSettingsScreen::CancelLabel())));
			ASSERT_THAT(IsTrue(Settings->Store().IsChanged(Setting(TEXT("interface_hud_scale"))), TEXT("Cancel keeps everything")));
			ASSERT_THAT(IsTrue(Press(UVeyraSettingsScreen::ResetAllLabel()) && Press(UVeyraSettingsScreen::ConfirmResetLabel())));
			ASSERT_THAT(IsFalse(Settings->Store().IsChanged(Setting(TEXT("interface_hud_scale")))));
			ASSERT_THAT(IsFalse(Screen->FindButton(UVeyraSettingsScreen::ResetAllLabel())->GetIsEnabled(), TEXT("nothing left to reset")));
		}

		TEST_METHOD(AResetWaitsForKeepAsAChangeDoes)
		{
			// An applier of the screen's own: outside a match, the window's size takes effect at once (SET-92).
			UVeyraDisplayApplier* Applier = NewObject<UVeyraDisplayApplier>(NewObject<UGameInstance>());
			UVeyraDisplayApplier::SetTestOverride(Applier);
			ON_SCOPE_EXIT
			{
				UVeyraDisplayApplier::SetTestOverride(nullptr);
			};
			Screen->Show(*Settings->Settings, /*bInLiveMatch*/ false, [this] { ++Closes; });
			const FVeyraContentId Size = Setting(TEXT("display_window_size"));
			Settings->Store().Set(Size, TEXT("1600x900"));
			Screen->ShowCategory(EVeyraSettingCategory::GraphicsDisplay);
			ASSERT_THAT(IsTrue(Press(UVeyraSettingsScreen::ResetLabel(Row(TEXT("display_window_size"))))));
			ASSERT_THAT(IsTrue(!Settings->Store().IsChanged(Size) && Applier->IsAwaitingConfirmation(), TEXT("a reset of the window's size waits for Keep")));

			Applier->KeepChange();
			Settings->Store().Set(Size, TEXT("1600x900"));
			ASSERT_THAT(IsTrue(Press(UVeyraSettingsScreen::ResetCategoryLabel()) && Press(UVeyraSettingsScreen::ConfirmResetLabel())));
			ASSERT_THAT(IsTrue(Applier->IsAwaitingConfirmation(), TEXT("so does a category's reset")));

			Applier->KeepChange();
			Settings->Store().Set(Size, TEXT("1600x900"));
			ASSERT_THAT(IsTrue(Press(UVeyraSettingsScreen::ResetAllLabel()) && Press(UVeyraSettingsScreen::ConfirmResetLabel())));
			ASSERT_THAT(IsTrue(Applier->IsAwaitingConfirmation(), TEXT("and everything's")));
		}

		TEST_METHOD(ASearchListsResultsWithTheirCategory)
		{
			Screen->SetSearch(TEXT("fullscreen"));
			const FString Text = Screen->DescribeText();
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("Display Mode")) && Text.Contains(TEXT("Graphics & Display"))));
			ASSERT_THAT(IsFalse(Text.Contains(TEXT("Camera Speed"))));
			Screen->ShowCategory(EVeyraSettingCategory::Camera);
			ASSERT_THAT(IsTrue(!Screen->GetModel().bSearching && Screen->DescribeText().Contains(TEXT("Camera Speed")), TEXT("a category ends the search")));
		}

		TEST_METHOD(ChangesFromElsewhereShowAndCloseRunsItsCallback)
		{
			Screen->ShowCategory(EVeyraSettingCategory::Interface);
			Settings->Store().Set(Setting(TEXT("interface_show_fps")), VeyraSettings::On());
			ASSERT_THAT(IsTrue(Row(TEXT("interface_show_fps")).Options[0].bSelected, TEXT("the account's sync or Undo shows at once")));
			ASSERT_THAT(IsTrue(Press(UVeyraSettingsScreen::CloseLabel()) && Closes == 1));
		}

		TEST_METHOD(TheWholeWindowTakesANewLookWhileItIsOpen)
		{
			// The look is the client's: the next test starts from the standard one.
			const FVeyraScopedShellLook Restore{ FVeyraShellLook() };
			const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
			const int32 StandardTitle = Screen->GetTitleTextSize();
			// Text size and transparency changed while Settings is open reach its frame too, not only its rows (ADR-055 §2–§3).
			Settings->Store().Set(Setting(TEXT("accessibility_text_size")), TEXT("ExtraLarge"));
			Settings->Store().Set(Setting(TEXT("accessibility_reduce_transparency")), VeyraSettings::On());
			ASSERT_THAT(IsTrue(Screen->GetTitleTextSize() > StandardTitle, TEXT("the title grows")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Screen->GetWindowFill().A, 1.0f) && Style.SurfaceRaisedColor.A < 1.0f, TEXT("the window goes opaque")));
		}
	};

	// Veyra.UI.SettingsBindings.*: rebinding in the Settings screen (Settings Bible §1.1; SET-81, SET-133).
	TEST_CLASS(SettingsBindings, "Veyra.UI")
	{
		FActorTestSpawner Spawner;
		/** Loaded before each test: CQTest builds its classes while registering them, which in a packaged client is before the engine starts. */
		FVeyraSettingsRegistry Registry;
		TUniquePtr<FScopedTestSettings> Settings;
		UVeyraSettingsScreen* Screen = nullptr;

		BEFORE_EACH()
		{
			Registry = CommittedRegistry();
			Settings = MakeUnique<FScopedTestSettings>(Registry);
			Screen = CreateWidget<UVeyraSettingsScreen>(&Spawner.GetWorld());
			Screen->Show(*Settings->Settings, /*bInLiveMatch*/ false, [] {});
			Screen->ShowCategory(EVeyraSettingCategory::Controls);
		}

		AFTER_EACH()
		{
			Screen->RemoveFromParent();
			Settings.Reset();
		}

		const FVeyraSettingRowModel& Row(const TCHAR* Id) const
		{
			return *FindRow(Screen->GetModel(), Id);
		}

		/** Presses a binding's button, then Key, as the player would. */
		bool Bind(const TCHAR* Id, const FKey& Key)
		{
			UVeyraShellButton* Change = Screen->FindButton(UVeyraSettingsScreen::ChangeLabel(Row(Id)));
			if (!Change)
			{
				return false;
			}
			Change->Press();
			if (!Screen->GetCapturing().IsSet())
			{
				return false;
			}
			Screen->CaptureKey(Key);
			return true;
		}

		TEST_METHOD(EveryBindingNamesAKeyOfTheDevelopersInputSettings)
		{
			for (const TPair<FVeyraContentId, FVeyraBindingSetting>& Binding : Registry.Bindings)
			{
				const bool bMatch = VeyraSettings::FindKeyProperty(*UVeyraInputSettings::StaticClass(), Binding.Value.Property) != nullptr;
				const bool bInterface = VeyraSettings::FindKeyProperty(*UVeyraUIInputSettings::StaticClass(), Binding.Value.Property) != nullptr;
				ASSERT_THAT(IsTrue(bMatch != bInterface, FString::Printf(TEXT("%s: %s is a key of exactly one input settings class"), *Binding.Key.ToString(), *Binding.Value.Property)));
			}
		}

		TEST_METHOD(ACapturedKeyBecomesTheBindingAndEscapeCancels)
		{
			ASSERT_THAT(AreEqual(FString(TEXT("Q")), Row(TEXT("controls_bind_ability_q")).ValueText.ToString(), TEXT("the developer's key shows until the player's")));
			ASSERT_THAT(IsTrue(Bind(TEXT("controls_bind_ability_q"), EKeys::T)));
			ASSERT_THAT(IsTrue(Settings->Store().Get(Setting(TEXT("controls_bind_ability_q"))) == TEXT("T") && !Screen->GetCapturing().IsSet()));
			ASSERT_THAT(AreEqual(FString(TEXT("T")), Row(TEXT("controls_bind_ability_q")).ValueText.ToString()));

			ASSERT_THAT(IsTrue(Bind(TEXT("controls_bind_ability_q"), EKeys::Escape)));
			ASSERT_THAT(IsTrue(Settings->Store().Get(Setting(TEXT("controls_bind_ability_q"))) == TEXT("T"), TEXT("Escape cancels")));
			ASSERT_THAT(IsTrue(Bind(TEXT("controls_bind_ability_q"), EKeys::Q)));
			ASSERT_THAT(IsFalse(Settings->Store().IsChanged(Setting(TEXT("controls_bind_ability_q"))), TEXT("the developer's key is the default again")));
		}

		TEST_METHOD(ATakenKeyAsksBeforeLeavingTheOtherWithoutAndEssentialsWarn)
		{
			ASSERT_THAT(IsTrue(Bind(TEXT("controls_bind_ability_q"), EKeys::B)));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("B is already Recall")), Screen->DescribeText()));
			ASSERT_THAT(IsFalse(Settings->Store().IsChanged(Setting(TEXT("controls_bind_ability_q"))), TEXT("nothing changes before the answer")));
			Screen->FindButton(UVeyraSettingsScreen::CancelLabel())->Press();
			ASSERT_THAT(IsFalse(Settings->Store().IsChanged(Setting(TEXT("controls_bind_recall")))));

			ASSERT_THAT(IsTrue(Bind(TEXT("controls_bind_ability_q"), EKeys::B)));
			Screen->FindButton(UVeyraSettingsScreen::ReplaceLabel())->Press();
			ASSERT_THAT(IsTrue(Settings->Store().Get(Setting(TEXT("controls_bind_ability_q"))) == TEXT("B") && Settings->Store().Get(Setting(TEXT("controls_bind_recall"))) == VeyraSettings::Unbound()));
			ASSERT_THAT(IsTrue(Screen->GetModel().UnboundEssentials.Num() == 1 && Screen->DescribeText().Contains(TEXT("No key for Recall")), TEXT("SET-133")));
		}

		TEST_METHOD(ASharedClickNeverConflicts)
		{
			ASSERT_THAT(IsTrue(Bind(TEXT("controls_bind_ping_click"), EKeys::RightMouseButton)));
			ASSERT_THAT(IsTrue(Settings->Store().Get(Setting(TEXT("controls_bind_ping_click"))) == TEXT("RightMouseButton"), TEXT("used with a ping key, it shares Move's button")));
		}
	};

	// Veyra.UI.SettingsEntryPoints.*: where Settings opens, and where it never does (ADR-024 §4).
	TEST_CLASS(SettingsEntryPoints, "Veyra.UI")
	{
		FActorTestSpawner Spawner;
		/** Loaded before each test: CQTest builds its classes while registering them, which in a packaged client is before the engine starts. */
		FVeyraSettingsRegistry Registry;
		TUniquePtr<FScopedTestSettings> Settings;
		/** Two players' clients, each outliving any screen bound to it. */
		FClientFlowTestRig Rig;
		FClientFlowTestRig Other;
		UVeyraShellScreen* Screen = nullptr;

		BEFORE_EACH()
		{
			Registry = CommittedRegistry();
			Settings = MakeUnique<FScopedTestSettings>(Registry);
		}

		AFTER_EACH()
		{
			if (Screen)
			{
				Screen->Unbind();
			}
			Settings.Reset();
		}

		/** A screen showing Shown's client, in place of any screen before it. */
		UVeyraShellScreen& ShowScreen(FClientFlowTestRig& Shown)
		{
			if (Screen)
			{
				Screen->Unbind();
			}
			Screen = CreateWidget<UVeyraShellScreen>(&Spawner.GetWorld());
			Screen->SetSettingsForTests(Settings->Settings);
			Screen->Bind(*Shown.Flow);
			return *Screen;
		}

		bool OffersSettings() const
		{
			UVeyraShellButton* Button = Screen->FindButton(UVeyraShellScreen::SettingsLabel());
			return Button && Button->GetIsEnabled();
		}

		TEST_METHOD(TheShellOffersSettingsOverItAndAMatchFoundClosesThem)
		{
			ASSERT_THAT(IsTrue(Rig.ReachQueue()));
			ShowScreen(Rig);
			ASSERT_THAT(IsTrue(OffersSettings()));
			Screen->FindButton(UVeyraShellScreen::SettingsLabel())->Press();
			ASSERT_THAT(IsTrue(Screen->IsSettingsOpen() && Screen->GetShownScreen() == EVeyraShellScreen::Shell));
			// A page of the shell, with the friends and party panels beside and under it (ADR-083 §1).
			ASSERT_THAT(IsTrue(Screen->GetSettingsScreen()->IsPage()));
			ASSERT_THAT(IsTrue(Screen->FindButton(UVeyraShellScreen::CollapseFriendsLabel()) || Screen->FindButton(UVeyraShellScreen::OpenFriendsLabel())));
			ASSERT_THAT(IsTrue(Screen->FindButton(UVeyraShellScreen::CollapsePartyLabel()) || Screen->FindButton(UVeyraShellScreen::OpenPartyLabel())));

			// A poll that changes the shell leaves them open; a match found takes the screen (UX-17).
			Rig.Advance(1.0);
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/party"), 200, PartyBody(TEXT("found"), true, 1.0))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/match-found"), 200, MatchFoundBody(TEXT("pending"), TEXT("pending"), 0))));
			ASSERT_THAT(IsTrue(Screen->GetShownScreen() == EVeyraShellScreen::MatchFound && !Screen->IsSettingsOpen()));
			ASSERT_THAT(IsFalse(OffersSettings(), TEXT("never in Match Found")));
		}

		TEST_METHOD(ThePartyPanelEasesBetweenOpenAndPutAwayUnlessStill)
		{
			// As the friends panel does (ADR-083 §6): from the height it had to its new one, over PanelCollapseSeconds.
			ASSERT_THAT(IsTrue(Rig.ReachQueue()));
			ShowScreen(Rig);
			// The panel as this test found it, on this device, for the tests after it.
			ON_SCOPE_EXIT { Screen->SetPartyCollapsed(false); };
			Screen->SetPartyCollapsed(false);
			Screen->SetPartyCollapsed(true);
			ASSERT_THAT(IsTrue(Screen->IsPartyMoving(), TEXT("it eases to its bar")));
			Screen->SetPartyCollapsed(false);
			ASSERT_THAT(IsTrue(Screen->IsPartyMoving(), TEXT("and back")));
			// Reduce UI Animation, as the player sets it, holds it still: it is at its new size at once. The look is the
			// client's: the next test starts from the standard one.
			const FVeyraScopedShellLook Restore{ FVeyraShellLook() };
			Screen->BindSettings(Settings->Store());
			Settings->Store().Set(VeyraInterfacePreferences::ReduceUiAnimation(), VeyraSettings::On());
			Screen->SetPartyCollapsed(true);
			ASSERT_THAT(IsFalse(Screen->IsPartyMoving()));
		}

		TEST_METHOD(TheFriendsAndPartyPanelsPutAwayComeBackAndAreRemembered)
		{
			ASSERT_THAT(IsTrue(Rig.ReachQueue()));
			ShowScreen(Rig);
			Screen->SetFriendsCollapsed(false);
			Screen->SetPartyCollapsed(false);
			ASSERT_THAT(IsTrue(Screen->FindButton(FText::FromString(TEXT("Add Friend"))) != nullptr, TEXT("open friends")));
			ASSERT_THAT(IsTrue(Screen->FindButton(UVeyraShellScreen::CollapsePartyLabel()) != nullptr, TEXT("open party")));
			// Put away: the friends panel a strip, the party a bar (ADR-083 §6).
			Screen->FindButton(UVeyraShellScreen::CollapseFriendsLabel())->Press();
			Screen->FindButton(UVeyraShellScreen::CollapsePartyLabel())->Press();
			ASSERT_THAT(IsTrue(Screen->IsFriendsCollapsed() && Screen->IsPartyCollapsed(), TEXT("both put away")));
			ASSERT_THAT(IsTrue(!Screen->FindButton(FText::FromString(TEXT("Add Friend"))) && Screen->FindButton(UVeyraShellScreen::OpenFriendsLabel())
				&& Screen->FindButton(UVeyraShellScreen::OpenPartyLabel())));
			// This machine remembers them for the next shell.
			if (const UVeyraUserSettings* Device = UVeyraUserSettings::Get())
			{
				ASSERT_THAT(IsTrue(Device->bFriendsPanelCollapsed && Device->bPartyPanelCollapsed, TEXT("remembered")));
				UVeyraShellScreen* Next = CreateWidget<UVeyraShellScreen>(&Spawner.GetWorld());
				ASSERT_THAT(IsTrue(Next->IsFriendsCollapsed() && Next->IsPartyCollapsed(), TEXT("the next shell")));
			}
			// And they come back.
			Screen->FindButton(UVeyraShellScreen::OpenFriendsLabel())->Press();
			Screen->FindButton(UVeyraShellScreen::OpenPartyLabel())->Press();
			ASSERT_THAT(IsTrue(!Screen->IsFriendsCollapsed() && !Screen->IsPartyCollapsed() && Screen->FindButton(FText::FromString(TEXT("Add Friend")))));
		}

		TEST_METHOD(TheLobbyAndTheResultsOfferSettings)
		{
			ASSERT_THAT(IsTrue(Rig.ReachLobby()));
			ShowScreen(Rig);
			ASSERT_THAT(IsTrue(Screen->GetShownScreen() == EVeyraShellScreen::Lobby && OffersSettings()));

			ASSERT_THAT(IsTrue(Other.ReachResults()));
			ShowScreen(Other);
			ASSERT_THAT(IsTrue(Screen->GetShownScreen() == EVeyraShellScreen::Results && OffersSettings()));
			Screen->OpenSettings();
			ASSERT_THAT(IsTrue(Screen->IsSettingsOpen()));
		}

		TEST_METHOD(NeverInChampionSelectOrReconnectOnly)
		{
			ASSERT_THAT(IsTrue(Rig.ReachSelect()));
			ShowScreen(Rig);
			ASSERT_THAT(IsTrue(Screen->GetShownScreen() == EVeyraShellScreen::ChampionSelect && Screen->FindButton(UVeyraShellScreen::SettingsLabel()) == nullptr));
			Screen->OpenSettings();
			ASSERT_THAT(IsFalse(Screen->IsSettingsOpen()));

			ASSERT_THAT(IsTrue(Other.ReachReconnectOnly()));
			ShowScreen(Other);
			ASSERT_THAT(IsTrue(Screen->GetShownScreen() == EVeyraShellScreen::ReconnectOnly && Screen->FindButton(UVeyraShellScreen::SettingsLabel()) == nullptr));
		}

		TEST_METHOD(TheMatchMenuOffersSettings)
		{
			AVeyraPlayerController& Controller = Spawner.SpawnActor<AVeyraPlayerController>();
			UVeyraMatchMenu* Menu = CreateWidget<UVeyraMatchMenu>(&Spawner.GetWorld());
			int32 Opened = 0;
			Menu->Show(Controller, [] {}, [&Opened] { ++Opened; });
			ASSERT_THAT(IsNotNull(Menu->FindButton(UVeyraMatchMenu::SettingsLabel())));
			Menu->FindButton(UVeyraMatchMenu::SettingsLabel())->Press();
			ASSERT_THAT(AreEqual(1, Opened));
			Menu->Show(Controller, [] {});
			ASSERT_THAT(IsNull(Menu->FindButton(UVeyraMatchMenu::SettingsLabel()), TEXT("only where Settings can open")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
