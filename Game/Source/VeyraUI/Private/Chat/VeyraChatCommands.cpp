// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Chat/VeyraChatCommands.h"

namespace VeyraChatCommands
{
namespace
{
	/** A command's lead character. */
	constexpr TCHAR Lead = TEXT('/');

	/** Whether Line starts with the command Word, alone or before a space; if so, Rest is what follows it. */
	bool TakeCommand(const FString& Line, const TCHAR* Word, FString& Rest)
	{
		const int32 Length = FCString::Strlen(Word);
		if (!Line.StartsWith(Word, ESearchCase::IgnoreCase) || (Line.Len() > Length && !FChar::IsWhitespace(Line[Length])))
		{
			return false;
		}
		Rest = Line.Mid(Length).TrimStartAndEnd();
		return true;
	}
}

FVeyraChatCommand Parse(const FString& Typed, EVeyraChatChannel Chosen)
{
	FVeyraChatCommand Command;
	const FString Line = Typed.TrimStartAndEnd();
	if (Line.IsEmpty())
	{
		return Command;
	}
	if (Line[0] != Lead)
	{
		Command.Kind = EVeyraChatCommandKind::Send;
		Command.Channel = Chosen;
		Command.Text = Line;
		return Command;
	}
	FString Rest;
	if (TakeCommand(Line, TEXT("/all"), Rest))
	{
		Command.Kind = Rest.IsEmpty() ? EVeyraChatCommandKind::Nothing : EVeyraChatCommandKind::Send;
		Command.Channel = EVeyraChatChannel::All;
		Command.Text = Rest;
	}
	else if (TakeCommand(Line, TEXT("/mute"), Rest) || TakeCommand(Line, TEXT("/unmute"), Rest))
	{
		const bool bMute = Line.StartsWith(TEXT("/mute"), ESearchCase::IgnoreCase);
		Command.Kind = Rest.IsEmpty() ? EVeyraChatCommandKind::Nothing : bMute ? EVeyraChatCommandKind::Mute : EVeyraChatCommandKind::Unmute;
		Command.Name = Rest;
	}
	else if (TakeCommand(Line, TEXT("/p"), Rest) || TakeCommand(Line, TEXT("/r"), Rest))
	{
		const bool bParty = Line.StartsWith(TEXT("/p"), ESearchCase::IgnoreCase);
		Command.Kind = Rest.IsEmpty() ? EVeyraChatCommandKind::Nothing : bParty ? EVeyraChatCommandKind::Party : EVeyraChatCommandKind::Reply;
		Command.Text = Rest;
	}
	else if (TakeCommand(Line, TEXT("/msg"), Rest))
	{
		// A display name has no spaces, so the first word is the friend's (Profiles & Identity Bible §4).
		int32 Space = INDEX_NONE;
		const bool bHasText = Rest.FindChar(TEXT(' '), Space);
		Command.Name = bHasText ? Rest.Left(Space) : Rest;
		Command.Text = bHasText ? Rest.Mid(Space + 1).TrimStartAndEnd() : FString();
		Command.Kind = Command.Name.IsEmpty() || Command.Text.IsEmpty() ? EVeyraChatCommandKind::Nothing : EVeyraChatCommandKind::Message;
	}
	else
	{
		Command.Kind = EVeyraChatCommandKind::Unknown;
		int32 Space = INDEX_NONE;
		Command.Name = Line.FindChar(TEXT(' '), Space) ? Line.Left(Space) : Line;
	}
	return Command;
}

TOptional<int32> FindPlayer(const FString& Name, TConstArrayView<FVeyraChatParticipant> Participants)
{
	const FString Wanted = Name.TrimStartAndEnd();
	TOptional<int32> Found;
	for (const FVeyraChatParticipant& Participant : Participants)
	{
		if (Participant.Name.TrimStartAndEnd().Equals(Wanted, ESearchCase::IgnoreCase))
		{
			if (Found.IsSet())
			{
				return {};
			}
			Found = Participant.PlayerId;
		}
	}
	return Found;
}
}
