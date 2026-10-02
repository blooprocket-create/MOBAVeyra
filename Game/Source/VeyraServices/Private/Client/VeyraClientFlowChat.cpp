// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Client/VeyraClientFlow.h"

#include "Backend/VeyraChatProtocol.h"
#include "Misc/Guid.h"

// Party Chat, friend direct messages, champion-select team chat and post-match chat (ADR-046). The backend
// decides who reads each message and delivers them through one polled inbox; the flow reads it on its own
// clock in every signed-in state but Reconnect-only, so moving between states never interrupts a
// conversation (Chat & Communication Bible §3, §4).

namespace
{
	using VeyraBackendProtocol::EChatKind;

	const TCHAR* const ChatPath = TEXT("/v1/me/chat");
	const TCHAR* const ChatMatchesPath = TEXT("/v1/me/chat/matches/");
	/** A line's failure when no answer came after every try. */
	const TCHAR* const ChatNotSent = TEXT("not_sent");
	/** A line's failure when the answer could not be read. */
	const TCHAR* const ChatBadAnswer = TEXT("bad_answer");

	FString ChatSendPath(EChatKind Kind, const FString& Target)
	{
		switch (Kind)
		{
		case EChatKind::Party:
			return FString(TEXT("/v1/me/chat/party/")) + Target;
		case EChatKind::Direct:
			return FString(TEXT("/v1/me/chat/direct/")) + Target;
		case EChatKind::Select:
			return FString(TEXT("/v1/me/chat/select/")) + Target;
		case EChatKind::PostMatch:
			return FString(ChatMatchesPath) + Target;
		}
		return FString();
	}

	/** The backend's refusal code, or "http_<status>" when it gave none. */
	FString ChatRefusalCode(const FVeyraBackendResponse& Response)
	{
		const FString Code = VeyraBackendProtocol::ParseErrorCode(Response.Body);
		return Code.IsEmpty() ? FString::Printf(TEXT("http_%d"), Response.Status) : Code;
	}

	/** Confirmed lines in the backend's order, then the player's own lines still awaiting their answers. */
	void SortChatLines(TArray<FVeyraChatEntry>& Lines)
	{
		Lines.StableSort([](const FVeyraChatEntry& A, const FVeyraChatEntry& B) {
			const int64 KeyA = A.Seq == 0 ? MAX_int64 : A.Seq;
			const int64 KeyB = B.Seq == 0 ? MAX_int64 : B.Seq;
			return KeyA < KeyB;
		});
	}
}

bool FVeyraClientFlow::ChatRuns() const
{
	return !GameSession.IsEmpty() && IsIntentAllowed(Snapshot.State, EVeyraClientIntent::SendChatMessage);
}

void FVeyraClientFlow::TickChat(double Now)
{
	if (!ChatRuns())
	{
		return;
	}
	if (!bChatPollInFlight && Now >= NextChatPollAt)
	{
		PollChat();
	}
	TArray<FString> Due;
	for (const FChatSend& Pending : ChatSends)
	{
		if (Pending.DueAt > 0.0 && Now >= Pending.DueAt)
		{
			Due.Add(Pending.ClientId);
		}
	}
	for (const FString& ClientId : Due)
	{
		SendPendingChat(ClientId);
	}
}

void FVeyraClientFlow::PollChat()
{
	bChatPollInFlight = true;
	const FString Path = bChatHasCursor ? FString::Printf(TEXT("%s?after=%lld"), ChatPath, static_cast<long long>(ChatCursor)) : FString(ChatPath);
	Send(EVerb::Get, Path, FString(), [this, WeakAlive = TWeakPtr<bool>(Alive), Generation = ChatGeneration](const FVeyraBackendResponse& Response) {
		if (!WeakAlive.IsValid() || Generation != ChatGeneration)
		{
			return;
		}
		bChatPollInFlight = false;
		NextChatPollAt = Host.Now() + Config.ChatPollIntervalSeconds;
		if (Response.IsUnauthorized())
		{
			EndSession();
			return;
		}
		VeyraBackendProtocol::FChatPage Page;
		FString Problem;
		// A backend without chat, or a failed read, is read again at the next interval; nothing stops.
		if (!Response.IsSuccess() || !VeyraBackendProtocol::ParseChatPage(Response.Body, Page, Problem))
		{
			return;
		}
		const bool bHistory = !bChatHasCursor;
		for (const VeyraBackendProtocol::FChatMessage& Message : Page.Messages)
		{
			ApplyChatMessage(Message, bHistory);
		}
		ChatCursor = Page.Next;
		bChatHasCursor = true;
		if (Page.bMore)
		{
			NextChatPollAt = Host.Now();
		}
		if (!Page.Messages.IsEmpty() || !Snapshot.Chat.bLoaded)
		{
			Snapshot.Chat.bLoaded = true;
			Broadcast();
		}
	});
}

