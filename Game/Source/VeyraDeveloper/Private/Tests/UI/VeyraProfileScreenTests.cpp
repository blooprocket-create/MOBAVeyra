// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Blueprint/UserWidget.h"
#include "Components/ActorTestSpawner.h"
#include "Shell/VeyraConductModels.h"
#include "Shell/VeyraProfileModels.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellModels.h"
#include "Shell/VeyraShellScreen.h"
#include "Tests/Services/VeyraClientFlowTestRig.h"

namespace VeyraProfileScreenTests
{
	using namespace VeyraClientFlowTests;
	using namespace VeyraProfileModels;

	// Fixture answers, independent of the committed backend configuration.
	FString ShownProfile(const TCHAR* Name, bool bShares)
	{
		return FString::Printf(TEXT("{\"profile\":{\"name\":\"%s\",\"icon\":\"vanguard_cairn\",\"background\":\"vanguard_cairn\",\"level\":12,")
								   TEXT("\"featured\":{\"vanguardId\":\"cairn\",\"masteryLevel\":4},\"sharesMatchHistory\":%s}}"),
			Name, bShares ? TEXT("true") : TEXT("false"));
	}

	FString OwnChoices(const TCHAR* Icon = TEXT("default"), const TCHAR* FeaturedJson = TEXT("null"), bool bShows = false)
	{
		return FString::Printf(TEXT("{\"settings\":{\"icon\":\"%s\",\"background\":\"default\",\"featuredVanguardId\":%s,\"showMatchHistory\":%s},")
								   TEXT("\"catalog\":{\"icons\":[\"default\",\"vanguard_cairn\",\"vanguard_oriel\"],\"backgrounds\":[\"default\",\"vanguard_cairn\"],")
								   TEXT("\"defaultIcon\":\"default\",\"defaultBackground\":\"default\",\"featuredChoices\":[\"cairn\"]}}"),
			Icon, FeaturedJson, bShows ? TEXT("true") : TEXT("false"));
	}

	const TCHAR* const MenuConduct = TEXT("{\"conduct\":{\"reported\":[],\"commended\":null,\"players\":[{\"name\":\"DevTwo\",\"teammate\":false}],")
									 TEXT("\"reasons\":[\"afk\",\"other\"],\"detailsMaxCharacters\":500}}");

	// Veyra.UI.ProfileScreen.*: the Profile page's choices and Save, and another player's profile from the friends
	// card and the player menu, with its shared Match History (ADR-048 §5), clicked as the player would click them.
	TEST_CLASS(ProfileScreen, "Veyra.UI")
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

