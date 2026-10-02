// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Chat/VeyraChatTypes.h"
#include "Containers/ArrayView.h"
#include "Misc/Optional.h"

/** What a line typed in the chat composer asks for (ADR-029 §3, §5). */
enum class EVeyraChatCommandKind : uint8
{
	/** Nothing to send. */
	Nothing,
	Send,
	Mute,
	Unmute,
	/** "/p <text>": Party Chat, which the backend carries (ADR-046 §6). */
	Party,
	/** "/r <text>": a direct message to the friend who sent the latest one. */
	Reply,
	/** "/msg <name> <text>": a direct message to the friend called Name. */
	Message,
	/** A command the composer does not know. */
	Unknown,
};

struct FVeyraChatCommand
{
	EVeyraChatCommandKind Kind = EVeyraChatCommandKind::Nothing;
	/** Send: where to. Send, Party, Reply and Message: what. */
	EVeyraChatChannel Channel = EVeyraChatChannel::Team;
	FString Text;
	/** Mute and Unmute: the participant's name. Message: the friend's. Unknown: the command as typed. */
	FString Name;
};

/** A participant the composer can name. */
struct FVeyraChatParticipant
{
	int32 PlayerId = INDEX_NONE;
	FString Name;
};

/** The chat composer's commands (ADR-029 §5, §8). */
namespace VeyraChatCommands
{
	/**
	 * What Typed asks for, sent on Chosen unless it says otherwise: "/all <text>" sends that one message
	 * to All Chat, "/mute <name>" and "/unmute <name>" mute and unmute a participant, "/p <text>" goes to
	 * Party Chat, "/r <text>" answers the latest direct message and "/msg <name> <text>" messages a friend
	 * (ADR-046 §6). Any other leading "/" is an unknown command. Anything else is a message.
	 */
	VEYRAUI_API FVeyraChatCommand Parse(const FString& Typed, EVeyraChatChannel Chosen);

	/** The PlayerId of the one participant called Name, ignoring case and surrounding spaces; unset when none or several are. */
	VEYRAUI_API TOptional<int32> FindPlayer(const FString& Name, TConstArrayView<FVeyraChatParticipant> Participants);
}