FVeyraChatConversation* FVeyraClientFlow::ChatConversationFor(const VeyraBackendProtocol::FChatMessage& Message)
{
	FVeyraChat& Chat = Snapshot.Chat;
	switch (Message.Kind)
	{
	case EChatKind::Party:
	case EChatKind::Select:
	{
		// The backend delivers only the current party's and select's lines, so another key means a new one.
		FVeyraChatConversation& Conversation = Message.Kind == EChatKind::Party ? Chat.Party : Chat.Select;
		if (!Conversation.Key.IsEmpty() && Conversation.Key != Message.Conversation)
		{
			ResetChatConversation(Conversation, Message.Conversation);
		}
		Conversation.Key = Message.Conversation;
		return &Conversation;
	}
	case EChatKind::Direct:
	{
		const FString With = Message.SenderId == Snapshot.AccountId ? Message.RecipientId : Message.SenderId;
		FVeyraChatConversation& Conversation = Chat.Direct.FindOrAdd(With);
		Conversation.Key = With;
		return &Conversation;
	}
	case EChatKind::PostMatch:
		// Only the match on the results screen; a late line for an earlier one has nowhere to go.
		return !Chat.PostMatch.Key.IsEmpty() && Chat.PostMatch.Key == Message.Conversation ? &Chat.PostMatch : nullptr;
	}
	return nullptr;
}

void FVeyraClientFlow::ApplyChatMessage(const VeyraBackendProtocol::FChatMessage& Message, bool bHistory)
{
	FVeyraChatConversation* Conversation = ChatConversationFor(Message);
	if (!Conversation)
	{
		return;
	}
	const bool bOwn = Message.SenderId == Snapshot.AccountId;
	// A line already shown: a repeated page, or the player's own message answered after a read delivered it.
	FVeyraChatEntry* Shown = Conversation->Lines.FindByPredicate([&Message, bOwn](const FVeyraChatEntry& Line) {
		return Line.Seq == Message.Seq || (bOwn && Line.Seq == 0 && Line.ClientId == Message.ClientId);
	});
	if (bOwn)
	{
		ChatSends.RemoveAll([&Message](const FChatSend& Pending) { return Pending.ClientId == Message.ClientId; });
		// The player's own post-match line opted them in (UX-59), whether its answer or a read brought it first.
		if (Message.Kind == EChatKind::PostMatch)
		{
			Snapshot.Chat.bPostMatchJoined = true;
		}
	}
	if (Shown)
	{
		Shown->Seq = Message.Seq;
		Shown->bPending = false;
		Shown->Failure.Reset();
		// The backend's cleaned text and the name it recorded are the line's from now on.
		Shown->Text = Message.Text;
		Shown->SenderName = Message.SenderName;
		SortChatLines(Conversation->Lines);
		return;
	}
	FVeyraChatEntry Line;
	Line.Seq = Message.Seq;
	Line.Kind = Message.Kind;
	Line.SenderId = Message.SenderId;
	Line.SenderName = Message.SenderName;
	Line.Text = Message.Text;
	Line.ClientId = Message.ClientId;
	Line.ArrivedAt = Host.Now();
	Line.bHistory = bHistory;
	if (Message.Kind == EChatKind::Direct)
	{
		Line.With = Conversation->Key;
		if (!bOwn && !bHistory)
		{
			Snapshot.Chat.LastDirectFrom = Message.SenderId;
			if (Snapshot.Chat.OpenDirect != Conversation->Key)
			{
				++Conversation->Unread;
			}
		}
	}
	Conversation->Lines.Add(MoveTemp(Line));
	SortChatLines(Conversation->Lines);
	TrimChat(*Conversation);
}

void FVeyraClientFlow::TrimChat(FVeyraChatConversation& Conversation) const
{
	const int32 Excess = Conversation.Lines.Num() - FMath::Max(Config.ChatKeepMessages, 1);
	if (Excess > 0)
	{
		Conversation.Lines.RemoveAt(0, Excess);
	}
}

void FVeyraClientFlow::ResetChatConversation(FVeyraChatConversation& Conversation, const FString& Key)
{
	for (const FVeyraChatEntry& Line : Conversation.Lines)
	{
		if (Line.bPending)
		{
			const FString ClientId = Line.ClientId;
			ChatSends.RemoveAll([&ClientId](const FChatSend& Pending) { return Pending.ClientId == ClientId; });
		}
	}
	Conversation = FVeyraChatConversation();
	Conversation.Key = Key;
}

