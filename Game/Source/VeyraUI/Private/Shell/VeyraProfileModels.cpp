// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraProfileModels.h"

#include "Misc/StringBuilder.h"
#include "Shell/VeyraShellModels.h"

#define LOCTEXT_NAMESPACE "VeyraProfileModels"

namespace VeyraProfileModels
{
namespace
{
	/** Catalog entries drawn from a released Vanguard's art (ADR-048 §2). */
	const TCHAR* const VanguardEntryPrefix = TEXT("vanguard_");

	FText EntryName(const FString& Entry)
	{
		const FString Vanguard = VanguardOfEntry(Entry);
		return Vanguard.IsEmpty() ? LOCTEXT("DefaultEntry", "Default") : VeyraShellModels::VanguardNameOf(Vanguard);
	}

	FText FeedbackText(const FString& Code)
	{
		if (Code.IsEmpty())
		{
			return FText::GetEmpty();
		}
		if (Code == TEXT("profile_saved"))
		{
			return LOCTEXT("Saved", "Your profile is saved.");
		}
		if (Code == TEXT("not_owned"))
		{
			return LOCTEXT("NotOwned", "Only a Vanguard you own can be featured.");
		}
		if (Code == TEXT("invalid_icon") || Code == TEXT("invalid_background"))
		{
			return LOCTEXT("NotOffered", "That choice is no longer offered.");
		}
		return FText::Format(LOCTEXT("SaveFailed", "Your profile was not saved ({0})."), FText::FromString(Code));
	}
}

FString VanguardOfEntry(const FString& Entry)
{
	return Entry.StartsWith(VanguardEntryPrefix, ESearchCase::CaseSensitive) ? Entry.RightChop(FCString::Strlen(VanguardEntryPrefix)) : FString();
}

FVeyraProfileCardModel DescribeCard(const VeyraBackendProtocol::FPublicProfile& Profile)
{
	FVeyraProfileCardModel Card;
	Card.Name = FText::FromString(Profile.Name);
	Card.IconVanguard = VanguardOfEntry(Profile.Icon);
	Card.BackgroundVanguard = VanguardOfEntry(Profile.Background);
	Card.Level = FText::Format(LOCTEXT("Level", "Level {0}"), FText::AsNumber(Profile.Level));
	// One chosen, permanently owned Vanguard with its base art and Mastery, or a neutral state (UX-71, UX-74).
	if (Profile.Featured.IsSet())
	{
		Card.FeaturedVanguard = Profile.Featured->VanguardId;
		Card.Featured = FText::Format(LOCTEXT("Featured", "{0}, Mastery Level {1}"), VeyraShellModels::VanguardNameOf(Profile.Featured->VanguardId),
			FText::AsNumber(Profile.Featured->MasteryLevel));
	}
	else
	{
		Card.Featured = LOCTEXT("NoFeatured", "No featured Vanguard");
	}
	return Card;
}

FVeyraProfileViewModel DescribeView(const FVeyraClientSnapshot& Snapshot, bool bCanLoadMore)
{
	const FVeyraProfileView& View = Snapshot.ProfileView;
	FVeyraProfileViewModel Model;
	Model.bOpen = !View.Name.IsEmpty();
	if (!Model.bOpen)
	{
		return Model;
	}
	if (!View.bLoaded)
	{
		Model.Status = LOCTEXT("Reading", "Reading the profile...");
		return Model;
	}
	// An unknown name and a block either way read alike, so a block is never revealed (ADR-048 §3).
	if (View.bUnavailable)
	{
		Model.Status = LOCTEXT("Unavailable", "This profile is unavailable.");
		return Model;
	}
	Model.Card = DescribeCard(View.Profile);
	if (View.OpenedMatch.IsSet())
	{
		Model.Opened = VeyraShellModels::DescribeOutcome(*View.OpenedMatch);
		return Model;
	}
	if (!View.Profile.bSharesMatchHistory)
	{
		Model.HistoryNote = FText::Format(LOCTEXT("Private", "{0}'s Match History is private."), FText::FromString(View.Profile.Name));
		return Model;
	}
	for (const VeyraBackendProtocol::FHistoryEntry& Entry : View.Matches)
	{
		Model.Rows.Add(VeyraMatchHistoryModel::DescribeRow(Entry));
	}
	if (Model.Rows.IsEmpty())
	{
		Model.HistoryNote = View.bMatchesLoaded ? LOCTEXT("NoMatches", "No completed matches yet.") : LOCTEXT("ReadingMatches", "Reading the Match History...");
	}
	Model.bOffersLoadMore = bCanLoadMore && !View.Next.IsEmpty();
	return Model;
}

FVeyraProfilePageModel DescribePage(const FVeyraClientSnapshot& Snapshot, const VeyraBackendProtocol::FProfileSettings& Draft)
{
	const FVeyraProfileSettings& Own = Snapshot.ProfileSettings;
	FVeyraProfilePageModel Model;
	Model.bLoaded = Own.bLoaded;
	if (!Own.bLoaded)
	{
		return Model;
	}
	if (Own.Preview.IsSet())
	{
		Model.Preview = DescribeCard(*Own.Preview);
	}
	for (const FString& Icon : Own.Catalog.Icons)
	{
		Model.Icons.Add({ Icon, IconLabel(Icon), VanguardOfEntry(Icon), Draft.Icon == Icon });
	}
	for (const FString& Background : Own.Catalog.Backgrounds)
	{
		Model.Backgrounds.Add({ Background, BackgroundLabel(Background), VanguardOfEntry(Background), Draft.Background == Background });
	}
	// Only a permanently owned Vanguard may be featured, or none (Profiles Bible §2).
	Model.Featured.Add({ FString(), FeatureLabel(FString()), FString(), Draft.FeaturedVanguardId.IsEmpty() });
	for (const FString& Vanguard : Own.Catalog.FeaturedChoices)
	{
		Model.Featured.Add({ Vanguard, FeatureLabel(Vanguard), Vanguard, Draft.FeaturedVanguardId == Vanguard });
	}
	Model.bShowsMatchHistory = Draft.bShowMatchHistory;
	Model.bChanged = !(Draft == Own.Saved);
	Model.Feedback = FeedbackText(Own.Feedback);
	return Model;
}

FText PageLabel()
{
	return LOCTEXT("PageLabel", "Profile");
}

FText IconLabel(const FString& Entry)
{
	return FText::Format(LOCTEXT("IconLabel", "Icon {0}"), EntryName(Entry));
}

FText BackgroundLabel(const FString& Entry)
{
	return FText::Format(LOCTEXT("BackgroundLabel", "Background {0}"), EntryName(Entry));
}

FText FeatureLabel(const FString& VanguardId)
{
	return VanguardId.IsEmpty() ? LOCTEXT("FeatureNone", "Feature None") : FText::Format(LOCTEXT("FeatureLabel", "Feature {0}"), VeyraShellModels::VanguardNameOf(VanguardId));
}

FText ShareHistoryLabel()
{
	return LOCTEXT("ShareHistory", "Show Match History on My Profile");
}

FText SaveLabel()
{
	return LOCTEXT("SaveProfile", "Save Profile");
}

FText ViewProfileLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("ViewProfile", "View Profile {0}"), FText::FromString(Name));
}