		void Show()
		{
			Screen = CreateWidget<UVeyraShellScreen>(&Spawner.GetWorld());
			Screen->Bind(*Rig.Flow);
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

		TEST_METHOD(TheProfilePageChoosesAndSaves)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			Show();
			ASSERT_THAT(IsTrue(Press(PageLabel())));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/profile-settings"), 200, OwnChoices())));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/profiles/DevOne"), 200, ShownProfile(TEXT("DevOne"), false))));
			ASSERT_THAT(IsTrue(Screen->GetPage() == EVeyraShellPage::Profile && Screen->DescribeText().Contains(TEXT("How others see you"))));
			// Nothing to save until a choice changes.
			ASSERT_THAT(IsTrue(Offers(SaveLabel()) && !Screen->FindButton(SaveLabel())->GetIsEnabled()));
			// Only the Vanguards the player owns may be featured, or none.
			ASSERT_THAT(IsTrue(Offers(FeatureLabel(FString())) && Offers(FeatureLabel(TEXT("cairn"))) && !Offers(FeatureLabel(TEXT("oriel")))));
			ASSERT_THAT(IsTrue(Press(IconLabel(TEXT("vanguard_oriel")))));
			// The card previews the choice before it is saved, over the confirmed level and Mastery.
			const TOptional<FVeyraProfileCardModel> Preview = DescribePage(Rig.Flow->GetSnapshot(), Screen->GetProfileDraft()).Preview;
			ASSERT_THAT(IsTrue(Preview.IsSet() && Preview->IconVanguard == TEXT("oriel") && Preview->Level.ToString() == TEXT("Level 12")));
			ASSERT_THAT(IsTrue(Press(FeatureLabel(TEXT("cairn")))));
			ASSERT_THAT(IsTrue(Press(ShareHistoryLabel())));
			const VeyraBackendProtocol::FProfileSettings& Draft = Screen->GetProfileDraft();
			ASSERT_THAT(IsTrue(Draft.Icon == TEXT("vanguard_oriel") && Draft.FeaturedVanguardId == TEXT("cairn") && Draft.bShowMatchHistory));
			ASSERT_THAT(IsTrue(Press(SaveLabel())));
			const FFlowTestBackend::FRequest* Request = Rig.Backend.Find(TEXT("PUT"), TEXT("/v1/me/profile-settings"));
			ASSERT_THAT(IsTrue(Request && Request->Body.Contains(TEXT("\"icon\":\"vanguard_oriel\"")) && Request->Body.Contains(TEXT("\"showMatchHistory\":true"))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("PUT"), TEXT("/v1/me/profile-settings"), 200, OwnChoices(TEXT("vanguard_oriel"), TEXT("\"cairn\""), true))));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Your profile is saved."))));
			ASSERT_THAT(IsFalse(Screen->FindButton(SaveLabel())->GetIsEnabled(), TEXT("saved: nothing left to save")));
		}

		TEST_METHOD(AFriendsProfileOpensFromTheirCard)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell() && Rig.ReadSocial()));
			Show();
			ASSERT_THAT(IsTrue(Press(VeyraShellModels::FriendCardLabel(TEXT("DevTwo")))));
			ASSERT_THAT(IsTrue(Press(ViewProfileLabel(TEXT("DevTwo")))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/profiles/DevTwo"), 200, ShownProfile(TEXT("DevTwo"), false))));
			const FString Text = Screen->DescribeText();
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("Level 12")) && Text.Contains(TEXT("Mastery Level 4")) && Text.Contains(TEXT("DevTwo's Match History is private.")), Text));
			ASSERT_THAT(IsTrue(Press(CloseLabel())));
			ASSERT_THAT(IsFalse(Offers(CloseLabel()), TEXT("closed")));
		}

		TEST_METHOD(AnUnavailableProfileSaysOnlyThat)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			Show();
			ASSERT_THAT(IsTrue(Rig.Flow->OpenProfile(TEXT("DevTwo"))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/profiles/DevTwo"), 404, ErrorBody(TEXT("profile_unavailable")))));
			const FString Text = Screen->DescribeText();
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("This profile is unavailable.")) && !Text.Contains(TEXT("Level 12")), Text));
		}

		TEST_METHOD(ThePlayerMenuOpensAProfileAndItsSharedMatches)
		{
			ASSERT_THAT(IsTrue(Rig.ReachResults(ScoredOutcomeBody())));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), FString(TEXT("/v1/me/matches/")) + MatchId + TEXT("/conduct"), 200, MenuConduct)));
			Show();
			ASSERT_THAT(IsTrue(Press(VeyraConductModels::MenuLabel(TEXT("DevTwo")))));
			ASSERT_THAT(IsTrue(Press(MenuProfileLabel(TEXT("DevTwo")))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/profiles/DevTwo"), 200, ShownProfile(TEXT("DevTwo"), true))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/profiles/DevTwo/matches"), 200, HistoryBody({ HistoryEntry(OlderMatchId, TEXT("loss")) }, TEXT("null")))));
			const TArray<FVeyraHistoryRow> Rows = VeyraProfileModels::DescribeView(Rig.Flow->GetSnapshot(), false).Rows;
			ASSERT_THAT(AreEqual(1, Rows.Num()));
			// The owner's own filters read the shared history again (ADR-048 §3).
			ASSERT_THAT(IsTrue(Press(VeyraMatchHistoryModel::OutcomeText(TEXT("loss")))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/profiles/DevTwo/matches?outcome=loss"), 200, HistoryBody({ HistoryEntry(OlderMatchId, TEXT("loss")) }, TEXT("null")))));
			ASSERT_THAT(IsTrue(Rig.Flow->GetSnapshot().ProfileView.Filter.Outcome == TEXT("loss")));
			ASSERT_THAT(IsTrue(Press(FText::Format(FText::FromString(TEXT("Open {0}")), Rows[0].Summary))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), FString(TEXT("/v1/profiles/DevTwo/matches/")) + OlderMatchId, 200, ScoredOutcomeBody())));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Back to Match History"))));
		}
	};
}

#endif