bool FVeyraClientFlow::SendChatMessage(VeyraBackendProtocol::EChatKind Kind, const FString& Target, const FString& Text)
{
	const FString Trimmed = Text.TrimStartAndEnd();
	if (!CanIssue(EVeyraClientIntent::SendChatMessage) || Trimmed.IsEmpty())
	{
		return false;
	}
	FVeyraChat& Chat = Snapshot.Chat;
	FVeyraChatConversation* Conversation = nullptr;
	FString SendTarget;
	switch (Kind)
	{
	case EChatKind::Party:
		// The party the player is writing to, so a send that arrives after they changed party is refused (ADR-046 §3).
		SendTarget = Snapshot.Party.IsSet() ? Snapshot.Party->Id : Chat.Party.Key;
		if (SendTarget.IsEmpty())
		{
			return false;
		}
		Conversation = &Chat.Party;
		break;
	case EChatKind::Direct:
		if (!Snapshot.Social.Friends.Friends.ContainsByPredicate([&Target](const VeyraBackendProtocol::FAccount& Friend) { return Friend.Id == Target; }))
		{
			return false;
		}
		Conversation = &Chat.Direct.FindOrAdd(Target);
		Conversation->Key = Target;
		SendTarget = Target;
		break;
	case EChatKind::Select:
		if (Snapshot.State != EVeyraClientState::Selecting || Snapshot.Select.Id.IsEmpty())
		{
			return false;
		}
		Conversation = &Chat.Select;
		SendTarget = Snapshot.Select.Id;
		break;
	case EChatKind::PostMatch:
		if (Snapshot.State != EVeyraClientState::Results || Chat.PostMatch.Key.IsEmpty())
		{
			return false;
		}
		Conversation = &Chat.PostMatch;
		SendTarget = Chat.PostMatch.Key;
		break;
	}
	if (!Conversation)
	{
		return false;
	}
	FChatSend Pending;
	Pending.ClientId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	Pending.Kind = Kind;
	Pending.Target = SendTarget;
	Pending.Text = Trimmed;
	FVeyraChatEntry Line;
	Line.Kind = Kind;
	Line.SenderId = Snapshot.AccountId;
	Line.SenderName = Snapshot.DisplayName;
	Line.With = Kind == EChatKind::Direct ? Target : FString();
	Line.Text = Trimmed;
	Line.ClientId = Pending.ClientId;
	Line.bPending = true;
	Line.ArrivedAt = Host.Now();
	Conversation->Lines.Add(MoveTemp(Line));
	TrimChat(*Conversation);
	const FString ClientId = Pending.ClientId;
	ChatSends.Add(MoveTemp(Pending));
	Log(FString::Printf(TEXT("sending a %s chat message."), VeyraBackendProtocol::ChatKindName(Kind)));
	SendPendingChat(ClientId);
	Broadcast();
	return true;
}

void FVeyraClientFlow::SendPendingChat(const FString& ClientId)
{
	FChatSend* Pending = ChatSends.FindByPredicate([&ClientId](const FChatSend& Candidate) { return Candidate.ClientId == ClientId; });
	if (!Pending)
	{
		return;
	}
	Pending->DueAt = 0.0;
	const FString Body = VeyraBackendProtocol::BuildChatBody(Pending->ClientId, Pending->Text);
	Send(EVerb::Post, ChatSendPath(Pending->Kind, Pending->Target), Body,
		[this, WeakAlive = TWeakPtr<bool>(Alive), Generation = ChatGeneration, ClientId](const FVeyraBackendResponse& Response) {
			if (!WeakAlive.IsValid() || Generation != ChatGeneration)
			{
				return;
			}
			if (Response.IsUnauthorized())
			{
				EndSession();
				return;
			}
			FChatSend* Answered = ChatSends.FindByPredicate([&ClientId](const FChatSend& Candidate) { return Candidate.ClientId == ClientId; });
			if (!Answered)
			{
				// A read already delivered it, or its conversation started afresh.
				return;
			}
			if (Response.IsTransient())
			{
				// No answer: the same ID again, so the backend never stores it twice (ADR-046 §3).
				if (Answered->Attempt < Config.RequestAttempts)
				{
					++Answered->Attempt;
					Answered->DueAt = Host.Now() + Config.RetryIntervalSeconds;
					return;
				}
				FailChatLine(ClientId, ChatNotSent);
				return;
			}
			if (!Response.IsSuccess())
			{
				FailChatLine(ClientId, ChatRefusalCode(Response));
				return;
			}
			VeyraBackendProtocol::FChatMessage Message;
			FString Problem;
			if (!VeyraBackendProtocol::ParseChatSent(Response.Body, Message, Problem))
			{
				FailChatLine(ClientId, ChatBadAnswer);
				return;
			}
			ApplyChatMessage(Message, false);
			Broadcast();
		});
}

