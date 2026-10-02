// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"

/**
 * Chat on the wire (ADR-046): Party Chat, friend direct messages, champion-select team chat and post-match
 * chat, which the backend delivers through one polled inbox. The backend decides who reads each message;
 * the client only shows what it was given.
 */
namespace VeyraBackendProtocol
{
	/** A conversation's kind (ADR-046 §2). */
	enum class EChatKind : uint8
	{
		/** The player's party. */
		Party,
		/** Two friends. */
		Direct,
		/** One side of a champion select. */
		Select,
		/** A match's cross-team chat on the results screen. */
		PostMatch,
	};

	/** The kind's name on the wire: "party", "direct", "select" or "postmatch". */
	VEYRASERVICES_API const TCHAR* ChatKindName(EChatKind Kind);

	/** One message as the backend stored it. */
	struct FChatMessage
	{
		/** Orders every message; a later one has a greater sequence. */
		int64 Seq = 0;
		EChatKind Kind = EChatKind::Party;
		/** The conversation's key: the party, the two accounts, the select and side, or the match. */
		FString Conversation;
		FString SenderId;
		/** The sender's display name when it sent. */
		FString SenderName;
		/** A direct message's other account; empty otherwise. */
		FString RecipientId;
		FString Text;
		/** The sender's own ID for the message. */
		FString ClientId;
	};

	/** One poll's answer: GET /v1/me/chat. */
	struct FChatPage
	{
		/** Oldest first. */
		TArray<FChatMessage> Messages;
		/** The cursor for the next poll. */
		int64 Next = 0;
		/** The page was full: the next poll should come at once. */
		bool bMore = false;
	};

	/** Reads a poll's answer. False, with Problem, if it is not one. */
	VEYRASERVICES_API bool ParseChatPage(const FString& Body, FChatPage& Out, FString& Problem);

	/** Reads a send's answer, {"message": ...}. False, with Problem, if it is not one. */
	VEYRASERVICES_API bool ParseChatSent(const FString& Body, FChatMessage& Out, FString& Problem);

	/** The body of a send: the client's message ID and the text. */
	VEYRASERVICES_API FString BuildChatBody(const FString& ClientId, const FString& Text);
}
