// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Client/VeyraClientFlow.h"

#include "Backend/VeyraProfileProtocol.h"

// Player profiles (ADR-048). The backend owns every profile, its choices and who may read them; the flow names
// players by display name and never by account.

namespace
{
	const TCHAR* const ProfileSettingsPath = TEXT("/v1/me/profile-settings");
	/** The Profile page's feedback after a save went through. */
	const TCHAR* const ProfileSavedFeedback = TEXT("profile_saved");

	/** The backend's refusal code, or "http_<status>" when it gave none. */
	FString ProfileRefusalCode(const FVeyraBackendResponse& Response)
	{
		const FString Code = VeyraBackendProtocol::ParseErrorCode(Response.Body);
		return Code.IsEmpty() ? FString::Printf(TEXT("http_%d"), Response.Status) : Code;
	}

	bool ProfileRefusedWith(const FVeyraBackendResponse& Response, const TCHAR* Code)
	{
		return !Response.IsSuccess() && VeyraBackendProtocol::ParseErrorCode(Response.Body).Equals(Code, ESearchCase::CaseSensitive);
	}
}

bool FVeyraClientFlow::OpenProfile(const FString& Name)
{
	const FString Trimmed = Name.TrimStartAndEnd();
	if (!CanIssue(EVeyraClientIntent::OpenProfile) || Trimmed.IsEmpty())
	{
		return false;
	}
	Snapshot.ProfileView = FVeyraProfileView();
	Snapshot.ProfileView.Name = Trimmed;
	Log(TEXT("opening a profile."));
	Broadcast();
	ReadProfile(Trimmed);
	return true;
}

bool FVeyraClientFlow::CloseProfile()
{
	if (!CanIssue(EVeyraClientIntent::CloseProfile) || Snapshot.ProfileView.Name.IsEmpty())
	{
		return false;
	}
	Snapshot.ProfileView = FVeyraProfileView();
	Broadcast();
	return true;
}

void FVeyraClientFlow::ReadProfile(const FString& Name)
{
	Call(EVerb::Get, VeyraBackendProtocol::ProfilePath(Name), FString(), [this, Name](const FVeyraBackendResponse& Response) {
		FVeyraProfileView& View = Snapshot.ProfileView;
		if (View.Name != Name)
		{
			return;
		}
		VeyraBackendProtocol::FPublicProfile Profile;
		FString Problem;
		// An unknown name and a block either way answer alike, and the screen says only that (ADR-048 §3).
		if (ProfileRefusedWith(Response, TEXT("profile_unavailable")))
		{
			View.bLoaded = View.bUnavailable = true;
			Broadcast();
			return;
		}
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the profile"), [this, Name] { ReadProfile(Name); });
			return;
		}
		if (!VeyraBackendProtocol::ParsePublicProfile(Response.Body, Profile, Problem))
		{
			ShowBadAnswer(TEXT("the profile"), Problem, [this, Name] { ReadProfile(Name); });
			return;
		}
		View.Profile = MoveTemp(Profile);
		View.bLoaded = true;
		Broadcast();
		if (View.Profile.bSharesMatchHistory)
		{
			ReadProfileMatches(Name, View.Filter, FString());
		}
	});
}

void FVeyraClientFlow::ReadProfileMatches(const FString& Name, const VeyraBackendProtocol::FHistoryFilter& Filter, const FString& Cursor)
{
	Snapshot.ProfileView.bReadingMatches = true;
	Call(EVerb::Get, VeyraBackendProtocol::ProfileMatchesPath(Name, Filter, Cursor), FString(), [this, Name, Filter, Cursor](const FVeyraBackendResponse& Response) {
		FVeyraProfileView& View = Snapshot.ProfileView;
		// Another profile, or other filters, since it was asked for: a newer read answers instead.
		if (View.Name != Name || !(View.Filter == Filter))
		{
			return;
		}
		View.bReadingMatches = false;
		VeyraBackendProtocol::FHistoryPage Page;
		FString Problem;
		// The owner may stop sharing, or block, after the profile was read: the history then shows nothing.
		if (ProfileRefusedWith(Response, TEXT("history_private")) || ProfileRefusedWith(Response, TEXT("profile_unavailable")))
		{
			View.bMatchesLoaded = true;
			View.Matches.Reset();
			View.Next.Reset();
			View.Profile.bSharesMatchHistory = false;
			Broadcast();
			return;
		}
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the profile's Match History"), [this, Name, Filter, Cursor] { ReadProfileMatches(Name, Filter, Cursor); });
			return;
		}
		if (!VeyraBackendProtocol::ParseHistoryPage(Response.Body, Page, Problem))
		{
			ShowBadAnswer(TEXT("the profile's Match History"), Problem, [this, Name, Filter, Cursor] { ReadProfileMatches(Name, Filter, Cursor); });
			return;
		}
		if (Cursor.IsEmpty())
		{
			View.Matches.Reset();
		}
		View.Matches.Append(MoveTemp(Page.Entries));
		View.Next = Page.Next;
		View.Modes = MoveTemp(Page.Modes);
		View.bMatchesLoaded = true;
		Broadcast();
	});
}

bool FVeyraClientFlow::LoadMoreProfileMatches()
{
	const FVeyraProfileView& View = Snapshot.ProfileView;
	// One page at a time: a second Load More before the first page arrives asks for nothing.
	if (!CanIssue(EVeyraClientIntent::LoadMoreProfileMatches) || View.Name.IsEmpty() || View.Next.IsEmpty() || View.OpenedMatch.IsSet() || View.bReadingMatches)
	{
		return false;
	}
	ReadProfileMatches(View.Name, View.Filter, View.Next);
	return true;
}