void FVeyraClientFlow::FailChatLine(const FString& ClientId, const FString& Failure)
{
	ChatSends.RemoveAll([&ClientId](const FChatSend& Pending) { return Pending.ClientId == ClientId; });
	FVeyraChat& Chat = Snapshot.Chat;
	TArray<FVeyraChatConversation*> Conversations = { &Chat.Party, &Chat.Select, &Chat.PostMatch };
	for (TPair<FString, FVeyraChatConversation>& Direct : Chat.Direct)
	{
		Conversations.Add(&Direct.Value);
	}
	for (FVeyraChatConversation* Conversation : Conversations)
	{
		if (FVeyraChatEntry* Line = Conversation->Lines.FindByPredicate([&ClientId](const FVeyraChatEntry& Candidate) { return Candidate.ClientId == ClientId; }))
		{
			Line->bPending = false;
			Line->Failure = Failure;
			Log(FString::Printf(TEXT("a %s chat message was not sent: %s."), VeyraBackendProtocol::ChatKindName(Line->Kind), *Failure));
			Broadcast();
			return;
		}
	}
}

bool FVeyraClientFlow::OpenDirectChat(const FString& AccountId)
{
	if (!CanIssue(EVeyraClientIntent::OpenDirectChat)
		|| !Snapshot.Social.Friends.Friends.ContainsByPredicate([&AccountId](const VeyraBackendProtocol::FAccount& Friend) { return Friend.Id == AccountId; }))
	{
		return false;
	}
	FVeyraChatConversation& Conversation = Snapshot.Chat.Direct.FindOrAdd(AccountId);
	Conversation.Key = AccountId;
	Conversation.Unread = 0;
	Snapshot.Chat.OpenDirect = AccountId;
	Broadcast();
	return true;
}

bool FVeyraClientFlow::CloseDirectChat()
{
	if (!CanIssue(EVeyraClientIntent::CloseDirectChat) || Snapshot.Chat.OpenDirect.IsEmpty())
	{
		return false;
	}
	Snapshot.Chat.OpenDirect.Reset();
	Broadcast();
	return true;
}

bool FVeyraClientFlow::MutePostMatchChat(const FString& AccountId, bool bMute)
{
	FVeyraChat& Chat = Snapshot.Chat;
	// Only someone the player can read: another participant whose line the post-match chat shows.
	const bool bSpoke = Chat.PostMatch.Lines.ContainsByPredicate([&AccountId](const FVeyraChatEntry& Line) { return Line.SenderId == AccountId; });
	if (!CanIssue(EVeyraClientIntent::MutePostMatchChat) || Chat.PostMatch.Key.IsEmpty() || AccountId == Snapshot.AccountId || !bSpoke
		|| Chat.PostMatchMuted.Contains(AccountId) == bMute)
	{
		return false;
	}
	if (bMute)
	{
		Chat.PostMatchMuted.Add(AccountId);
	}
	else
	{
		Chat.PostMatchMuted.Remove(AccountId);
	}
	// The backend keeps the mute at delivery; a lost answer leaves the screen's mute, which hides the lines anyway.
	Send(bMute ? EVerb::Put : EVerb::Delete, FString(ChatMatchesPath) + Chat.PostMatch.Key + TEXT("/mutes/") + AccountId, FString(),
		[](const FVeyraBackendResponse&) {});
	Broadcast();
	return true;
}

void FVeyraClientFlow::StartPostMatchChat(const FString& MatchId)
{
	ResetChatConversation(Snapshot.Chat.PostMatch, MatchId);
	Snapshot.Chat.bPostMatchJoined = false;
	Snapshot.Chat.PostMatchMuted.Reset();
}

void FVeyraClientFlow::LeavePostMatchChat()
{
	FVeyraChat& Chat = Snapshot.Chat;
	if (Chat.PostMatch.Key.IsEmpty())
	{
		return;
	}
	// Leaving the results screen ends the player's part for good (UX-60); the answer changes nothing here.
	Send(EVerb::Delete, FString(ChatMatchesPath) + Chat.PostMatch.Key, FString(), [](const FVeyraBackendResponse&) {});
	StartPostMatchChat(FString());
}
