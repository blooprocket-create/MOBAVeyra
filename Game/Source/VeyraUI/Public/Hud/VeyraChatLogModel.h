// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Chat/VeyraChatTypes.h"
#include "Client/VeyraClientFlowTypes.h"
#include "Containers/ArrayView.h"
#include "Math/Vector2D.h"
#include "Templates/Function.h"

class UVeyraGreyboxSettings;

/** How the viewer stands to a chat line's sender. */
enum class EVeyraChatLineSide : uint8
{
	/** The viewer or a teammate. */
	Ally,
	Enemy,
	/** A line of the client's own, such as a refusal. */
	Notice,
	/** A Party Chat line, which the backend carries (ADR-046 §6). */
	Party,
	/** A direct message from or to a friend. */
	Direct,
};

/** Which conversation a line from outside the match belongs to (ADR-046 §6). */
enum class EVeyraOutsideChatKind : uint8
{
	Party,
	/** A friend's direct message to the player. */
	DirectFrom,
	/** The player's own direct message to a friend. */
	DirectTo,
};

/** A chat line the backend carries into the match: the party's, or a friend's, never the match server's (ADR-046 §6). */
struct FVeyraOutsideChat
{
	EVeyraOutsideChatKind Kind = EVeyraOutsideChatKind::Party;
	/** The sender's name; for a direct message, the friend's, whoever sent it. */
	FString Name;
	FString Text;
	/** "sending" or "not sent" for the player's own line; empty once sent. */
	FString Status;
	/** When it arrived, on the same real-time clock as the match's lines. */
	double ReceivedAt = 0.0;
};

/** A chat line as the HUD draws it (ADR-029 §5). */
struct FVeyraChatLine
{
	/** The match time, if the player shows timestamps, and the channel: "[05:12] [All] ". Empty for a notice. */
	FString Prefix;
	/** "Name (Vanguard): ", in the side's colour. Empty for a notice. */
	FString Sender;
	FString Text;
	EVeyraChatLineSide Side = EVeyraChatLineSide::Notice;
	/** 1 while fresh, down to 0 as it fades. */
	double Opacity = 1.0;
};

/** The log's presentation, from the player's settings over the developer's. */
struct FVeyraChatLogPreferences
{
	/** How many of the newest lines show at once. */
	int32 Lines = 0;
	/** How long a line stays whole, then how long it takes to fade away, in seconds. */
	double FadeSeconds = 0.0;
	double FadeOutSeconds = 0.0;
	/** Lines start with the match time they arrived at. */
	bool bTimestamps = false;
};

/** Where the chat log and its composer sit, in screen pixels. */
struct FVeyraChatFrame
{
	/** The log's left edge and its bottom, just above the composer. */
	FVector2D LogBottomLeft = FVector2D::ZeroVector;
	double Width = 0.0;
	/** The composer's top-left corner and size. */
	FVector2D InputTopLeft = FVector2D::ZeroVector;
	FVector2D InputSize = FVector2D::ZeroVector;
};

/** The HUD's chat log at the bottom left (Chat & Communication Bible §2; ADR-029 §5). */
namespace VeyraChatLog
{
	/**
	 * The lines to draw, oldest first: the newest Preferences.Lines of Chat. While the player composes,
	 * every one shows whole; otherwise each fades after Preferences.FadeSeconds from its arrival at Now.
	 * VanguardOf names a sender's Vanguard, or gives nothing.
	 */
	VEYRAUI_API TArray<FVeyraChatLine> Describe(TConstArrayView<FVeyraReceivedChat> Chat, double Now, bool bComposing, const FVeyraChatLogPreferences& Preferences,
		EVeyraTeam ViewerSide, TFunctionRef<FString(int32 PlayerId)> VanguardOf);

	/** The same with the party's and friends' lines from outside the match mixed in by arrival, marked as theirs (ADR-046 §6). */
	VEYRAUI_API TArray<FVeyraChatLine> Describe(TConstArrayView<FVeyraReceivedChat> Chat, TConstArrayView<FVeyraOutsideChat> Outside, double Now, bool bComposing,
		const FVeyraChatLogPreferences& Preferences, EVeyraTeam ViewerSide, TFunctionRef<FString(int32 PlayerId)> VanguardOf);

	/** The party's and friends' lines the client flow holds, as the match HUD shows them. */
	VEYRAUI_API TArray<FVeyraOutsideChat> OutsideOf(const FVeyraClientSnapshot& Snapshot);

	/** A notice line's words, such as "You muted Nyx."; empty for a message. */
	VEYRAUI_API FText NoticeText(const FVeyraReceivedChat& Line);

	/** Where the log and the composer sit on a Viewport-sized screen, at the player's HUD scale. */
	VEYRAUI_API FVeyraChatFrame FrameFor(const FVector2D& Viewport, const UVeyraGreyboxSettings& Settings, float HudScale);
}
