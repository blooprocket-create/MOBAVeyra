// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Blueprint/UserWidget.h"
#include "Components/ActorTestSpawner.h"
#include "GameFramework/PlayerState.h"
#include "Match/VeyraMatchMenu.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellModels.h"
#include "Shell/VeyraShellScreen.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "Shell/VeyraUIInputSettings.h"
#include "Tests/Services/VeyraClientFlowTestRig.h"
#include "UObject/Package.h"
#include "VeyraPlayerController.h"

namespace VeyraShellTests
{
	using namespace VeyraClientFlowTests;

	/** The labels of Buttons, in order. */
	TArray<FString> LabelsOf(const TArray<UVeyraShellButton*>& Buttons)
	{
		TArray<FString> Labels;
		for (const UVeyraShellButton* Button : Buttons)
		{
			Labels.Add(Button->GetLabel().ToString());
		}
		return Labels;
	}

	/** A select as its only player sees it, hovering or locking a Vanguard. */
	FVeyraClientSnapshot SelectSnapshot(const FString& Hover, const FString& Locked)
	{
		FVeyraClientSnapshot Snapshot;
		Snapshot.State = EVeyraClientState::Selecting;
		Snapshot.Select.Id = SelectId;
		Snapshot.Select.Mode = TEXT("custom_practice");
		Snapshot.Select.Seats.Add(VeyraBackendProtocol::FSelectSeat{ TEXT("DevOne"), TEXT("A"), true, Hover, Locked });
		Snapshot.AvailableVanguards = { TEXT("cairn"), TEXT("oriel") };
		return Snapshot;
	}

