// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Chat/VeyraChatTypes.h"
#include "Subsystems/WorldSubsystem.h"

#include "VeyraChatSubsystem.generated.h"

class AVeyraPlayerState;

/**
 * Server: in-match Team Chat and All Chat (Chat & Communication Bible §2; ADR-029). A player chats
 * through its controller; this owner checks it may, counts it against the spam limit and hands it to
 * each human player who may receive it: its side for Team, every player with All Chat on for All, never
 * one who muted the sender. Recipients are decided here, at delivery, so no client can read what it may
 * not. Messages go to each recipient's own controller, so spectators and replays never carry them (§6).
 */
UCLASS()
class VEYRAMATCH_API UVeyraChatSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Server: Sender sends Text on Channel. Returns why not, or None. */
	EVeyraChatRefusal Send(const AVeyraPlayerState& Sender, EVeyraChatChannel Channel, const FString& Text);

	/** Server: Muter stops, or starts again, receiving the chat of the player MutedId, for this match. */
	void SetMuted(const AVeyraPlayerState& Muter, int32 MutedId, bool bMuted);

	/** Server: whether Player takes part in All Chat, as its setting says (§2); on until it says otherwise. */
	void SetAllChat(const AVeyraPlayerState& Player, bool bOn);

	bool IsMuted(int32 MuterId, int32 MutedId) const;
	bool IsAllChatOn(int32 PlayerId) const { return !AllChatOff.Contains(PlayerId); }

private:
	/** When each player, by PlayerId, sent its recent messages, in real seconds. */
	TMap<int32, TArray<double>> SentAt;

	/** Whom each player, by PlayerId, muted. */
	TMap<int32, TSet<int32>> Mutes;

	/** The players who turned All Chat off. */
	TSet<int32> AllChatOff;
};
