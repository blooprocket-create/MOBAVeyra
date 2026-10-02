// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Client/VeyraClientFlowTypes.h"
#include "Internationalization/Text.h"

/** One chat line as a panel shows it (ADR-046 §6). */
struct FVeyraChatLineModel
{
	/** "You", or the sender's name. */
	FText Sender;
	FText Text;
	/** "Sending…" while the player's own line awaits its answer, or why it did not go; empty once sent. */
	FText Status;
	bool bOwn = false;
	/** A party line in a panel that shows another conversation too, marked as the party's (UX-34). */
	bool bParty = false;
	bool bFailed = false;
};

/** A chat panel: one conversation, its lines and its composer (ADR-046 §6). */
struct FVeyraChatPanelModel
{
	/** Whether the panel shows at all. */
	bool bVisible = false;
	/** What the composer sends to: a party, a friend, the select's team or the post-match chat. */
	VeyraBackendProtocol::EChatKind Kind = VeyraBackendProtocol::EChatKind::Party;
	/** The friend of a direct conversation; empty otherwise. */
	FString Target;
	/** "Party Chat", the friend's name, "Team Chat" or "Post-Match Chat". */
	FText Title;
	/** Oldest first. */
	TArray<FVeyraChatLineModel> Lines;
	/** What the empty composer says, such as "Message your party". */
	FText Hint;
	/** Shown in place of the lines while there are none. */
	FText Empty;
	bool bCanSend = false;
	/** A direct conversation closes back to Party Chat. */
	bool bCanClose = false;
};

/** The chat panels' models: pure, so the screens and their tests share them (ADR-046 §6). */
namespace VeyraChatModels
{
	/** One line, as the player sees it. MarkParty marks a party line in a panel that mixes conversations. */
	VEYRAUI_API FVeyraChatLineModel DescribeLine(const FVeyraChatEntry& Entry, const FString& PlayerId, bool bMarkParty);

	/** Why the player's own line did not go, from its failure code. */
	VEYRAUI_API FText DescribeFailure(const FString& Code);

	/**
	 * The shell's sidebar chat (UX-3): the direct conversation the player opened, else the party's while the
	 * player has a party or its lines. Hidden otherwise.
	 */
	VEYRAUI_API FVeyraChatPanelModel DescribeSidebar(const FVeyraClientSnapshot& Snapshot, bool bCanSend);

	/** What the chat panels show, for the screen's rebuild signature. */
	VEYRAUI_API FString Signature(const FVeyraChat& Chat);
}
