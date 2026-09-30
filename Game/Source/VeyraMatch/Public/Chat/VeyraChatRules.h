// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Chat/VeyraChatTypes.h"
#include "Containers/Array.h"

struct FVeyraChatTuning;

/** The pure rules of in-match chat (ADR-029), tested without a world. */
namespace VeyraChat
{
	/** Text as it may be sent: control characters removed, surrounding whitespace trimmed. */
	VEYRAMATCH_API FString Clean(const FString& Text);

	/** Why cleaned Text may not be sent: nothing left, or more than chat.maxCharacters. None if it may. */
	VEYRAMATCH_API EVeyraChatRefusal CheckText(const FString& Cleaned, const FVeyraChatTuning& Tuning);

	/**
	 * Whether a player who chatted at SentAt may chat again at Now: at most chat.maxPerWindow in any
	 * chat.windowSeconds. If so, records it. Real seconds, as pings count.
	 */
	VEYRAMATCH_API bool Allow(TArray<double>& SentAt, double Now, const FVeyraChatTuning& Tuning);

	/**
	 * Whether a reader receives a message on Channel from a sender on SenderSide (Chat Bible §2): Team
	 * reaches the sender's side, All every reader whose All Chat is on; a reader who muted the sender
	 * receives neither.
	 */
	VEYRAMATCH_API bool Receives(EVeyraChatChannel Channel, EVeyraTeam SenderSide, EVeyraTeam ReaderSide, bool bReaderAllChat, bool bReaderMutedSender);

	/** Keeps a client's newest chat.keepMessages messages. */
	VEYRAMATCH_API void Forget(TArray<FVeyraReceivedChat>& Held, const FVeyraChatTuning& Tuning);
}
