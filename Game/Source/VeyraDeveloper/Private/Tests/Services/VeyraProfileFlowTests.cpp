// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "Backend/VeyraProfileProtocol.h"
#include "Tests/Services/VeyraClientFlowTestRig.h"

namespace VeyraClientFlowTests
{
	// Fixture answers, independent of the committed backend configuration.
	inline FString ProfileAnswer(const TCHAR* Name, bool bShares, const TCHAR* FeaturedJson = TEXT("{\"vanguardId\":\"cairn\",\"masteryLevel\":4}"))
	{
		return FString::Printf(TEXT("{\"profile\":{\"name\":\"%s\",\"icon\":\"vanguard_cairn\",\"background\":\"default\",\"level\":12,\"featured\":%s,")
								   TEXT("\"sharesMatchHistory\":%s}}"),
			Name, FeaturedJson, bShares ? TEXT("true") : TEXT("false"));
	}

	inline FString ProfileSettingsAnswer(const TCHAR* Icon = TEXT("default"), const TCHAR* FeaturedJson = TEXT("null"), bool bShows = false)
	{
		return FString::Printf(TEXT("{\"settings\":{\"icon\":\"%s\",\"background\":\"default\",\"featuredVanguardId\":%s,\"showMatchHistory\":%s},")
								   TEXT("\"catalog\":{\"icons\":[\"default\",\"vanguard_cairn\"],\"backgrounds\":[\"default\",\"vanguard_cairn\"],")
								   TEXT("\"defaultIcon\":\"default\",\"defaultBackground\":\"default\",\"featuredChoices\":[\"cairn\"]}}"),
			Icon, FeaturedJson, bShows ? TEXT("true") : TEXT("false"));
	}

	inline const TCHAR* const ProfileOfDevTwo = TEXT("/v1/profiles/DevTwo");

