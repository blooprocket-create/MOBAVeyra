// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraConductModels.h"

#include "Misc/StringBuilder.h"
#include "Shell/VeyraShellModels.h"

#define LOCTEXT_NAMESPACE "VeyraConductModels"

namespace VeyraConductModels
{
namespace
{
	const VeyraBackendProtocol::FAccount* ConductAccountNamed(const TArray<VeyraBackendProtocol::FAccount>& Accounts, const FString& Name)
	{
		return Accounts.FindByPredicate([&Name](const VeyraBackendProtocol::FAccount& Account) { return Account.DisplayName == Name; });
	}
}

FString ShownMatch(const FVeyraClientSnapshot& Snapshot)
{
	if (Snapshot.State == EVeyraClientState::Results)
	{
		return Snapshot.Result.IsSet() ? Snapshot.Result->MatchId : FString();
	}
	return Snapshot.State == EVeyraClientState::Shell && Snapshot.History.Opened.IsSet() ? Snapshot.History.Opened->MatchId : FString();
}

const VeyraBackendProtocol::FConductPlayer* MenuPlayer(const FVeyraClientSnapshot& Snapshot, const FString& Name)
{
	const FVeyraConduct& Conduct = Snapshot.Conduct;
	const FString Match = ShownMatch(Snapshot);
	if (!Conduct.bLoaded || Match.IsEmpty() || Conduct.MatchId != Match)
	{
		return nullptr;
	}
	return Conduct.Record.Players.FindByPredicate([&Name](const VeyraBackendProtocol::FConductPlayer& Player) { return Player.Name == Name; });
}

FVeyraPlayerMenuModel DescribeMenu(const FVeyraClientSnapshot& Snapshot, const FString& Name, const FVeyraPlayerMenuPermissions& Can)
{
	FVeyraPlayerMenuModel Model;
	Model.Name = Name;
	const VeyraBackendProtocol::FConductPlayer* Player = MenuPlayer(Snapshot, Name);
	if (!Player)
	{
		return Model;
	}
	const VeyraBackendProtocol::FConductRecord& Record = Snapshot.Conduct.Record;
	// After a report the card says so, and shows nothing more (ADR-047 §5).
	if (Record.Reported.Contains(Name))
	{
		Model.Notes.Add(LOCTEXT("ReportSent", "Report sent"));
		return Model;
	}
	const VeyraBackendProtocol::FFriends& Lists = Snapshot.Social.Friends;
	const VeyraBackendProtocol::FAccount* Friend = ConductAccountNamed(Lists.Friends, Name);
	const bool bRequestWaits = ConductAccountNamed(Lists.Outgoing, Name) || ConductAccountNamed(Lists.Incoming, Name);
	Model.bOffersAddFriend = Can.bCanAddFriend && !Friend && !bRequestWaits;
	Model.bOffersInvite = Can.bCanInvite && Friend && !(Snapshot.Party.IsSet() && Snapshot.Party->Find(Friend->Id));
	Model.FriendAccountId = Friend ? Friend->Id : FString();
	Model.bOffersCommend = Can.bCanCommend && Player->bTeammate && Record.Commended.IsEmpty();
	Model.bOffersReport = Can.bCanReport;
	Model.bOffersProfile = Can.bCanViewProfile;
	if (Record.Commended == Name)
	{
		Model.Notes.Add(LOCTEXT("Commended", "Commended"));
	}
	const FVeyraConduct& Conduct = Snapshot.Conduct;
	if (Conduct.FeedbackName == Name && Conduct.Feedback != TEXT("commended") && !Conduct.Feedback.IsEmpty())
	{
		Model.Notes.Add(FeedbackText(Conduct.Feedback));
	}
	if (Snapshot.Social.FeedbackName == Name && !Snapshot.Social.Feedback.IsEmpty())
	{
		Model.Notes.Add(VeyraShellModels::DescribeSocialFeedback(Snapshot.Social.Feedback, Name));
	}
	return Model;
}

FVeyraReportFormModel DescribeReportForm(const FVeyraClientSnapshot& Snapshot)
{
	FVeyraReportFormModel Model;
	Model.Reasons = Snapshot.Conduct.Record.Reasons;
	Model.DetailsMaxCharacters = Snapshot.Conduct.Record.DetailsMaxCharacters;
	return Model;
}

FText FeedbackText(const FString& Code)
{
	if (Code == TEXT("report_sent"))
	{
		return LOCTEXT("FeedbackReportSent", "Report sent");
	}
	if (Code == TEXT("commended"))
	{
		return LOCTEXT("FeedbackCommended", "Commended");
	}
	if (Code == TEXT("report_closed"))
	{
		return LOCTEXT("FeedbackReportClosed", "Reports for this match have closed.");
	}
	if (Code == TEXT("commend_closed"))
	{
		return LOCTEXT("FeedbackCommendClosed", "Commendation for this match has closed.");
	}
	if (Code == TEXT("already_commended"))
	{
		return LOCTEXT("FeedbackAlreadyCommended", "You have already commended a player in this match.");
	}
	if (Code == TEXT("not_teammate"))
	{
		return LOCTEXT("FeedbackNotTeammate", "Only a teammate can be commended.");
	}
	if (Code == TEXT("details_too_long"))
	{
		return LOCTEXT("FeedbackDetailsTooLong", "The details are too long.");
	}
	return FText::Format(LOCTEXT("FeedbackFailed", "That did not go through ({0})."), FText::FromString(Code));
}

FText ReasonLabel(const FString& Reason)
{
	// The backend's reasons (ADR-047 §6); one it adds later still shows, by its own name.
	if (Reason == TEXT("abusive_chat"))
	{
		return LOCTEXT("ReasonAbusiveChat", "Abusive chat");
	}
	if (Reason == TEXT("afk"))
	{
		return LOCTEXT("ReasonAway", "Away or leaving");
	}
	if (Reason == TEXT("griefing"))
	{
		return LOCTEXT("ReasonGriefing", "Deliberately losing");
	}
	if (Reason == TEXT("cheating"))
	{
		return LOCTEXT("ReasonCheating", "Cheating");
	}
	if (Reason == TEXT("offensive_name"))
	{
		return LOCTEXT("ReasonOffensiveName", "Offensive name");
	}
	if (Reason == TEXT("other"))
	{
		return LOCTEXT("ReasonOther", "Other");
	}
	FString Words = Reason.Replace(TEXT("_"), TEXT(" "));
	if (!Words.IsEmpty())
	{
		Words[0] = FChar::ToUpper(Words[0]);
	}
	return FText::FromString(Words);
}

FText MenuLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("MenuLabel", "Player {0}"), FText::FromString(Name));
}

