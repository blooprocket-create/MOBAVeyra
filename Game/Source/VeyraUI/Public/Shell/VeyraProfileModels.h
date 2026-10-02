// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Client/VeyraClientFlowTypes.h"
#include "Internationalization/Text.h"
#include "Shell/VeyraMatchHistoryModel.h"

/** A profile as its card shows it (Profiles Bible §1–§2): never an account, an email or anything private. */
struct FVeyraProfileCardModel
{
	FText Name;
	/** The Vanguard whose portrait is the icon; empty for the neutral default icon. */
	FString IconVanguard;
	/** The Vanguard whose art is the background; empty for the neutral default background. */
	FString BackgroundVanguard;
	FText Level;
	/** Empty when no Vanguard is featured. */
	FString FeaturedVanguard;
	/** The featured Vanguard and its Mastery Level, or that none is featured (UX-71). */
	FText Featured;
};

/** Another player's profile opened over the screen (ADR-048 §5). */
struct FVeyraProfileViewModel
{
	bool bOpen = false;
	/** Said instead of the card while it is read, and when it is unavailable. */
	FText Status;
	TOptional<FVeyraProfileCardModel> Card;
	/** The shared Match History's rows, or why there are none: private, still reading, or no matches. */
	TArray<FVeyraHistoryRow> Rows;
	FText HistoryNote;
	bool bOffersLoadMore = false;
	/** One shared match opened into its report. */
	TOptional<FVeyraResultsModel> Opened;
};

/** One choice on the Profile page: an icon, a background or a featured Vanguard. */
struct FVeyraProfileChoice
{
	FString Value;
	/** The button's name, for tests and scripts. */
	FText Label;
	/** The Vanguard its picture shows; empty for a neutral one. */
	FString Vanguard;
	bool bSelected = false;
};

/** The Profile page (ADR-048 §5): the player's profile as others see it, and the choices, as the draft stands. */
struct FVeyraProfilePageModel
{
	bool bLoaded = false;
	TOptional<FVeyraProfileCardModel> Preview;
	TArray<FVeyraProfileChoice> Icons;
	TArray<FVeyraProfileChoice> Backgrounds;
	/** "None", then each permanently owned Vanguard. */
	TArray<FVeyraProfileChoice> Featured;
	bool bShowsMatchHistory = false;
	/** Whether the draft differs from what is saved. */
	bool bChanged = false;
	/** What came of the last save. */
	FText Feedback;
};

namespace VeyraProfileModels
{
	/** The Vanguard a catalog entry's picture shows: "vanguard_<id>" is that Vanguard; anything else is neutral. */
	VEYRAUI_API FString VanguardOfEntry(const FString& Entry);

	VEYRAUI_API FVeyraProfileCardModel DescribeCard(const VeyraBackendProtocol::FPublicProfile& Profile);

	VEYRAUI_API FVeyraProfileViewModel DescribeView(const FVeyraClientSnapshot& Snapshot, bool bCanLoadMore);

	/** The Profile page with Draft, the choices as the player is making them, against what is saved. */
	VEYRAUI_API FVeyraProfilePageModel DescribePage(const FVeyraClientSnapshot& Snapshot, const VeyraBackendProtocol::FProfileSettings& Draft);

	/** The top bar's Profile page. */
	VEYRAUI_API FText PageLabel();
	VEYRAUI_API FText IconLabel(const FString& Entry);
	VEYRAUI_API FText BackgroundLabel(const FString& Entry);
	/** A featured-Vanguard choice; an empty VanguardId is None. */
	VEYRAUI_API FText FeatureLabel(const FString& VanguardId);
	VEYRAUI_API FText ShareHistoryLabel();
	VEYRAUI_API FText SaveLabel();
	/** A friend's card's way to Name's profile. */
	VEYRAUI_API FText ViewProfileLabel(const FString& Name);
	/** The player menu's way to Name's profile. */
	VEYRAUI_API FText MenuProfileLabel(const FString& Name);
	VEYRAUI_API FText CloseLabel();

	/** Everything the profile view and the Profile page show, so the screen rebuilds when any of it changes. */
	VEYRAUI_API FString Signature(const FVeyraClientSnapshot& Snapshot);
}