FText MenuProfileLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("MenuProfile", "Profile {0}"), FText::FromString(Name));
}

FText CloseLabel()
{
	return LOCTEXT("CloseProfile", "Close Profile");
}

FString Signature(const FVeyraClientSnapshot& Snapshot)
{
	TStringBuilder<512> Text;
	const FVeyraProfileView& View = Snapshot.ProfileView;
	const VeyraBackendProtocol::FPublicProfile& P = View.Profile;
	Text << TEXT("|profile:") << View.Name << TEXT(":") << (View.bLoaded ? 1 : 0) << (View.bUnavailable ? 1 : 0) << TEXT(":") << P.Icon << TEXT(":") << P.Background
		 << TEXT(":") << P.Level << TEXT(":") << (P.Featured.IsSet() ? *P.Featured->VanguardId : TEXT("")) << TEXT(":") << (P.bSharesMatchHistory ? 1 : 0) << TEXT(":")
		 << (View.bMatchesLoaded ? 1 : 0) << TEXT(":") << View.Matches.Num() << TEXT(":") << View.Next << TEXT(":")
		 << (View.OpenedMatch.IsSet() ? *View.OpenedMatch->MatchId : TEXT(""));
	const FVeyraProfileSettings& Own = Snapshot.ProfileSettings;
	Text << TEXT("|own:") << (Own.bLoaded ? 1 : 0) << TEXT(":") << Own.Saved.Icon << TEXT(":") << Own.Saved.Background << TEXT(":") << Own.Saved.FeaturedVanguardId
		 << TEXT(":") << (Own.Saved.bShowMatchHistory ? 1 : 0) << TEXT(":") << Own.Feedback << TEXT(":") << Own.Catalog.FeaturedChoices.Num();
	if (Own.Preview.IsSet())
	{
		Text << TEXT(":preview:") << Own.Preview->Level << TEXT(":") << (Own.Preview->Featured.IsSet() ? Own.Preview->Featured->MasteryLevel : 0);
	}
	return FString(Text.ToString());
}
}

#undef LOCTEXT_NAMESPACE
