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
	/** Champion select's composer names its recipient, Team or Party, as the draft stands (UX-34). */
	bool bShowsRecipient = false;
	/** What came of the player's last command in the panel, such as a mute; empty for none. */
	FText Notice;
};

/** What a line typed in the post-match chat asks for (ADR-046 §5). */
enum class EVeyraPostMatchCommandKind : uint8
{
	/** A message to both teams. */
	Send,
	Mute,
	Unmute,
	/** "/mute" or "/unmute" naming no one whose line the chat shows. */
	NoSuchSpeaker,
};

struct FVeyraPostMatchCommand
{
	EVeyraPostMatchCommandKind Kind = EVeyraPostMatchCommandKind::Send;
	/** Mute and Unmute: the participant's account. */
	FString AccountId;
	/** Mute, Unmute and NoSuchSpeaker: the name as the chat shows it, or as typed. */
	FString Name;
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

	/**
	 * Champion select's one chat panel (UX-33–34): the team's lines and the party's, marked as the party's, in the
	 * backend's order, with the player's unanswered lines last. It sends to the team unless the draft says /p.
	 */
	VEYRAUI_API FVeyraChatPanelModel DescribeSelectChat(const FVeyraClientSnapshot& Snapshot, bool bCanSend);

	/**
	 * Where a champion-select draft goes (UX-33): a leading "/p" sends what follows it to Party Chat, anything else
	 * goes to the team as typed. Text is what is sent, with the command taken off.
	 */
	VEYRAUI_API VeyraBackendProtocol::EChatKind SelectRecipient(const FString& Draft, FString& OutText);

	/** The composer's recipient as the player reads it: "Team" or "Party" (UX-34). */
	VEYRAUI_API FText RecipientLabel(VeyraBackendProtocol::EChatKind Kind);

	/**
	 * The results screen's post-match chat (UX-59–60): before the player's first message, only an invitation to
	 * say something; after it, what was said from then on, without the players they muted. Hidden without a match.
	 */
	VEYRAUI_API FVeyraChatPanelModel DescribePostMatch(const FVeyraClientSnapshot& Snapshot, bool bCanSend);

	/**
	 * What a post-match draft asks for: "/mute <name>" and "/unmute <name>" name another player whose line the chat
	 * shows (as in a match, ADR-029 §3); anything else is a message.
	 */
	VEYRAUI_API FVeyraPostMatchCommand ParsePostMatch(const FString& Draft, const FVeyraChat& Chat, const FString& PlayerId);

	/** What came of a post-match mute, unmute or a name the chat does not show, for the panel's notice. */
	VEYRAUI_API FText PostMatchNotice(const FVeyraPostMatchCommand& Command);

	/** What the chat panels show, for the screen's rebuild signature. */
	VEYRAUI_API FString Signature(const FVeyraChat& Chat);
}
