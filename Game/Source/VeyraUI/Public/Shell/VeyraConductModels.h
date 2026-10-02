// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Client/VeyraClientFlowTypes.h"
#include "Internationalization/Text.h"

/** What the screen may ask of the client now, for a player menu. */
struct FVeyraPlayerMenuPermissions
{
	bool bCanAddFriend = false;
	bool bCanInvite = false;
	bool bCanCommend = false;
	bool bCanReport = false;
};

/**
 * Another human's player menu on the results screen or a Match History record (UX-57; ADR-047 §5): what it
 * offers them, and what came of the player's requests about them.
 */
struct FVeyraPlayerMenuModel
{
	FString Name;
	/** Unless already friends, or a request waits either way. */
	bool bOffersAddFriend = false;
	/** For a friend not in the player's party, on the results screen. */
	bool bOffersInvite = false;
	/** The friend's account, which Invite to Party names. */
	FString FriendAccountId;
	/** For a teammate, once per match, on the results screen. */
	bool bOffersCommend = false;
	bool bOffersReport = false;
	/** Report sent, Commended, and what came of the player's last request about them. */
	TArray<FText> Notes;
};

/** A player menu's report form: the reasons the backend offers, and how long the details may be. */
struct FVeyraReportFormModel
{
	TArray<FString> Reasons;
	int32 DetailsMaxCharacters = 0;
};

/** Player menus, the report form and their labels (ADR-047 §5). */
namespace VeyraConductModels
{
	/**
	 * The record's player Name, when the match the screen shows (the results', or the opened Match History
	 * record's) has its conduct record read and it lists them; null otherwise, as for the player and bots.
	 */
	VEYRAUI_API const VeyraBackendProtocol::FConductPlayer* MenuPlayer(const FVeyraClientSnapshot& Snapshot, const FString& Name);

	/** The match the screen shows a report of: the results', or the opened Match History record's; empty for none. */
	VEYRAUI_API FString ShownMatch(const FVeyraClientSnapshot& Snapshot);

	VEYRAUI_API FVeyraPlayerMenuModel DescribeMenu(const FVeyraClientSnapshot& Snapshot, const FString& Name, const FVeyraPlayerMenuPermissions& Can);

	VEYRAUI_API FVeyraReportFormModel DescribeReportForm(const FVeyraClientSnapshot& Snapshot);

	/** What came of a report or commendation, as the card says it. */
	VEYRAUI_API FText FeedbackText(const FString& Code);

	/** A report reason as the form names it. */
	VEYRAUI_API FText ReasonLabel(const FString& Reason);

	/** The scoreboard row's name, which opens or closes Name's menu. */
	VEYRAUI_API FText MenuLabel(const FString& Name);
	VEYRAUI_API FText AddFriendLabel(const FString& Name);
	VEYRAUI_API FText InviteLabel(const FString& Name);
	VEYRAUI_API FText CommendLabel(const FString& Name);
	VEYRAUI_API FText ReportLabel(const FString& Name);
	/** A reason's button in Name's report form. */
	VEYRAUI_API FText ReasonButtonLabel(const FString& Name, const FString& Reason);
	VEYRAUI_API FText SubmitReportLabel(const FString& Name);
	VEYRAUI_API FText CancelReportLabel(const FString& Name);
	/** How much of the details' limit a draft uses. */
	VEYRAUI_API FText DetailsCount(int32 Used, int32 MaxCharacters);
	/** The results screen's Play Again (UX-62). */
	VEYRAUI_API FText PlayAgainLabel();

	/** Everything a player menu shows, so the screen rebuilds when any of it changes. */
	VEYRAUI_API FString Signature(const FVeyraClientSnapshot& Snapshot);
}