	// Veyra.UI.Shell.*: what the shell's screens show, from the coordinator's snapshot (ADR-010 §4).
	TEST_CLASS(Shell, "Veyra.UI")
	{
		TEST_METHOD(ScreenForState)
		{
			const TPair<EVeyraClientState, EVeyraShellScreen> Expected[] = {
				{ EVeyraClientState::SigningIn, EVeyraShellScreen::Status },
				{ EVeyraClientState::SignInFailed, EVeyraShellScreen::Stopped },
				{ EVeyraClientState::Loading, EVeyraShellScreen::Status },
				{ EVeyraClientState::StarterChoice, EVeyraShellScreen::StarterChoice },
				{ EVeyraClientState::Shell, EVeyraShellScreen::Shell },
				{ EVeyraClientState::Selecting, EVeyraShellScreen::ChampionSelect },
				{ EVeyraClientState::MatchStarting, EVeyraShellScreen::Status },
				{ EVeyraClientState::Connecting, EVeyraShellScreen::Status },
				{ EVeyraClientState::InMatch, EVeyraShellScreen::None },
				{ EVeyraClientState::Returning, EVeyraShellScreen::Status },
				{ EVeyraClientState::AwaitingResults, EVeyraShellScreen::Status },
				{ EVeyraClientState::Results, EVeyraShellScreen::Results },
				{ EVeyraClientState::ReconnectOnly, EVeyraShellScreen::ReconnectOnly },
				{ EVeyraClientState::SessionEnded, EVeyraShellScreen::Stopped },
			};
			ASSERT_THAT(AreEqual(static_cast<int32>(EVeyraClientState::SessionEnded) + 1, static_cast<int32>(UE_ARRAY_COUNT(Expected)), TEXT("every state has a screen")));
			for (const TPair<EVeyraClientState, EVeyraShellScreen>& Pair : Expected)
			{
				ASSERT_THAT(IsTrue(VeyraShellModels::ScreenFor(Pair.Key) == Pair.Value, LexToString(Pair.Key)));
			}
		}

		TEST_METHOD(SelectModel)
		{
			// A hover is tentative: Not Locked In, and Lock In would lock it (UX-35).
			FVeyraSelectModel Model = VeyraShellModels::DescribeSelect(SelectSnapshot(TEXT("oriel"), FString()), 27.2, true, true);
			ASSERT_THAT(AreEqual(Model.Countdown.ToString(), FString(TEXT("0:28"))));
			ASSERT_THAT(AreEqual(Model.Title.ToString(), FString(TEXT("Custom Practice: Champion Select"))));
			ASSERT_THAT(AreEqual(Model.Seats.Num(), 1));
			ASSERT_THAT(IsTrue(Model.Seats[0].Status == EVeyraSeatStatus::NotLockedIn && Model.Seats[0].bYou));
			ASSERT_THAT(AreEqual(Model.Seats[0].StatusText.ToString(), FString(TEXT("Not Locked In"))));
			ASSERT_THAT(AreEqual(Model.Seats[0].Vanguard.ToString(), FString(TEXT("Oriel"))));
			ASSERT_THAT(AreEqual(Model.Cards.Num(), 2));
			ASSERT_THAT(IsTrue(!Model.Cards[0].bChosen && Model.Cards[1].bChosen));
			ASSERT_THAT(AreEqual(Model.LockInVanguardId, FString(TEXT("oriel"))));
			ASSERT_THAT(IsTrue(Model.bCanLockIn));

			// With nothing hovered there is nothing to lock.
			Model = VeyraShellModels::DescribeSelect(SelectSnapshot(FString(), FString()), 30.0, true, true);
			ASSERT_THAT(IsTrue(Model.Seats[0].Status == EVeyraSeatStatus::Waiting && !Model.bCanLockIn));

			// A lock is final.
			Model = VeyraShellModels::DescribeSelect(SelectSnapshot(TEXT("oriel"), TEXT("oriel")), 12.0, false, false);
			ASSERT_THAT(IsTrue(Model.Seats[0].Status == EVeyraSeatStatus::LockedIn));
			ASSERT_THAT(IsTrue(Model.LockInVanguardId.IsEmpty() && !Model.bCanLockIn && !Model.bCanChoose));
			ASSERT_THAT(AreEqual(VeyraShellModels::FormatCountdown(-3.0).ToString(), FString(TEXT("0:00"))));
			ASSERT_THAT(AreEqual(VeyraShellModels::FormatCountdown(75.0).ToString(), FString(TEXT("1:15"))));
		}

		TEST_METHOD(ResultsModel)
		{
			FVeyraClientSnapshot Snapshot;
			Snapshot.State = EVeyraClientState::Results;
			FVeyraResultsModel Model = VeyraShellModels::DescribeResults(Snapshot);
			ASSERT_THAT(IsFalse(Model.bVerified, TEXT("no result is shown as pending, never made up")));

			VeyraBackendProtocol::FMatchOutcome Outcome;
			Outcome.MatchId = MatchId;
			Outcome.Mode = TEXT("custom_practice");
			Outcome.State = TEXT("ended");
			Outcome.VanguardId = TEXT("oriel");
			Outcome.bHasResult = true;
			Outcome.EndReason = TEXT("host_ended");
			Outcome.DurationSeconds = 42.5;
			Snapshot.Result = Outcome;
			Model = VeyraShellModels::DescribeResults(Snapshot);
			ASSERT_THAT(IsTrue(Model.bVerified));
			ASSERT_THAT(AreEqual(Model.Headline.ToString(), FString(TEXT("Match Over: No Winner"))));
			const FString Lines = FText::Join(FText::FromString(TEXT("|")), Model.Lines).ToString();
			ASSERT_THAT(IsTrue(Lines.Contains(TEXT("The host ended the match.")) && Lines.Contains(TEXT("Your Vanguard: Oriel"))
				&& Lines.Contains(TEXT("Mode: Custom Practice")) && Lines.Contains(TEXT("Duration: 0:43")), Lines));

			Outcome.bHasResult = false;
			Outcome.State = TEXT("failed");
			Outcome.FailureReason = TEXT("server_exited");
			Snapshot.Result = Outcome;
			Snapshot.Notice = TEXT("connection_lost");
			Model = VeyraShellModels::DescribeResults(Snapshot);
			ASSERT_THAT(AreEqual(Model.Headline.ToString(), FString(TEXT("The Match Did Not Finish"))));
			ASSERT_THAT(AreEqual(Model.Lines.Num(), 2, TEXT("why it failed, and why the player left it")));
		}

		TEST_METHOD(NamesAndTheSignature)
		{
			ASSERT_THAT(AreEqual(VeyraShellModels::NameOf(TEXT("custom_practice")).ToString(), FString(TEXT("Custom Practice"))));
			ASSERT_THAT(AreEqual(VeyraShellModels::NameOf(TEXT("cairn")).ToString(), FString(TEXT("Cairn"))));

			// A read that changes only the timer leaves the screen as it is.
			FVeyraClientSnapshot Snapshot = SelectSnapshot(FString(), FString());
			const FString Before = VeyraShellModels::Signature(Snapshot);
			Snapshot.PickEndsAt += 10.0;
			++Snapshot.Revision;
			ASSERT_THAT(AreEqual(VeyraShellModels::Signature(Snapshot), Before));
			Snapshot.Select.Seats[0].Hover = TEXT("cairn");
			ASSERT_THAT(IsFalse(VeyraShellModels::Signature(Snapshot) == Before));
		}

		TEST_METHOD(StyleSettings)
		{
			const TArray<FString> Problems = GetDefault<UVeyraShellStyleSettings>()->Validate();
			ASSERT_THAT(IsTrue(Problems.IsEmpty(), FString::Join(Problems, TEXT(" | "))));
			// A new object starts from the project's values, so these are cleared by hand.
			UVeyraShellStyleSettings* Missing = NewObject<UVeyraShellStyleSettings>(GetTransientPackage());
			Missing->BackgroundColor = FLinearColor::Transparent;
			Missing->MenuScrimColor = FLinearColor::Transparent;
			Missing->TitleFontSize = 0;
			Missing->CountdownFontSize = 0;
			Missing->CardWidth = 0.0f;
			Missing->MenuWidth = 0.0f;
			const FString Named = FString::Join(Missing->Validate(), TEXT(" | "));
			for (const TCHAR* Field : { TEXT("BackgroundColor"), TEXT("MenuScrimColor"), TEXT("TitleFontSize"), TEXT("CountdownFontSize"), TEXT("CardWidth"), TEXT("MenuWidth") })
			{
				ASSERT_THAT(IsTrue(Named.Contains(Field), FString::Printf(TEXT("%s is not named in: %s"), Field, *Named)));
			}
		}

		TEST_METHOD(InputSettings)
		{
			ASSERT_THAT(IsTrue(GetDefault<UVeyraUIInputSettings>()->Validate().IsEmpty()));
			UVeyraUIInputSettings* Missing = NewObject<UVeyraUIInputSettings>(GetTransientPackage());
			Missing->MatchMenuKey = FKey();
			ASSERT_THAT(IsFalse(Missing->Validate().IsEmpty()));
		}
	};

