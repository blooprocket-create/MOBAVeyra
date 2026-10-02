// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Blueprint/UserWidget.h"
#include "Components/ActorTestSpawner.h"
#include "Shell/VeyraConductModels.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellScreen.h"
#include "Tests/Services/VeyraClientFlowTestRig.h"

namespace VeyraPlayerMenuScreenTests
{
	using namespace VeyraClientFlowTests;
	using namespace VeyraConductModels;

	// Fixture answers, independent of the committed backend configuration: DevTwo an opponent, DevThree a teammate.
	FString MenuConductAnswer(const TCHAR* ReportedJson = TEXT("[]"), const TCHAR* CommendedJson = TEXT("null"))
	{
		return FString::Printf(TEXT("{\"conduct\":{\"reported\":%s,\"commended\":%s,\"players\":[{\"name\":\"DevTwo\",\"teammate\":false},")
								   TEXT("{\"name\":\"DevThree\",\"teammate\":true}],\"reasons\":[\"abusive_chat\",\"afk\",\"other\"],\"detailsMaxCharacters\":500}}"),
			ReportedJson, CommendedJson);
	}

	FString MenuRoute(const TCHAR* Leaf)
	{
		return FString::Printf(TEXT("/v1/me/matches/%s/%s"), MatchId, Leaf);
	}

	/** The scored match with DevThree playing beside the player, where the bot stood. */
	FString TeamOutcomeBody()
	{
		return ScoredOutcomeBody().Replace(TEXT("\"side\":\"B\",\"name\":\"Bot 1\""), TEXT("\"side\":\"A\",\"name\":\"DevThree\""));
	}

	// Veyra.UI.PlayerMenuScreen.*: the results screen's and a Match History record's player menus, the report form,
	// Commend once, and Play Again (UX-57, UX-58, UX-62; ADR-047 §5), clicked as the player would click them.
	TEST_CLASS(PlayerMenuScreen, "Veyra.UI")
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

		/** The results of Outcome over the coordinator, with the player's conduct record read. */
		bool ShowResults(const FString& Outcome)
		{
			if (!Rig.ReachResults(Outcome) || !Rig.Backend.Answer(TEXT("GET"), MenuRoute(TEXT("conduct")), 200, MenuConductAnswer()))
			{
				return false;
			}
			Screen = CreateWidget<UVeyraShellScreen>(&Spawner.GetWorld());
			Screen->Bind(*Rig.Flow);
			return true;
		}

		bool Press(const FText& Label)
		{
			UVeyraShellButton* Found = Screen->FindButton(Label);
			if (!Found || !Found->GetIsEnabled())
			{
				return false;
			}
			Found->Press();
			return true;
		}

		bool Offers(const FText& Label) const { return Screen->FindButton(Label) != nullptr; }

		TEST_METHOD(OnlyAnotherHumansLineOpensAMenu)
		{
			ASSERT_THAT(IsTrue(ShowResults(ScoredOutcomeBody())));
			ASSERT_THAT(IsTrue(Offers(MenuLabel(TEXT("DevTwo")))));
			ASSERT_THAT(IsFalse(Offers(MenuLabel(TEXT("DevOne"))), TEXT("the player's own line")));
			ASSERT_THAT(IsFalse(Offers(MenuLabel(TEXT("Bot 1"))), TEXT("a bot's")));
			ASSERT_THAT(IsTrue(Press(MenuLabel(TEXT("DevTwo")))));
			ASSERT_THAT(AreEqual(Screen->GetOpenPlayerMenu(), FString(TEXT("DevTwo"))));
			// Not a friend, and an opponent: Add Friend and Report, never Invite or Commend.
			ASSERT_THAT(IsTrue(Offers(AddFriendLabel(TEXT("DevTwo"))) && Offers(ReportLabel(TEXT("DevTwo")))));
			ASSERT_THAT(IsFalse(Offers(InviteLabel(TEXT("DevTwo"))) || Offers(CommendLabel(TEXT("DevTwo")))));
			ASSERT_THAT(IsTrue(Press(MenuLabel(TEXT("DevTwo")))));
			ASSERT_THAT(IsTrue(Screen->GetOpenPlayerMenu().IsEmpty() && !Offers(ReportLabel(TEXT("DevTwo"))), TEXT("closed again")));
		}