bool FVeyraClientFlow::FilterProfileMatches(const VeyraBackendProtocol::FHistoryFilter& Filter)
{
	FVeyraProfileView& View = Snapshot.ProfileView;
	if (!CanIssue(EVeyraClientIntent::FilterProfileMatches) || View.Name.IsEmpty() || !View.Profile.bSharesMatchHistory || View.OpenedMatch.IsSet())
	{
		return false;
	}
	View.Filter = Filter;
	View.Matches.Reset();
	View.Next.Reset();
	View.bMatchesLoaded = false;
	Broadcast();
	ReadProfileMatches(View.Name, Filter, FString());
	return true;
}

bool FVeyraClientFlow::OpenProfileMatch(const FString& MatchId)
{
	const FVeyraProfileView& View = Snapshot.ProfileView;
	const bool bListed = View.Matches.ContainsByPredicate([&MatchId](const VeyraBackendProtocol::FHistoryEntry& Entry) { return Entry.MatchId == MatchId; });
	if (!CanIssue(EVeyraClientIntent::OpenProfileMatch) || View.Name.IsEmpty() || !bListed)
	{
		return false;
	}
	const FString Name = View.Name;
	Call(EVerb::Get, VeyraBackendProtocol::ProfileMatchPath(Name, MatchId), FString(), [this, Name, MatchId](const FVeyraBackendResponse& Response) {
		FVeyraProfileView& Opened = Snapshot.ProfileView;
		if (Opened.Name != Name)
		{
			return;
		}
		VeyraBackendProtocol::FMatchOutcome Outcome;
		FString Problem;
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the shared match"), [this, MatchId] { OpenProfileMatch(MatchId); });
			return;
		}
		if (!VeyraBackendProtocol::ParseMatchOutcome(Response.Body, Outcome, Problem))
		{
			ShowBadAnswer(TEXT("the shared match"), Problem, [this, MatchId] { OpenProfileMatch(MatchId); });
			return;
		}
		Opened.OpenedMatch = MoveTemp(Outcome);
		Broadcast();
	});
	return true;
}

bool FVeyraClientFlow::CloseProfileMatch()
{
	if (!CanIssue(EVeyraClientIntent::CloseProfileMatch) || !Snapshot.ProfileView.OpenedMatch.IsSet())
	{
		return false;
	}
	Snapshot.ProfileView.OpenedMatch.Reset();
	Broadcast();
	return true;
}

bool FVeyraClientFlow::LoadProfileSettings()
{
	if (!CanIssue(EVeyraClientIntent::LoadProfileSettings))
	{
		return false;
	}
	Call(EVerb::Get, ProfileSettingsPath, FString(), [this](const FVeyraBackendResponse& Response) {
		VeyraBackendProtocol::FProfileSettings Settings;
		VeyraBackendProtocol::FProfileCatalog Catalog;
		FString Problem;
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("your profile"), [this] { LoadProfileSettings(); });
			return;
		}
		if (!VeyraBackendProtocol::ParseProfileSettings(Response.Body, Settings, Catalog, Problem))
		{
			ShowBadAnswer(TEXT("your profile"), Problem, [this] { LoadProfileSettings(); });
			return;
		}
		FVeyraProfileSettings& Own = Snapshot.ProfileSettings;
		Own.Saved = MoveTemp(Settings);
		Own.Catalog = MoveTemp(Catalog);
		Own.bLoaded = true;
		Broadcast();
	});
	ReadProfilePreview();
	return true;
}

void FVeyraClientFlow::ReadProfilePreview()
{
	if (Snapshot.DisplayName.IsEmpty())
	{
		return;
	}
	// A read that must never stop the page: a failure keeps the last preview.
	Probe(EVerb::Get, VeyraBackendProtocol::ProfilePath(Snapshot.DisplayName), [this](const FVeyraBackendResponse& Response) {
		VeyraBackendProtocol::FPublicProfile Profile;
		FString Problem;
		if (Response.IsSuccess() && VeyraBackendProtocol::ParsePublicProfile(Response.Body, Profile, Problem))
		{
			Snapshot.ProfileSettings.Preview = MoveTemp(Profile);
			Broadcast();
		}
	});
}

bool FVeyraClientFlow::SaveProfileSettings(const VeyraBackendProtocol::FProfileSettings& Settings)
{
	if (!CanIssue(EVeyraClientIntent::SaveProfileSettings) || !Snapshot.ProfileSettings.bLoaded)
	{
		return false;
	}
	Log(TEXT("saving the profile."));
	SetBusy(true);
	Call(EVerb::Put, ProfileSettingsPath, VeyraBackendProtocol::BuildProfileSettingsBody(Settings), [this](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		FVeyraProfileSettings& Own = Snapshot.ProfileSettings;
		VeyraBackendProtocol::FProfileSettings Saved;
		VeyraBackendProtocol::FProfileCatalog Catalog;
		FString Problem;
		if (!Response.IsSuccess())
		{
			// A refusal shows on the Profile page, never as the screen's problem.
			Own.Feedback = ProfileRefusalCode(Response);
			Log(FString::Printf(TEXT("profile: %s."), *Own.Feedback));
			Broadcast();
			return;
		}
		if (!VeyraBackendProtocol::ParseProfileSettings(Response.Body, Saved, Catalog, Problem))
		{
			ShowBadAnswer(TEXT("your profile"), Problem, [this] { LoadProfileSettings(); });
			return;
		}
		Own.Saved = MoveTemp(Saved);
		Own.Catalog = MoveTemp(Catalog);
		Own.Feedback = ProfileSavedFeedback;
		Log(TEXT("profile: profile_saved."));
		Broadcast();
		ReadProfilePreview();
	});
	return true;
}
