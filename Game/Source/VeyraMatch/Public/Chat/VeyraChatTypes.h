// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Teams/VeyraTeam.h"

#include "VeyraChatTypes.generated.h"

/** Who a chat message goes to (Chat & Communication Bible §2; ADR-029 §1). */
UENUM()
enum class EVeyraChatChannel : uint8
{
	/** The sender's side's human players. */
	Team,
	/** Both sides' human players who keep All Chat on. */
	All,
};

/** Why a chat message was not sent. */
UENUM()
enum class EVeyraChatRefusal : uint8
{
	None,
	/** Only a seated human player chats. */
	NotAPlayer,
	/** Chat needs the match: from preparation until it ends. */
	NotNow,
	/** Nothing to send once control characters and surrounding spaces are gone. */
	Empty,
	/** Longer than chat.maxCharacters. */
	TooLong,
	/** Too many in a short time (chat.maxPerWindow in any chat.windowSeconds). */
	TooMany,
	/** All Chat is off for the sender (Chat Bible §2). */
	AllChatOff,
	/** Neither Team nor All: no client of this build sends one. */
	UnknownChannel,
};

/** A chat message as its recipients receive it. */
USTRUCT()
struct FVeyraChatMessage
{
	GENERATED_BODY()

	/** The PlayerId of the participant who sent it. */
	UPROPERTY()
	int32 SenderId = INDEX_NONE;

	UPROPERTY()
	FString SenderName;

	UPROPERTY()
	EVeyraTeam SenderTeam = EVeyraTeam::None;

	UPROPERTY()
	EVeyraChatChannel Channel = EVeyraChatChannel::Team;

	UPROPERTY()
	FString Text;
};

/** A line a client adds to its own chat log: not a message anyone sent (ADR-029 §2, §3). */
enum class EVeyraChatNotice : uint8
{
	/** A message someone sent. */
	None,
	/** The server refused the player's message; Refusal says why. */
	Refused,
	/** The player muted, or unmuted, the participant named in Message.SenderName. */
	Muted,
	Unmuted,
	/** No participant has the name in Message.SenderName. */
	NoSuchPlayer,
	/** The composer knows no command by the name in Message.Text. */
	UnknownCommand,
	/** "/r" with no direct message to answer (ADR-046 §6). */
	NoReplyTarget,
	/** "/msg" naming no friend: the name is in Message.SenderName. */
	NoSuchFriend,
	/** Party Chat and direct messages need the backend, which this game does not reach (a game without a launcher). */
	OutsideUnavailable,
};

/** A chat line a client holds: when it arrived, in real seconds and on the match clock, and what it is. */
struct FVeyraReceivedChat
{
	FVeyraChatMessage Message;
	double ReceivedAt = 0.0;
	double MatchSeconds = 0.0;
	EVeyraChatNotice Notice = EVeyraChatNotice::None;
	EVeyraChatRefusal Refusal = EVeyraChatRefusal::None;
};
