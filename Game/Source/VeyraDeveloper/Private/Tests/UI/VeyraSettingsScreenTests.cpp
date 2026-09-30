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
#include "Settings/VeyraSettingsModels.h"
#include "Settings/VeyraSettingsScreen.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellScreen.h"
#include "Tests/Services/VeyraClientFlowTestRig.h"
#include "Text/VeyraContentText.h"
#include "VeyraPlayerController.h"
#include "VeyraSettingsStore.h"
#include "VeyraSettingsSubsystem.h"
#include "VeyraUserSettings.h"

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
		const FVeyraSettingsRegistry Registry = CommittedRegistry();

		TEST_METHOD(OnlyCategoriesWithSettingsShowInTheLayoutsOrder)
		{
			const FVeyraSettingsStore Store(Registry);
			const FVeyraSettingsModel Camera = VeyraSettingsModels::Describe(Store, EVeyraSettingCategory::Camera, FString(), false);
			ASSERT_THAT(IsTrue(Camera.Categories == (TArray<EVeyraSettingCategory>{ EVeyraSettingCategory::Controls, EVeyraSettingCategory::Camera,
				EVeyraSettingCategory::Interface, EVeyraSettingCategory::GraphicsDisplay })));
			ASSERT_THAT(IsTrue(!Camera.Rows.IsEmpty() && Camera.Rows[0].Id == Setting(TEXT("camera_default_mode")), TEXT("the layout's order")));
			ASSERT_THAT(IsTrue(Camera.Rows.ContainsByPredicate([](const FVeyraSettingRowModel& Row) { return Row.Category != EVeyraSettingCategory::Camera; }) == false));

			const FVeyraSettingsModel Audio = VeyraSettingsModels::Describe(Store, EVeyraSettingCategory::Audio, FString(), false);
			ASSERT_THAT(IsTrue(Audio.Category == EVeyraSettingCategory::Controls, TEXT("a category with nothing in it falls back to the first")));
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
		const FVeyraSettingsRegistry Registry = CommittedRegistry();
		TUniquePtr<FScopedTestSettings> Settings;
		UVeyraSettingsScreen* Screen = nullptr;
		int32 Closes = 0;

		BEFORE_EACH()
		{
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
	};

	// Veyra.UI.SettingsEntryPoints.*: where Settings opens, and where it never does (ADR-024 §4).
	TEST_CLASS(SettingsEntryPoints, "Veyra.UI")
	{
		FActorTestSpawner Spawner;
		const FVeyraSettingsRegistry Registry = CommittedRegistry();
		TUniquePtr<FScopedTestSettings> Settings;
		/** Two players' clients, each outliving any screen bound to it. */
		FClientFlowTestRig Rig;
		FClientFlowTestRig Other;
		UVeyraShellScreen* Screen = nullptr;

		BEFORE_EACH()
		{
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

			// A poll that changes the shell leaves them open; a match found takes the screen (UX-17).
			Rig.Advance(1.0);
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/party"), 200, PartyBody(TEXT("found"), true, 1.0))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/match-found"), 200, MatchFoundBody(TEXT("pending"), TEXT("pending"), 0))));
			ASSERT_THAT(IsTrue(Screen->GetShownScreen() == EVeyraShellScreen::MatchFound && !Screen->IsSettingsOpen()));
			ASSERT_THAT(IsFalse(OffersSettings(), TEXT("never in Match Found")));
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