	// Veyra.UI.ShellScreens.*: the screens built in C++, bound to a coordinator on a fake backend, and
	// clicked as the player would click them.
	TEST_CLASS(ShellScreens, "Veyra.UI")
	{
		FActorTestSpawner Spawner;
		FClientFlowTestRig Rig;
		UVeyraShellScreen* Screen = nullptr;

		AFTER_EACH()
		{
			if (Screen)
			{
				Screen->Unbind();
			}
		}

		/** A screen showing the rig's coordinator. */
		UVeyraShellScreen& ShowScreen()
		{
			Screen = CreateWidget<UVeyraShellScreen>(&Spawner.GetWorld());
			Screen->Bind(*Rig.Flow);
			return *Screen;
		}

		UVeyraShellButton* Button(const TCHAR* Label) const
		{
			return Screen->FindButton(FText::FromString(Label));
		}

		TEST_METHOD(TheStarterChoiceOffersEachStarter)
		{
			ASSERT_THAT(IsTrue(Rig.ReachStarterChoice()));
			ShowScreen();
			ASSERT_THAT(IsTrue(Screen->GetShownScreen() == EVeyraShellScreen::StarterChoice));
			ASSERT_THAT(IsTrue(LabelsOf(Screen->GetButtons()) == (TArray<FString>{ TEXT("Cairn"), TEXT("Qazharr"), TEXT("Oriel"), TEXT("Bryn") })));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("The tutorial is coming later"))));
			Button(TEXT("Oriel"))->Press();
			ASSERT_THAT(AreEqual(Rig.Backend.Find(TEXT("POST"), TEXT("/v1/me/starter"))->Body, FString(TEXT("{\"vanguardId\":\"oriel\"}"))));
			ASSERT_THAT(IsFalse(Button(TEXT("Cairn"))->GetIsEnabled(), TEXT("one choice at a time")));
		}

		TEST_METHOD(HomeLeadsToPlayAndPractice)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			ShowScreen();
			ASSERT_THAT(IsTrue(Screen->GetShownScreen() == EVeyraShellScreen::Shell && Screen->GetPage() == EVeyraShellPage::Home));
			ASSERT_THAT(IsTrue(Screen->IsFocusable(), TEXT("the shell's input mode gives the screen keyboard focus")));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Welcome, DevOne."))));
			Button(TEXT("Play"))->Press();
			ASSERT_THAT(IsTrue(Screen->GetPage() == EVeyraShellPage::Play));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Custom"))));
			Button(TEXT("Practice"))->Press();
			ASSERT_THAT(IsNotNull(Rig.Backend.Find(TEXT("POST"), TEXT("/v1/practice"))));
			ASSERT_THAT(IsFalse(Button(TEXT("Practice"))->GetIsEnabled(), TEXT("practice is starting")));
		}

		TEST_METHOD(ChampionSelectOwnsTheScreenAndLocksTheHover)
		{
			ASSERT_THAT(IsTrue(Rig.ReachSelect()));
			ShowScreen();
			ASSERT_THAT(IsTrue(Screen->GetShownScreen() == EVeyraShellScreen::ChampionSelect));
			// No navigation leaves a committed select (UX-4).
			ASSERT_THAT(IsTrue(LabelsOf(Screen->GetButtons()) == (TArray<FString>{ TEXT("Cairn"), TEXT("Qazharr"), TEXT("Oriel"), TEXT("Bryn"), TEXT("Lock In") })));
			ASSERT_THAT(IsFalse(Button(TEXT("Lock In"))->GetIsEnabled(), TEXT("nothing is hovered yet")));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("DevOne (you): Waiting"))));

			Button(TEXT("Oriel"))->Press();
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("PUT"), TEXT("/v1/me/select/hover"), 200, SelectBody(TEXT("picking"), TEXT("oriel")))));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("DevOne (you): Oriel, Not Locked In"))));
			ASSERT_THAT(IsTrue(Button(TEXT("Lock In"))->GetIsEnabled()));
			Button(TEXT("Lock In"))->Press();
			ASSERT_THAT(AreEqual(Rig.Backend.Find(TEXT("POST"), TEXT("/v1/me/select/lock"))->Body, FString(TEXT("{\"vanguardId\":\"oriel\"}"))));
		}

		TEST_METHOD(APollThatChangesNothingKeepsTheButtons)
		{
			ASSERT_THAT(IsTrue(Rig.ReachSelect()));
			ShowScreen();
			const UVeyraShellButton* Before = Button(TEXT("Oriel"));
			Rig.Advance(0.5);
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200, SelectBody(TEXT("picking"), FString(), FString(), FString(), FString(), 29.0))));
			ASSERT_THAT(IsTrue(Button(TEXT("Oriel")) == Before, TEXT("a click in progress is not interrupted")));
		}

		TEST_METHOD(ReconnectOnlyOffersNothingButReconnect)
		{
			ASSERT_THAT(IsTrue(Rig.ReachReconnectOnly()));
			ShowScreen();
			ASSERT_THAT(IsTrue(Screen->GetShownScreen() == EVeyraShellScreen::ReconnectOnly));
			ASSERT_THAT(IsTrue(LabelsOf(Screen->GetButtons()) == TArray<FString>{ TEXT("Reconnect") }));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Match in Progress"))));
			Button(TEXT("Reconnect"))->Press();
			ASSERT_THAT(IsNotNull(Rig.Backend.Find(TEXT("GET"), TEXT("/v1/me/match"))));
		}

		TEST_METHOD(ResultsShowTheVerifiedResultThenContinue)
		{
			ASSERT_THAT(IsTrue(Rig.ReachResults()));
			ShowScreen();
			ASSERT_THAT(IsTrue(Screen->GetShownScreen() == EVeyraShellScreen::Results));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("The host ended the match.")), Screen->DescribeText()));
			Button(TEXT("Continue"))->Press();
			ASSERT_THAT(IsTrue(Rig.State() == EVeyraClientState::Loading));
			ASSERT_THAT(IsTrue(Screen->GetShownScreen() == EVeyraShellScreen::Status));
		}

		TEST_METHOD(AProblemOffersRetry)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("VeyraClientFlow: problem in Loading (backend_unreachable)"), ELogVerbosity::Warning,
				EAutomationExpectedMessageFlags::Contains, 1);
			ASSERT_THAT(IsTrue(Rig.SignIn()));
			ShowScreen();
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 0)));
			Rig.Advance(1.0);
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 0)));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Veyra's services did not answer."))));
			Button(TEXT("Retry"))->Press();
			ASSERT_THAT(IsNotNull(Rig.Backend.Find(TEXT("GET"), TEXT("/v1/me/match"))));
			ASSERT_THAT(IsNull(Button(TEXT("Retry"))));
		}

		TEST_METHOD(ASignInFailureOffersOnlyQuit)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("VeyraClientFlow: signing in failed"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
			Rig.Flow->Start();
			Rig.Host.bInputClosed = true;
			Rig.Flow->Tick();
			ShowScreen();
			ASSERT_THAT(IsTrue(Screen->GetShownScreen() == EVeyraShellScreen::Stopped));
			ASSERT_THAT(IsTrue(LabelsOf(Screen->GetButtons()) == TArray<FString>{ TEXT("Quit") }));
			Button(TEXT("Quit"))->Press();
			ASSERT_THAT(IsTrue(Rig.Host.bQuit));
		}
	};

	// Veyra.UI.MatchMenu.*: the in-match menu (ADR-010 §4).
	TEST_CLASS(MatchMenu, "Veyra.UI")
	{
		FActorTestSpawner Spawner;

		TEST_METHOD(EndCustomMatchVisibility)
		{
			const APlayerState& Host = Spawner.SpawnActor<APlayerState>();
			const APlayerState& Other = Spawner.SpawnActor<APlayerState>();
			ASSERT_THAT(IsTrue(VeyraMatchMenuModel::CanEndCustomMatch(EVeyraMatchRules::Practice, &Host, &Host)));
			ASSERT_THAT(IsFalse(VeyraMatchMenuModel::CanEndCustomMatch(EVeyraMatchRules::Practice, &Host, &Other), TEXT("only the host")));
			ASSERT_THAT(IsFalse(VeyraMatchMenuModel::CanEndCustomMatch(EVeyraMatchRules::Standard, &Host, &Host), TEXT("only practice")));
			ASSERT_THAT(IsFalse(VeyraMatchMenuModel::CanEndCustomMatch(EVeyraMatchRules::Practice, nullptr, &Host), TEXT("before the host joins")));
		}

		TEST_METHOD(OutsidePracticeTheMenuOffersResume)
		{
			AVeyraPlayerController& Controller = Spawner.SpawnActor<AVeyraPlayerController>();
			UVeyraMatchMenu* Menu = CreateWidget<UVeyraMatchMenu>(&Spawner.GetWorld());
			bool bClosed = false;
			Menu->Show(Controller, [&bClosed] { bClosed = true; });
			ASSERT_THAT(IsTrue(LabelsOf(Menu->GetButtons()) == TArray<FString>{ TEXT("Resume") }));
			ASSERT_THAT(IsTrue(Menu->IsFocusable(), TEXT("the open menu's input mode gives it keyboard focus")));
			Menu->FindButton(FText::FromString(TEXT("Resume")))->Press();
			ASSERT_THAT(IsTrue(bClosed));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
