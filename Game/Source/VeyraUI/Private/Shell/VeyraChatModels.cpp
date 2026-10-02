// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraChatModels.h"

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
