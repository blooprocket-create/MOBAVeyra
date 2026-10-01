// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Chat/VeyraChatRules.h"

#include "Tuning/VeyraMatchTuning.h"

namespace VeyraChat
{
FString Clean(const FString& Text)
{
	FString Out;
	Out.Reserve(Text.Len());
	for (const TCHAR Character : Text)
	{
		// Control characters never reach another screen: no line breaks, tabs or escapes.
		Out.AppendChar(FChar::IsControl(Character) ? TEXT(' ') : Character);
	}
	Out.TrimStartAndEndInline();
	return Out;
}

EVeyraChatRefusal CheckText(const FString& Cleaned, const FVeyraChatTuning& Tuning)
{
	if (Cleaned.IsEmpty())
	{
		return EVeyraChatRefusal::Empty;
	}
	return Cleaned.Len() > Tuning.MaxCharacters ? EVeyraChatRefusal::TooLong : EVeyraChatRefusal::None;
}

bool Allow(TArray<double>& SentAt, double Now, const FVeyraChatTuning& Tuning)
{
	SentAt.RemoveAll([Now, &Tuning](double At) { return Now - At >= Tuning.WindowSeconds; });
	if (SentAt.Num() >= Tuning.MaxPerWindow)
	{
		return false;
	}
	SentAt.Add(Now);
	return true;
}

bool IsChannel(EVeyraChatChannel Channel)
{
	return Channel == EVeyraChatChannel::Team || Channel == EVeyraChatChannel::All;
}

bool Receives(EVeyraChatChannel Channel, EVeyraTeam SenderSide, EVeyraTeam ReaderSide, bool bReaderAllChat, bool bReaderMutedSender)
{
	const bool bSeated = ReaderSide == EVeyraTeam::A || ReaderSide == EVeyraTeam::B;
	if (bReaderMutedSender || !bSeated)
	{
		return false;
	}
	switch (Channel)
	{
	case EVeyraChatChannel::Team:
		return ReaderSide == SenderSide;
	case EVeyraChatChannel::All:
		return bReaderAllChat;
	}
	return false;
}

void Forget(TArray<FVeyraReceivedChat>& Held, const FVeyraChatTuning& Tuning)
{
	const int32 Excess = Held.Num() - Tuning.KeepMessages;
	if (Excess > 0)
	{
		Held.RemoveAt(0, Excess);
	}
}
}
