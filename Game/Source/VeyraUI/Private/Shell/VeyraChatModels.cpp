// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraChatModels.h"

#include "Algo/StableSort.h"
#include "Misc/StringBuilder.h"

#define LOCTEXT_NAMESPACE "VeyraChatModels"

namespace VeyraChatModels
{
namespace
{
	using VeyraBackendProtocol::EChatKind;

	const VeyraBackendProtocol::FAccount* FindChatFriend(const FVeyraClientSnapshot& Snapshot, const FString& AccountId)
	{
		return Snapshot.Social.Friends.Friends.FindByPredicate([&AccountId](const VeyraBackendProtocol::FAccount& Friend) { return Friend.Id == AccountId; });
	}

	void AppendChatSignature(FStringBuilderBase& Out, const TCHAR* Name, const FVeyraChatConversation& Conversation)
	{
		Out << Name << TEXT(":") << Conversation.Key << TEXT(":") << Conversation.Unread;
		for (const FVeyraChatEntry& Entry : Conversation.Lines)
		{
			Out << TEXT(",") << Entry.Seq << TEXT("/") << Entry.ClientId << (Entry.bPending ? TEXT("p") : TEXT("")) << Entry.Failure;
		}
		Out << TEXT(";");
	}
}

FText DescribeFailure(const FString& Code)
{
	if (Code == TEXT("rate_limited"))
	{
		return LOCTEXT("FailRate", "Not sent: too many messages at once. Wait a moment.");
	}
	if (Code == TEXT("not_in_party"))
	{
		return LOCTEXT("FailParty", "Not sent: you are not in a party.");
	}
	if (Code == TEXT("not_friends"))
	{
		return LOCTEXT("FailFriends", "Not sent: you are no longer friends.");
	}
	if (Code == TEXT("blocked"))
	{
		return LOCTEXT("FailBlocked", "Not sent: one of you blocked the other.");
	}
	if (Code == TEXT("message_too_long"))
	{
		return LOCTEXT("FailLong", "Not sent: the message is too long.");
	}
	if (Code == TEXT("no_select"))
	{
		return LOCTEXT("FailSelect", "Not sent: champion select has ended.");
	}
	if (Code == TEXT("postmatch_closed"))
	{
		return LOCTEXT("FailPostMatch", "Not sent: the post-match chat has closed.");
	}
	if (Code == TEXT("all_chat_off"))
	{
		return LOCTEXT("FailAllChat", "Not sent: All Chat is off in your settings.");
	}
	if (Code == TEXT("not_sent"))
	{
		return LOCTEXT("FailUnanswered", "Not sent: no answer from the server.");
	}
	return LOCTEXT("FailOther", "Not sent.");
}

FVeyraChatLineModel DescribeLine(const FVeyraChatEntry& Entry, const FString& PlayerId, bool bMarkParty)
{
	FVeyraChatLineModel Line;
	Line.bOwn = Entry.SenderId == PlayerId;
	Line.Sender = Line.bOwn ? LOCTEXT("You", "You") : FText::FromString(Entry.SenderName);
	Line.Text = FText::FromString(Entry.Text);
	Line.bParty = bMarkParty && Entry.Kind == EChatKind::Party;
	Line.bFailed = !Entry.Failure.IsEmpty();
	if (Entry.bPending)
	{
		Line.Status = LOCTEXT("Sending", "Sending…");
	}
	else if (Line.bFailed)
	{
		Line.Status = DescribeFailure(Entry.Failure);
	}
	return Line;
}

FVeyraChatPanelModel DescribeSidebar(const FVeyraClientSnapshot& Snapshot, bool bCanSend)
{
	FVeyraChatPanelModel Panel;
	const FVeyraChat& Chat = Snapshot.Chat;
	if (const VeyraBackendProtocol::FAccount* Friend = Chat.OpenDirect.IsEmpty() ? nullptr : FindChatFriend(Snapshot, Chat.OpenDirect))
	{
		Panel.bVisible = true;
		Panel.Kind = EChatKind::Direct;
		Panel.Target = Friend->Id;
		Panel.Title = FText::FromString(Friend->DisplayName);
		Panel.Hint = FText::Format(LOCTEXT("DirectHint", "Message {0}"), FText::FromString(Friend->DisplayName));
		Panel.Empty = FText::Format(LOCTEXT("DirectEmpty", "No messages with {0} yet."), FText::FromString(Friend->DisplayName));
		Panel.bCanClose = true;
		if (const FVeyraChatConversation* Conversation = Chat.Direct.Find(Friend->Id))
		{
			for (const FVeyraChatEntry& Entry : Conversation->Lines)
			{
				Panel.Lines.Add(DescribeLine(Entry, Snapshot.AccountId, /*bMarkParty*/ false));
			}
		}
	}
	else if (Snapshot.Party.IsSet() || !Chat.Party.Lines.IsEmpty())
	{
		Panel.bVisible = true;
		Panel.Kind = EChatKind::Party;
		Panel.Title = LOCTEXT("PartyTitle", "Party Chat");
		Panel.Hint = LOCTEXT("PartyHint", "Message your party");
		Panel.Empty = LOCTEXT("PartyEmpty", "Say something to your party.");
		for (const FVeyraChatEntry& Entry : Chat.Party.Lines)
		{
			Panel.Lines.Add(DescribeLine(Entry, Snapshot.AccountId, /*bMarkParty*/ false));
		}
	}
	Panel.bCanSend = Panel.bVisible && bCanSend;
	return Panel;
}

FVeyraChatPanelModel DescribeSelectChat(const FVeyraClientSnapshot& Snapshot, bool bCanSend)
{
	FVeyraChatPanelModel Panel;
	Panel.bVisible = true;
	Panel.Kind = EChatKind::Select;
	Panel.Title = LOCTEXT("SelectTitle", "Team Chat");
	Panel.Hint = Snapshot.Party.IsSet() || !Snapshot.Chat.Party.Lines.IsEmpty() ? LOCTEXT("SelectHintParty", "Message your team, or /p your party")
																				  : LOCTEXT("SelectHint", "Message your team");
	Panel.Empty = LOCTEXT("SelectEmpty", "Say something to your team.");
	Panel.bShowsRecipient = true;
	Panel.bCanSend = bCanSend;
	// One shared display: the team's lines and the party's, in the order the backend gave them (UX-33).
	TArray<const FVeyraChatEntry*> Entries;
	for (const FVeyraChatEntry& Entry : Snapshot.Chat.Select.Lines)
	{
		Entries.Add(&Entry);
	}
	for (const FVeyraChatEntry& Entry : Snapshot.Chat.Party.Lines)
	{
		Entries.Add(&Entry);
	}
	Algo::StableSortBy(Entries, [](const FVeyraChatEntry* Entry) { return Entry->Seq == 0 ? MAX_int64 : Entry->Seq; });
	for (const FVeyraChatEntry* Entry : Entries)
	{
		Panel.Lines.Add(DescribeLine(*Entry, Snapshot.AccountId, /*bMarkParty*/ true));
	}
	return Panel;
}

EChatKind SelectRecipient(const FString& Draft, FString& OutText)
{
	const FString Line = Draft.TrimStart();
	// "/p" alone, or before a space: the rest goes to the party.
	if (Line.StartsWith(TEXT("/p"), ESearchCase::IgnoreCase) && (Line.Len() == 2 || FChar::IsWhitespace(Line[2])))
	{
		OutText = Line.Mid(2).TrimStartAndEnd();
		return EChatKind::Party;
	}
	OutText = Draft;
	return EChatKind::Select;
}

FText RecipientLabel(EChatKind Kind)
{
	return Kind == EChatKind::Party ? LOCTEXT("RecipientParty", "Party") : LOCTEXT("RecipientTeam", "Team");
}

FString Signature(const FVeyraChat& Chat)
{
	TStringBuilder<512> Out;
	Out << TEXT("chat ") << (Chat.bLoaded ? 1 : 0) << TEXT(" open:") << Chat.OpenDirect << TEXT(" joined:") << (Chat.bPostMatchJoined ? 1 : 0) << TEXT(" muted:")
		<< FString::Join(Chat.PostMatchMuted, TEXT(",")) << TEXT("|");
	AppendChatSignature(Out, TEXT("party"), Chat.Party);
	AppendChatSignature(Out, TEXT("select"), Chat.Select);
	AppendChatSignature(Out, TEXT("postmatch"), Chat.PostMatch);
	TArray<FString> Friends;
	Chat.Direct.GetKeys(Friends);
	Friends.Sort();
	for (const FString& Friend : Friends)
	{
		AppendChatSignature(Out, *Friend, Chat.Direct[Friend]);
	}
	return Out.ToString();
}
}

#undef LOCTEXT_NAMESPACE
