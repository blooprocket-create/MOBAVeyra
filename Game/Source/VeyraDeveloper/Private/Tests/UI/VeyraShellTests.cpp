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
#include "Text/VeyraContentText.h"
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
				{ EVeyraClientState::MatchFound, EVeyraShellScreen::MatchFound },
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
			FVeyraSelectModel Model = VeyraShellModels::DescribeSelect(SelectSnapshot(TEXT("oriel"), FString()), 27.2, true, true, false);
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
			Model = VeyraShellModels::DescribeSelect(SelectSnapshot(FString(), FString()), 30.0, true, true, false);
			ASSERT_THAT(IsTrue(Model.Seats[0].Status == EVeyraSeatStatus::Waiting && !Model.bCanLockIn));

			// A lock is final.
			Model = VeyraShellModels::DescribeSelect(SelectSnapshot(TEXT("oriel"), TEXT("oriel")), 12.0, false, false, false);
			ASSERT_THAT(IsTrue(Model.Seats[0].Status == EVeyraSeatStatus::LockedIn));
			ASSERT_THAT(IsTrue(Model.LockInVanguardId.IsEmpty() && !Model.bCanLockIn && !Model.bCanChoose));
			ASSERT_THAT(AreEqual(VeyraShellModels::FormatCountdown(-3.0).ToString(), FString(TEXT("0:00"))));
			ASSERT_THAT(AreEqual(VeyraShellModels::FormatCountdown(75.0).ToString(), FString(TEXT("1:15"))));
		}

		TEST_METHOD(TeamSelectModel)
		{
			// A matchmade select: the enemy team apart, its locks visible and unique, and Leave offered.
			FVeyraClientSnapshot Snapshot = SelectSnapshot(TEXT("oriel"), FString());
			Snapshot.Select.Kind = TEXT("casual");
			Snapshot.Select.Mode = TEXT("casual_select");
			Snapshot.Select.Seats.Add(VeyraBackendProtocol::FSelectSeat{ TEXT("DevTwo"), TEXT("B"), false, FString(), TEXT("cairn") });
			FVeyraSelectModel Model = VeyraShellModels::DescribeSelect(Snapshot, 50.0, true, true, true);
			ASSERT_THAT(IsTrue(Model.bTeams));
			ASSERT_THAT(IsTrue(Model.Seats[0].bAlly && !Model.Seats[1].bAlly));
			ASSERT_THAT(AreEqual(Model.Seats[1].StatusText.ToString(), FString(TEXT("Locked In"))));
			ASSERT_THAT(AreEqual(Model.Seats[1].Vanguard.ToString(), FString(TEXT("Cairn"))));
			ASSERT_THAT(IsTrue(Model.Cards[0].VanguardId == TEXT("cairn") && Model.Cards[0].bTaken, TEXT("another player locked Cairn")));
			ASSERT_THAT(IsFalse(Model.Cards[1].bTaken));
			ASSERT_THAT(IsTrue(Model.bCanLockIn, TEXT("the player's own hover is free")));
			ASSERT_THAT(IsTrue(Model.bOffersLeave && Model.bCanLeave));

			// A hover on a Vanguard someone else locks cannot be locked in.
			Snapshot.Select.Seats[0].Hover = TEXT("cairn");
			Model = VeyraShellModels::DescribeSelect(Snapshot, 49.0, true, true, true);
			ASSERT_THAT(IsFalse(Model.bCanLockIn));

			// Once every pick is in, there is nothing left to leave.
			Snapshot.Select.State = VeyraBackendProtocol::ESelectState::Starting;
			ASSERT_THAT(IsFalse(VeyraShellModels::DescribeSelect(Snapshot, 0.0, false, false, false).bOffersLeave));

			// Practice has one team, and no Leave.
			Model = VeyraShellModels::DescribeSelect(SelectSnapshot(FString(), FString()), 30.0, true, true, false);
			ASSERT_THAT(IsFalse(Model.bTeams || Model.bOffersLeave));
		}

		TEST_METHOD(MatchFoundModel)
		{
			FVeyraClientSnapshot Snapshot;
			Snapshot.State = EVeyraClientState::MatchFound;
			Snapshot.MatchFound.Id = FoundId;
			Snapshot.MatchFound.Mode = CasualMode;
			Snapshot.MatchFound.State = TEXT("pending");
			Snapshot.MatchFound.You = TEXT("pending");
			Snapshot.MatchFound.Accepted = 1;
			Snapshot.MatchFound.Total = 2;
			FVeyraMatchFoundModel Model = VeyraShellModels::DescribeMatchFound(Snapshot, 14.2, true);
			ASSERT_THAT(AreEqual(Model.Title.ToString(), FString(TEXT("Match Found"))));
			ASSERT_THAT(AreEqual(Model.Mode.ToString(), FString(TEXT("Casual Select"))));
			ASSERT_THAT(AreEqual(Model.Countdown.ToString(), FString(TEXT("0:15"))));
			ASSERT_THAT(AreEqual(Model.Progress.ToString(), FString(TEXT("1 of 2 accepted"))));
			ASSERT_THAT(IsTrue(Model.Phase.ToString().StartsWith(TEXT("Accept to play")) && Model.bCanAnswer));

			Snapshot.MatchFound.You = TEXT("accepted");
			Model = VeyraShellModels::DescribeMatchFound(Snapshot, 9.0, false);
			ASSERT_THAT(AreEqual(Model.Phase.ToString(), FString(TEXT("Accepted. Waiting for the other players."))));
			ASSERT_THAT(IsFalse(Model.bCanAnswer));

			// How a match found ended reads by the player's own answer; nobody learns who declined.
			ASSERT_THAT(IsTrue(VeyraShellModels::DescribeNotice(TEXT("match_found_declined")).ToString().Contains(TEXT("Your party left the queue"))));
			ASSERT_THAT(IsTrue(VeyraShellModels::DescribeNotice(TEXT("match_found_missed")).ToString().Contains(TEXT("not accepted in time"))));
			ASSERT_THAT(IsTrue(VeyraShellModels::DescribeNotice(TEXT("match_found_abandoned")).ToString().StartsWith(TEXT("Someone in your party did not accept"))));
			ASSERT_THAT(IsTrue(VeyraShellModels::DescribeNotice(TEXT("match_found_requeued")).ToString().Contains(TEXT("You are back in the queue"))));
			// A block between two players ends a match found or a select, and nobody is told of it (§6).
			ASSERT_THAT(IsFalse(VeyraShellModels::DescribeNotice(TEXT("match_found_requeued")).ToString().Contains(TEXT("player"))));
			ASSERT_THAT(IsTrue(VeyraShellModels::DescribeNotice(TEXT("no_longer_matched")).ToString().Contains(TEXT("can no longer go ahead"))));
			ASSERT_THAT(AreEqual(VeyraShellModels::DescribeProblem(FVeyraClientProblem{ TEXT("member_busy"), TEXT("raw"), false }).ToString(),
				FString(TEXT("Someone in your party is still in a match or champion select."))));
		}

		TEST_METHOD(PartyAndModeModels)
		{
			FVeyraClientSnapshot Snapshot;
			Snapshot.State = EVeyraClientState::Shell;
			Snapshot.AccountId = AccountId;
			ASSERT_THAT(IsFalse(VeyraShellModels::DescribeParty(Snapshot, false, false, false).bShown, TEXT("no party, no panel")));

			// Only enabled modes show; one without a matchmaker says so (UX-12).
			FString Problem;
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseModes(ModesBody, Snapshot.Modes, Problem), Problem));
			Snapshot.Modes.Add(VeyraBackendProtocol::FModeInfo{ TEXT("ranked"), false, 5, false });
			TOptional<VeyraBackendProtocol::FParty> Party;
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseParty(PartyBody(TEXT("idle"), false), Party, Problem), Problem));
			Snapshot.Party = Party;
			const TArray<FVeyraModeCardModel> Cards = VeyraShellModels::DescribeModes(Snapshot);
			ASSERT_THAT(AreEqual(Cards.Num(), 2));
			ASSERT_THAT(IsTrue(Cards[0].bAvailable && Cards[0].bSelected && Cards[0].Availability.IsEmpty()));
			ASSERT_THAT(AreEqual(Cards[0].Format.ToString(), FString(TEXT("1v1"))));
			ASSERT_THAT(IsTrue(!Cards[1].bAvailable && !Cards[1].bSelected));
			ASSERT_THAT(AreEqual(Cards[1].Availability.ToString(), FString(TEXT("Not yet available"))));

			// The leader of a party that is not Ready yet.
			FVeyraPartyModel Model = VeyraShellModels::DescribeParty(Snapshot, true, false, false);
			ASSERT_THAT(IsTrue(Model.bShown && !Model.bQueued));
			ASSERT_THAT(AreEqual(Model.Mode.ToString(), FString(TEXT("Mode: Casual Select"))));
			ASSERT_THAT(AreEqual(Model.Members[0].ToString(), FString(TEXT("DevOne (you, leader): Not Ready"))));
			ASSERT_THAT(AreEqual(Model.Status.ToString(), FString(TEXT("Find Match opens once everyone is Ready."))));
			ASSERT_THAT(IsTrue(Model.bReadyTarget && Model.bCanReady));
			ASSERT_THAT(AreEqual(Model.ReadyLabel.ToString(), FString(TEXT("Ready"))));
			ASSERT_THAT(IsTrue(Model.bOffersFindMatch && !Model.bCanFindMatch && !Model.bOffersCancel));

			// Queued: the time shows instead, only the leader may cancel, and Ready is locked (§2).
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseParty(PartyBody(TEXT("queued"), true, 3.0), Party, Problem), Problem));
			Snapshot.Party = Party;
			Model = VeyraShellModels::DescribeParty(Snapshot, false, false, true);
			ASSERT_THAT(IsTrue(Model.bQueued && Model.Status.IsEmpty()));
			ASSERT_THAT(IsTrue(!Model.bOffersFindMatch && Model.bOffersCancel && Model.bCanCancel));
			ASSERT_THAT(AreEqual(Model.ReadyLabel.ToString(), FString(TEXT("Unready"))));
			ASSERT_THAT(AreEqual(VeyraShellModels::FormatQueueStatus(67.9).ToString(), FString(TEXT("In queue: 1:07. Estimate unavailable."))));

			// A member who does not lead sees the leader, and neither Find Match nor Cancel.
			Snapshot.AccountId = TEXT("66666666-7777-4888-8999-aaaaaaaaaaaa");
			Model = VeyraShellModels::DescribeParty(Snapshot, false, false, false);
			ASSERT_THAT(AreEqual(Model.Members[0].ToString(), FString(TEXT("DevOne (leader): Ready"))));
			ASSERT_THAT(IsFalse(Model.bOffersFindMatch || Model.bOffersCancel));
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

		TEST_METHOD(ResultsModelVictory)
		{
			VeyraBackendProtocol::FMatchOutcome Outcome;
			Outcome.MatchId = MatchId;
			Outcome.Mode = TEXT("casual_select");
			Outcome.State = TEXT("ended");
			Outcome.Side = TEXT("A");
			Outcome.bHasResult = true;
			Outcome.EndReason = TEXT("prime_well_destroyed");
			Outcome.Winner = TEXT("A");
			FVeyraClientSnapshot Snapshot;
			Snapshot.State = EVeyraClientState::Results;
			Snapshot.Result = Outcome;
			FVeyraResultsModel Model = VeyraShellModels::DescribeResults(Snapshot);
			ASSERT_THAT(AreEqual(Model.Headline.ToString(), FString(TEXT("Victory"))));
			ASSERT_THAT(IsTrue(FText::Join(FText::FromString(TEXT("|")), Model.Lines).ToString().Contains(TEXT("A Prime Well was destroyed."))));

			Outcome.Side = TEXT("B");
			Snapshot.Result = Outcome;
			Model = VeyraShellModels::DescribeResults(Snapshot);
			ASSERT_THAT(AreEqual(Model.Headline.ToString(), FString(TEXT("Defeat"))));
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

		TEST_METHOD(PlayQueuesTheParty)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/modes"), 200, ModesBody)));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/party"), 200, NoParty)));
			ShowScreen();
			Button(TEXT("Play"))->Press();
			// Mode cards: one to choose, one not yet available; no party panel before a mode is chosen.
			ASSERT_THAT(IsTrue(Button(TEXT("Casual Select"))->GetIsEnabled()));
			ASSERT_THAT(IsFalse(Button(TEXT("Draft Pick"))->GetIsEnabled()));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Not yet available"))));
			ASSERT_THAT(IsNull(Button(TEXT("Ready"))));

			Button(TEXT("Casual Select"))->Press();
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("PUT"), TEXT("/v1/party/mode"), 200, PartyBody(TEXT("idle"), false))));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("DevOne (you, leader): Not Ready")), Screen->DescribeText()));
			ASSERT_THAT(IsFalse(Button(TEXT("Find Match"))->GetIsEnabled(), TEXT("everyone must be Ready")));
			Button(TEXT("Ready"))->Press();
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("PUT"), TEXT("/v1/party/ready"), 200, PartyBody(TEXT("idle"), true))));
			ASSERT_THAT(IsNotNull(Button(TEXT("Unready"))));
			Button(TEXT("Find Match"))->Press();
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("POST"), TEXT("/v1/party/queue"), 200, PartyBody(TEXT("queued"), true))));

			// Queued: the time shows, Practice waits, and the leader may cancel.
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Estimate unavailable.")), Screen->DescribeText()));
			ASSERT_THAT(IsFalse(Button(TEXT("Practice"))->GetIsEnabled()));
			ASSERT_THAT(IsNull(Button(TEXT("Find Match"))));
			Button(TEXT("Cancel"))->Press();
			ASSERT_THAT(IsNotNull(Rig.Backend.Find(TEXT("DELETE"), TEXT("/v1/party/queue"))));
		}

		TEST_METHOD(MatchFoundBlocksTheShell)
		{
			ASSERT_THAT(IsTrue(Rig.ReachMatchFound()));
			ShowScreen();
			// Only Accept and Decline: no navigation, party controls or Quit until it is answered (UX §5).
			ASSERT_THAT(IsTrue(Screen->GetShownScreen() == EVeyraShellScreen::MatchFound));
			ASSERT_THAT(IsTrue(LabelsOf(Screen->GetButtons()) == (TArray<FString>{ TEXT("Accept"), TEXT("Decline") })));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("0 of 2 accepted"))));
			Button(TEXT("Accept"))->Press();
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("POST"), TEXT("/v1/me/match-found/accept"), 200, MatchFoundBody(TEXT("pending"), TEXT("accepted"), 1))));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Accepted. Waiting for the other players."))));
			ASSERT_THAT(IsFalse(Button(TEXT("Decline"))->GetIsEnabled()));
		}

		TEST_METHOD(ACasualSelectShowsTheTeamsAndOffersLeave)
		{
			ASSERT_THAT(IsTrue(Rig.ReachCasualSelect()));
			ShowScreen();
			const FString Text = Screen->DescribeText();
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("Your Team")) && Text.Contains(TEXT("Enemy Team")) && Text.Contains(TEXT("DevTwo: Waiting")), Text));
			ASSERT_THAT(IsTrue(Button(TEXT("Leave"))->GetIsEnabled()));
			Button(TEXT("Leave"))->Press();
			ASSERT_THAT(IsNotNull(Rig.Backend.Find(TEXT("POST"), TEXT("/v1/me/select/leave"))));
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

	// Veyra.UI.ContentText.*: what players read about Vanguards, abilities and passives (ADR-010 §4).
	TEST_CLASS(ContentText, "Veyra.UI")
	{
		static FVeyraContentId IdOf(const TCHAR* Text)
		{
			return FVeyraContentId::FromText(Text).GetValue();
		}

		TEST_METHOD(EveryReleasedVanguardHasItsText)
		{
			const TArray<FString> Missing = VeyraContentText::FindMissingPlayableText();
			ASSERT_THAT(IsTrue(Missing.IsEmpty(), FString::Printf(TEXT("Game/Text/VeyraText.csv lacks %s"), *FString::Join(Missing, TEXT(", ")))));
		}

		TEST_METHOD(NamesComeFromTheTableAndDeveloperContentShowsItsId)
		{
			const FText Name = VeyraContentText::AbilityName(IdOf(TEXT("qazharr_heavy_hand")));
			ASSERT_THAT(IsTrue(Name.IsFromStringTable()));
			ASSERT_THAT(AreEqual(Name.ToString(), FString(TEXT("Heavy Hand"))));
			ASSERT_THAT(AreEqual(VeyraContentText::VanguardTitle(IdOf(TEXT("qazharr"))).ToString(), FString(TEXT("The Harbor Wolf"))));
			// A quoted field keeps its commas.
			ASSERT_THAT(IsTrue(VeyraContentText::AbilityDescription(IdOf(TEXT("cairn_immovable"))).ToString().Contains(TEXT("less far, but walks slower"))));
			ASSERT_THAT(AreEqual(VeyraShellModels::VanguardNameOf(TEXT("qazharr")).ToString(), FString(TEXT("Qazharr"))));

			// The developer test Vanguard has no text: its ID stands in, and it has no title or description.
			ASSERT_THAT(AreEqual(VeyraContentText::VanguardName(IdOf(TEXT("test_vanguard"))).ToString(), FString(TEXT("test_vanguard"))));
			ASSERT_THAT(IsTrue(VeyraContentText::VanguardTitle(IdOf(TEXT("test_vanguard"))).IsEmpty()));
			ASSERT_THAT(IsTrue(VeyraContentText::AbilityDescription(IdOf(TEXT("test_bolt"))).IsEmpty()));
			ASSERT_THAT(AreEqual(VeyraContentText::AbilityName(IdOf(TEXT("test_bolt"))).ToString(), FString(TEXT("test_bolt"))));
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

		TEST_METHOD(DeveloperEndVisibility)
		{
			// A standard match has no victory condition yet: outside Shipping a developer may end it.
			ASSERT_THAT(AreEqual(VeyraMatchMenuModel::OffersDeveloperEnd(EVeyraMatchRules::Standard), !UE_BUILD_SHIPPING));
			ASSERT_THAT(IsFalse(VeyraMatchMenuModel::OffersDeveloperEnd(EVeyraMatchRules::Practice), TEXT("practice has End Custom Match")));
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