FText AddFriendLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("AddFriendLabel", "Add Friend {0}"), FText::FromString(Name));
}

FText InviteLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("InviteLabel", "Invite to Party {0}"), FText::FromString(Name));
}

FText CommendLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("CommendLabel", "Commend {0}"), FText::FromString(Name));
}

FText ReportLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("ReportLabel", "Report {0}"), FText::FromString(Name));
}

FText ReasonButtonLabel(const FString& Name, const FString& Reason)
{
	return FText::Format(LOCTEXT("ReasonButtonLabel", "Report {0} for {1}"), FText::FromString(Name), ReasonLabel(Reason));
}

FText SubmitReportLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("SubmitReportLabel", "Submit Report {0}"), FText::FromString(Name));
}

FText CancelReportLabel(const FString& Name)
{
	return FText::Format(LOCTEXT("CancelReportLabel", "Cancel Report {0}"), FText::FromString(Name));
}

FText DetailsCount(int32 Used, int32 MaxCharacters)
{
	return FText::Format(LOCTEXT("DetailsCount", "{0} / {1}"), FText::AsNumber(Used), FText::AsNumber(MaxCharacters));
}

FText PlayAgainLabel()
{
	return LOCTEXT("PlayAgain", "Play Again");
}

FString Signature(const FVeyraClientSnapshot& Snapshot)
{
	const FVeyraConduct& Conduct = Snapshot.Conduct;
	TStringBuilder<256> Text;
	Text << TEXT("|conduct:") << Conduct.MatchId << TEXT(":") << (Conduct.bLoaded ? 1 : 0) << TEXT(":") << Conduct.Record.Commended << TEXT(":") << Conduct.Feedback
		 << TEXT(":") << Conduct.FeedbackName;
	for (const FString& Name : Conduct.Record.Reported)
	{
		Text << TEXT(";reported:") << Name;
	}
	for (const VeyraBackendProtocol::FConductPlayer& Player : Conduct.Record.Players)
	{
		Text << TEXT(";player:") << Player.Name << (Player.bTeammate ? TEXT("+") : TEXT("-"));
	}
	return FString(Text.ToString());
}
}

#undef LOCTEXT_NAMESPACE