		TEST_METHOD(AReportNeedsAReasonAndThenSaysOnlyReportSent)
		{
			ASSERT_THAT(IsTrue(ShowResults(ScoredOutcomeBody())));
			ASSERT_THAT(IsTrue(Press(MenuLabel(TEXT("DevTwo")))));
			ASSERT_THAT(IsTrue(Press(ReportLabel(TEXT("DevTwo")))));
			// The backend's reasons, and no Submit before one is chosen.
			for (const TCHAR* Reason : { TEXT("abusive_chat"), TEXT("afk"), TEXT("other") })
			{
				ASSERT_THAT(IsTrue(Offers(ReasonButtonLabel(TEXT("DevTwo"), Reason)), Reason));
			}
			ASSERT_THAT(IsFalse(Press(SubmitReportLabel(TEXT("DevTwo")))));
			ASSERT_THAT(IsTrue(Press(ReasonButtonLabel(TEXT("DevTwo"), TEXT("afk")))));
			// The details hold no more than the backend accepts.
			Screen->SetReportDetailsDraft(FString::ChrN(600, TEXT('x')));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(DetailsCount(500, 500).ToString())));
			Screen->SetReportDetailsDraft(TEXT("stood in base"));
			ASSERT_THAT(IsTrue(Press(SubmitReportLabel(TEXT("DevTwo")))));
			const FFlowTestBackend::FRequest* Request = Rig.Backend.Find(TEXT("POST"), MenuRoute(TEXT("reports")));
			ASSERT_THAT(IsNotNull(Request));
			ASSERT_THAT(IsTrue(Request->Body.Contains(TEXT("\"reason\":\"afk\"")) && Request->Body.Contains(TEXT("\"details\":\"stood in base\""))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("POST"), MenuRoute(TEXT("reports")), 200, TEXT("{\"report\":{\"reportedName\":\"DevTwo\",\"status\":\"received\"}}"))));
			// The card says so, and shows nothing more.
			const FString Text = Screen->DescribeText();
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("Report sent")), Text));
			ASSERT_THAT(IsFalse(Offers(ReportLabel(TEXT("DevTwo"))) || Offers(AddFriendLabel(TEXT("DevTwo")))));
		}

		TEST_METHOD(ATeammateIsCommendedOnce)
		{
			ASSERT_THAT(IsTrue(ShowResults(TeamOutcomeBody())));
			ASSERT_THAT(IsTrue(Press(MenuLabel(TEXT("DevThree")))));
			ASSERT_THAT(IsTrue(Press(CommendLabel(TEXT("DevThree")))));
			const FFlowTestBackend::FRequest* Request = Rig.Backend.Find(TEXT("POST"), MenuRoute(TEXT("commendation")));
			ASSERT_THAT(IsTrue(Request && Request->Body.Contains(TEXT("\"name\":\"DevThree\""))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("POST"), MenuRoute(TEXT("commendation")), 200, TEXT("{\"commendation\":{\"name\":\"DevThree\"}}"))));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Commended"))));
			ASSERT_THAT(IsFalse(Offers(CommendLabel(TEXT("DevThree"))), TEXT("one commendation per match")));
		}

		TEST_METHOD(AddFriendAsksByTheNameTheScoreboardShows)
		{
			ASSERT_THAT(IsTrue(ShowResults(ScoredOutcomeBody())));
			ASSERT_THAT(IsTrue(Press(MenuLabel(TEXT("DevTwo")))));
			ASSERT_THAT(IsTrue(Press(AddFriendLabel(TEXT("DevTwo")))));
			ASSERT_THAT(IsNotNull(Rig.Backend.Find(TEXT("GET"), VeyraBackendProtocol::AccountLookupPath(TEXT("DevTwo")))));
		}

		TEST_METHOD(PlayAgainLeavesForPlayAndChangesNothing)
		{
			ASSERT_THAT(IsTrue(ShowResults(ScoredOutcomeBody())));
			ASSERT_THAT(IsTrue(Press(PlayAgainLabel())));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 200, NoMatch)));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200, NoSelect)));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/profile"), 200, ProfileBody(true))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/lobby"), 200, NoLobby)));
			ASSERT_THAT(IsTrue(Rig.State() == EVeyraClientState::Shell && Screen->GetPage() == EVeyraShellPage::Play));
			// It readies, queues and chooses nothing (UX-62).
			for (const TCHAR* Path : { TEXT("/v1/party/ready"), TEXT("/v1/party/queue"), TEXT("/v1/party/mode") })
			{
				ASSERT_THAT(IsTrue(Rig.Backend.Find(TEXT("PUT"), Path) == nullptr && Rig.Backend.Find(TEXT("POST"), Path) == nullptr, Path));
			}
		}

		TEST_METHOD(AMatchHistoryRecordOffersReportButNeverCommend)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			Screen = CreateWidget<UVeyraShellScreen>(&Spawner.GetWorld());
			Screen->Bind(*Rig.Flow);
			ASSERT_THAT(IsTrue(Press(FText::FromString(TEXT("Match History")))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/matches"), 200, HistoryBody({ HistoryEntry(MatchId, TEXT("win")) }, TEXT("null")))));
			ASSERT_THAT(IsTrue(Press(FText::FromString(TEXT("Open")))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), MatchOutcomePath(), 200, TeamOutcomeBody())));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), MenuRoute(TEXT("conduct")), 200, MenuConductAnswer())));
			ASSERT_THAT(IsTrue(Press(MenuLabel(TEXT("DevThree")))));
			ASSERT_THAT(IsTrue(Offers(ReportLabel(TEXT("DevThree"))) && Offers(AddFriendLabel(TEXT("DevThree")))));
			ASSERT_THAT(IsFalse(Offers(CommendLabel(TEXT("DevThree"))), TEXT("a teammate, but not on the results screen")));
		}
	};
}

#endif