	// Veyra.Services.ProfileFlow.*: profiles opened by name, their shared Match History, and the player's own
	// choices (ADR-048), driven through the fake backend as the screens drive them.
	TEST_CLASS(ProfileFlow, "Veyra.Services")
	{
		FClientFlowTestRig Rig;
		FFlowTestBackend& Backend = Rig.Backend;

		const FVeyraClientSnapshot& Snapshot() const { return Rig.Flow->GetSnapshot(); }

		TEST_METHOD(AProfileOpensByNameWithItsFeaturedVanguard)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			ASSERT_THAT(IsTrue(Rig.Flow->OpenProfile(TEXT("  DevTwo "))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), ProfileOfDevTwo, 200, ProfileAnswer(TEXT("DevTwo"), false))));
			const FVeyraProfileView& View = Snapshot().ProfileView;
			ASSERT_THAT(IsTrue(View.bLoaded && !View.bUnavailable && View.Name == TEXT("DevTwo") && View.Profile.Level == 12 && View.Profile.Icon == TEXT("vanguard_cairn")));
			ASSERT_THAT(IsTrue(View.Profile.Featured.IsSet() && View.Profile.Featured->VanguardId == TEXT("cairn") && View.Profile.Featured->MasteryLevel == 4));
			// Private history is never asked for.
			ASSERT_THAT(IsNull(Backend.Find(TEXT("GET"), FString(ProfileOfDevTwo) + TEXT("/matches"))));
			ASSERT_THAT(IsTrue(Rig.Flow->CloseProfile() && Snapshot().ProfileView.Name.IsEmpty()));
		}

		TEST_METHOD(AnUnavailableProfileSaysOnlyThat)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			ASSERT_THAT(IsTrue(Rig.Flow->OpenProfile(TEXT("DevTwo"))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), ProfileOfDevTwo, 404, ErrorBody(TEXT("profile_unavailable")))));
			ASSERT_THAT(IsTrue(Snapshot().ProfileView.bLoaded && Snapshot().ProfileView.bUnavailable && !Snapshot().Problem.IsSet()));
		}

		TEST_METHOD(ASharedHistoryPagesAndOpensItsMatches)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			ASSERT_THAT(IsTrue(Rig.Flow->OpenProfile(TEXT("DevTwo"))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), ProfileOfDevTwo, 200, ProfileAnswer(TEXT("DevTwo"), true, TEXT("null")))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), FString(ProfileOfDevTwo) + TEXT("/matches"), 200,
				HistoryBody({ HistoryEntry(MatchId, TEXT("win")) }, TEXT("\"cursor_1\"")))));
			const FVeyraProfileView& View = Snapshot().ProfileView;
			ASSERT_THAT(IsTrue(View.bMatchesLoaded && View.Matches.Num() == 1 && View.Next == TEXT("cursor_1") && !View.Profile.Featured.IsSet()));
			ASSERT_THAT(IsTrue(Rig.Flow->LoadMoreProfileMatches()));
			ASSERT_THAT(IsFalse(Rig.Flow->LoadMoreProfileMatches(), TEXT("one page at a time: the next is on its way")));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), FString(ProfileOfDevTwo) + TEXT("/matches?cursor=cursor_1"), 200,
				HistoryBody({ HistoryEntry(OlderMatchId, TEXT("loss")) }, TEXT("null")))));
			ASSERT_THAT(IsTrue(View.Matches.Num() == 2 && View.Next.IsEmpty()));
			ASSERT_THAT(IsNull(Backend.Find(TEXT("GET"), FString(ProfileOfDevTwo) + TEXT("/matches?cursor=cursor_1")), TEXT("asked for once")));

			// The owner's own filters: the first page again, with them (ADR-048 §3).
			VeyraBackendProtocol::FHistoryFilter Filter;
			Filter.VanguardId = TEXT("cairn");
			Filter.Outcome = TEXT("loss");
			ASSERT_THAT(IsTrue(Rig.Flow->FilterProfileMatches(Filter)));
			ASSERT_THAT(IsTrue(View.Matches.IsEmpty() && !View.bMatchesLoaded));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), FString(ProfileOfDevTwo) + TEXT("/matches?vanguard=cairn&outcome=loss"), 200,
				HistoryBody({ HistoryEntry(OlderMatchId, TEXT("loss")) }, TEXT("null")))));
			ASSERT_THAT(IsTrue(View.Matches.Num() == 1 && View.Filter == Filter));
			ASSERT_THAT(IsFalse(Rig.Flow->OpenProfileMatch(TEXT("not-listed"))));
			ASSERT_THAT(IsTrue(Rig.Flow->OpenProfileMatch(OlderMatchId)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), FString(ProfileOfDevTwo) + TEXT("/matches/") + OlderMatchId, 200, ScoredOutcomeBody())));
			ASSERT_THAT(IsTrue(View.OpenedMatch.IsSet() && View.OpenedMatch->bHasScoreboard));
			ASSERT_THAT(IsTrue(Rig.Flow->CloseProfileMatch() && !View.OpenedMatch.IsSet()));
		}

		TEST_METHOD(AnOwnerWhoStopsSharingShowsNoHistory)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			ASSERT_THAT(IsTrue(Rig.Flow->OpenProfile(TEXT("DevTwo"))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), ProfileOfDevTwo, 200, ProfileAnswer(TEXT("DevTwo"), true))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), FString(ProfileOfDevTwo) + TEXT("/matches"), 403, ErrorBody(TEXT("history_private")))));
			const FVeyraProfileView& View = Snapshot().ProfileView;
			ASSERT_THAT(IsTrue(View.bMatchesLoaded && View.Matches.IsEmpty() && !View.Profile.bSharesMatchHistory && !Snapshot().Problem.IsSet()));
		}

		TEST_METHOD(TheProfilePageReadsAndSavesTheChoices)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			ASSERT_THAT(IsFalse(Rig.Flow->SaveProfileSettings({}), TEXT("nothing to save before the choices are read")));
			ASSERT_THAT(IsTrue(Rig.Flow->LoadProfileSettings()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/profile-settings"), 200, ProfileSettingsAnswer())));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/profiles/DevOne"), 200, ProfileAnswer(TEXT("DevOne"), false, TEXT("null")))));
			const FVeyraProfileSettings& Own = Snapshot().ProfileSettings;
			ASSERT_THAT(IsTrue(Own.bLoaded && Own.Saved.Icon == TEXT("default") && Own.Saved.FeaturedVanguardId.IsEmpty() && !Own.Saved.bShowMatchHistory));
			ASSERT_THAT(IsTrue(Own.Catalog.Icons.Num() == 2 && Own.Catalog.FeaturedChoices == TArray<FString>{ TEXT("cairn") } && Own.Preview.IsSet()));

			VeyraBackendProtocol::FProfileSettings Choice;
			Choice.Icon = TEXT("vanguard_cairn");
			Choice.Background = TEXT("default");
			Choice.FeaturedVanguardId = TEXT("cairn");
			Choice.bShowMatchHistory = true;
			ASSERT_THAT(IsTrue(Rig.Flow->SaveProfileSettings(Choice)));
			const FFlowTestBackend::FRequest* Request = Backend.Find(TEXT("PUT"), TEXT("/v1/me/profile-settings"));
			ASSERT_THAT(IsTrue(Request && Request->Body.Contains(TEXT("\"featuredVanguardId\":\"cairn\"")) && Request->Body.Contains(TEXT("\"showMatchHistory\":true"))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("PUT"), TEXT("/v1/me/profile-settings"), 200, ProfileSettingsAnswer(TEXT("vanguard_cairn"), TEXT("\"cairn\""), true))));
			ASSERT_THAT(IsTrue(Own.Feedback == TEXT("profile_saved") && Own.Saved == Choice));
			ASSERT_THAT(IsNotNull(Backend.Find(TEXT("GET"), TEXT("/v1/profiles/DevOne")), TEXT("the preview is read again")));

			// A refusal shows on the page, never as the screen's problem.
			ASSERT_THAT(IsTrue(Rig.Flow->SaveProfileSettings(Choice)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("PUT"), TEXT("/v1/me/profile-settings"), 409, ErrorBody(TEXT("not_owned")))));
			ASSERT_THAT(IsTrue(Own.Feedback == TEXT("not_owned") && !Snapshot().Problem.IsSet()));
		}

		TEST_METHOD(ProfilesOpenFromTheShellALobbyAndTheResults)
		{
			for (const EVeyraClientIntent Intent : { EVeyraClientIntent::OpenProfile, EVeyraClientIntent::OpenProfileMatch, EVeyraClientIntent::LoadMoreProfileMatches,
					 EVeyraClientIntent::FilterProfileMatches })
			{
				for (const EVeyraClientState Where : { EVeyraClientState::Shell, EVeyraClientState::Lobby, EVeyraClientState::Results })
				{
					ASSERT_THAT(IsTrue(FVeyraClientFlow::IsIntentAllowed(Where, Intent), LexToString(Intent)));
				}
				for (const EVeyraClientState Elsewhere : { EVeyraClientState::MatchFound, EVeyraClientState::Selecting, EVeyraClientState::InMatch,
						 EVeyraClientState::ReconnectOnly })
				{
					ASSERT_THAT(IsFalse(FVeyraClientFlow::IsIntentAllowed(Elsewhere, Intent), LexToString(Intent)));
				}
			}
			ASSERT_THAT(IsFalse(FVeyraClientFlow::IsIntentAllowed(EVeyraClientState::Results, EVeyraClientIntent::SaveProfileSettings), TEXT("choices on the Profile page only")));
		}
	};

	// Veyra.Services.ProfileProtocol.*: profiles and the player's choices as the client reads them, and a save as the
	// backend reads it (ADR-048 §3–§4).
	TEST_CLASS(ProfileProtocol, "Veyra.Services")
	{
		TEST_METHOD(AProfileAndTheChoicesAreReadAndMalformedOnesRefused)
		{
			VeyraBackendProtocol::FPublicProfile Profile;
			VeyraBackendProtocol::FProfileSettings Settings;
			VeyraBackendProtocol::FProfileCatalog Catalog;
			FString Problem;
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParsePublicProfile(ProfileAnswer(TEXT("Dev Two"), true), Profile, Problem), Problem));
			ASSERT_THAT(IsTrue(Profile.Name == TEXT("Dev Two") && Profile.bSharesMatchHistory && Profile.Featured.IsSet()));
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseProfileSettings(ProfileSettingsAnswer(), Settings, Catalog, Problem), Problem));
			for (const TCHAR* Bad : {
					 TEXT("{\"profile\":{\"name\":\"X\",\"icon\":\"Not An Icon\",\"background\":\"default\",\"level\":1,\"featured\":null,\"sharesMatchHistory\":false}}"),
					 TEXT("{\"profile\":{\"name\":\"X\",\"icon\":\"default\",\"background\":\"default\",\"level\":0,\"featured\":null,\"sharesMatchHistory\":false}}"),
					 TEXT("{\"profile\":{\"name\":\"X\",\"icon\":\"default\",\"background\":\"default\",\"level\":1,\"featured\":{\"vanguardId\":\"cairn\"},\"sharesMatchHistory\":false}}"),
					 TEXT("{\"profile\":{\"name\":\"\",\"icon\":\"default\",\"background\":\"default\",\"level\":1,\"featured\":null,\"sharesMatchHistory\":false}}"),
					 TEXT("{}") })
			{
				ASSERT_THAT(IsFalse(VeyraBackendProtocol::ParsePublicProfile(Bad, Profile, Problem), Bad));
			}
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::ParseProfileSettings(TEXT("{\"settings\":{\"icon\":\"default\"}}"), Settings, Catalog, Problem)));
		}

		TEST_METHOD(PathsEscapeTheNameAndASaveWritesNoneAsNull)
		{
			ASSERT_THAT(AreEqual(VeyraBackendProtocol::ProfilePath(TEXT("Dev Two")), FString(TEXT("/v1/profiles/Dev%20Two"))));
			ASSERT_THAT(AreEqual(VeyraBackendProtocol::ProfileMatchesPath(TEXT("DevTwo"), {}, TEXT("c 1")), FString(TEXT("/v1/profiles/DevTwo/matches?cursor=c%201"))));
			VeyraBackendProtocol::FHistoryFilter Filter;
			Filter.Mode = TEXT("casual");
			ASSERT_THAT(AreEqual(VeyraBackendProtocol::ProfileMatchesPath(TEXT("DevTwo"), Filter, FString()), FString(TEXT("/v1/profiles/DevTwo/matches?mode=casual"))));
			VeyraBackendProtocol::FProfileSettings Settings;
			Settings.Icon = TEXT("default");
			Settings.Background = TEXT("default");
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::BuildProfileSettingsBody(Settings).Contains(TEXT("\"featuredVanguardId\":null"))));
		}
	};
}

#endif
