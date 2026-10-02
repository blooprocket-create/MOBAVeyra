// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Hud/VeyraChatLogModel.h"

#include "Algo/StableSort.h"
#include "Greybox/VeyraGreyboxSettings.h"

#define LOCTEXT_NAMESPACE "VeyraChatLog"

namespace VeyraChatLog
{
namespace
{
	constexpr int32 SecondsPerMinute = 60;

	FText RefusalText(EVeyraChatRefusal Refusal)
	{
		switch (Refusal)
		{
		case EVeyraChatRefusal::None:
			break;
		case EVeyraChatRefusal::NotAPlayer:
			return LOCTEXT("NotAPlayer", "Only the match's players can chat.");
		case EVeyraChatRefusal::NotNow:
			return LOCTEXT("NotNow", "Chat is closed right now.");
		case EVeyraChatRefusal::Empty:
			return LOCTEXT("Empty", "There is nothing to send.");
		case EVeyraChatRefusal::TooLong:
			return LOCTEXT("TooLong", "That message is too long.");
		case EVeyraChatRefusal::TooMany:
			return LOCTEXT("TooMany", "You are sending messages too quickly. Wait a moment.");
		case EVeyraChatRefusal::AllChatOff:
			return LOCTEXT("AllChatOff", "All Chat is off. Turn it on in Settings, under Communication.");
		case EVeyraChatRefusal::UnknownChannel:
			return LOCTEXT("UnknownChannel", "That chat channel does not exist.");
		}
		return FText::GetEmpty();
	}

	/** 1 while Line is fresh, falling to 0 as it fades. */
	double OpacityOf(double ReceivedAt, double Now, bool bComposing, const FVeyraChatLogPreferences& Preferences)
	{
		if (bComposing)
		{
			return 1.0;
		}
		const double Past = Now - ReceivedAt - Preferences.FadeSeconds;
		if (Past <= 0.0)
		{
			return 1.0;
		}
		return Preferences.FadeOutSeconds > 0.0 ? FMath::Clamp(1.0 - Past / Preferences.FadeOutSeconds, 0.0, 1.0) : 0.0;
	}

	FString Clock(double Seconds)
	{
		const int32 Whole = FMath::Max(0, FMath::FloorToInt32(Seconds));
		return FString::Printf(TEXT("[%02d:%02d] "), Whole / SecondsPerMinute, Whole % SecondsPerMinute);
	}
}

TArray<FVeyraChatLine> Describe(TConstArrayView<FVeyraReceivedChat> Chat, double Now, bool bComposing, const FVeyraChatLogPreferences& Preferences,
	EVeyraTeam ViewerSide, TFunctionRef<FString(int32 PlayerId)> VanguardOf)
{
	return Describe(Chat, TConstArrayView<FVeyraOutsideChat>(), Now, bComposing, Preferences, ViewerSide, VanguardOf);
}

TArray<FVeyraChatLine> Describe(TConstArrayView<FVeyraReceivedChat> Chat, TConstArrayView<FVeyraOutsideChat> Outside, double Now, bool bComposing,
	const FVeyraChatLogPreferences& Preferences, EVeyraTeam ViewerSide, TFunctionRef<FString(int32 PlayerId)> VanguardOf)
{
	// The match's lines and the backend's, in the order they arrived; the match's first where two arrived at once.
	struct FArrival
	{
		double At = 0.0;
		const FVeyraReceivedChat* Match = nullptr;
		const FVeyraOutsideChat* Outside = nullptr;
	};
	TArray<FArrival> Arrivals;
	for (const FVeyraReceivedChat& Received : Chat)
	{
		Arrivals.Add({ Received.ReceivedAt, &Received, nullptr });
	}
	for (const FVeyraOutsideChat& Line : Outside)
	{
		Arrivals.Add({ Line.ReceivedAt, nullptr, &Line });
	}
	Algo::StableSortBy(Arrivals, &FArrival::At);
	TArray<FVeyraChatLine> Lines;
	const int32 First = FMath::Max(0, Arrivals.Num() - FMath::Max(0, Preferences.Lines));
	for (int32 Index = First; Index < Arrivals.Num(); ++Index)
	{
		const FArrival& Arrival = Arrivals[Index];
		const double Opacity = OpacityOf(Arrival.At, Now, bComposing, Preferences);
		if (Opacity <= 0.0)
		{
			continue;
		}
		FVeyraChatLine& Line = Lines.AddDefaulted_GetRef();
		Line.Opacity = Opacity;
		if (const FVeyraOutsideChat* From = Arrival.Outside)
		{
			// The party's and friends' lines, which the backend carries, are marked as theirs (ADR-046 §6).
			Line.Side = From->Kind == EVeyraOutsideChatKind::Party ? EVeyraChatLineSide::Party : EVeyraChatLineSide::Direct;
			Line.Prefix = (From->Kind == EVeyraOutsideChatKind::Party ? LOCTEXT("PartyChannel", "[Party] ")
							  : From->Kind == EVeyraOutsideChatKind::DirectFrom ? LOCTEXT("FromChannel", "[From] ")
																				 : LOCTEXT("ToChannel", "[To] "))
							  .ToString();
			Line.Sender = FString::Printf(TEXT("%s: "), *From->Name);
			Line.Text = From->Status.IsEmpty() ? From->Text : FString::Printf(TEXT("%s (%s)"), *From->Text, *From->Status);
			continue;
		}
		const FVeyraReceivedChat& Received = *Arrival.Match;
		if (Received.Notice != EVeyraChatNotice::None)
		{
			Line.Text = NoticeText(Received).ToString();
			continue;
		}
		const FVeyraChatMessage& Message = Received.Message;
		Line.Side = Message.SenderTeam == ViewerSide ? EVeyraChatLineSide::Ally : EVeyraChatLineSide::Enemy;
		Line.Prefix = (Preferences.bTimestamps ? Clock(Received.MatchSeconds) : FString())
			+ (Message.Channel == EVeyraChatChannel::All ? LOCTEXT("AllChannel", "[All] ") : LOCTEXT("TeamChannel", "[Team] ")).ToString();
		const FString Vanguard = VanguardOf(Message.SenderId);
		Line.Sender = Vanguard.IsEmpty() ? FString::Printf(TEXT("%s: "), *Message.SenderName) : FString::Printf(TEXT("%s (%s): "), *Message.SenderName, *Vanguard);
		Line.Text = Message.Text;
	}
	return Lines;
}

TArray<FVeyraOutsideChat> OutsideOf(const FVeyraClientSnapshot& Snapshot)
{
	TArray<FVeyraOutsideChat> Out;
	const auto StatusOf = [](const FVeyraChatEntry& Entry) {
		return Entry.bPending ? LOCTEXT("OutsideSending", "sending").ToString() : Entry.Failure.IsEmpty() ? FString() : LOCTEXT("OutsideNotSent", "not sent").ToString();
	};
	for (const FVeyraChatEntry& Entry : Snapshot.Chat.Party.Lines)
	{
		const bool bOwn = Entry.SenderId == Snapshot.AccountId;
		Out.Add({ EVeyraOutsideChatKind::Party, bOwn ? Snapshot.DisplayName : Entry.SenderName, Entry.Text, StatusOf(Entry), Entry.ArrivedAt });
	}
	for (const TPair<FString, FVeyraChatConversation>& Direct : Snapshot.Chat.Direct)
	{
		const VeyraBackendProtocol::FAccount* Friend = Snapshot.Social.Friends.Friends.FindByPredicate(
			[&Direct](const VeyraBackendProtocol::FAccount& Account) { return Account.Id == Direct.Key; });
		for (const FVeyraChatEntry& Entry : Direct.Value.Lines)
		{
			const bool bOwn = Entry.SenderId == Snapshot.AccountId;
			// The friend's name, whoever sent it: the player reads "[To] DevTwo" for their own.
			const FString Name = !bOwn ? Entry.SenderName : Friend ? Friend->DisplayName : FString(LOCTEXT("OutsideAFriend", "a friend").ToString());
			Out.Add({ bOwn ? EVeyraOutsideChatKind::DirectTo : EVeyraOutsideChatKind::DirectFrom, Name, Entry.Text, StatusOf(Entry), Entry.ArrivedAt });
		}
	}
	return Out;
}

FText NoticeText(const FVeyraReceivedChat& Line)
{
	const FText Subject = FText::FromString(Line.Notice == EVeyraChatNotice::UnknownCommand ? Line.Message.Text : Line.Message.SenderName);
	switch (Line.Notice)
	{
	case EVeyraChatNotice::None:
		break;
	case EVeyraChatNotice::Refused:
		return RefusalText(Line.Refusal);
	case EVeyraChatNotice::Muted:
		return FText::Format(LOCTEXT("Muted", "You muted {0} for this match."), Subject);
	case EVeyraChatNotice::Unmuted:
		return FText::Format(LOCTEXT("Unmuted", "You unmuted {0}."), Subject);
	case EVeyraChatNotice::NoSuchPlayer:
		return FText::Format(LOCTEXT("NoSuchPlayer", "No one in this match is called {0}."), Subject);
	case EVeyraChatNotice::UnknownCommand:
		return FText::Format(LOCTEXT("UnknownCommand", "{0} is not a command. Try /all, /p, /r, /msg, /mute or /unmute."), Subject);
	case EVeyraChatNotice::NoReplyTarget:
		return LOCTEXT("NoReplyTarget", "No friend has messaged you yet.");
	case EVeyraChatNotice::NoSuchFriend:
		return FText::Format(LOCTEXT("NoSuchFriend", "You have no friend called {0}."), Subject);
	case EVeyraChatNotice::OutsideUnavailable:
		return LOCTEXT("OutsideUnavailable", "Party Chat and friend messages need the Veyra launcher.");
	}
	return FText::GetEmpty();
}

FVeyraChatFrame FrameFor(const FVector2D& Viewport, const UVeyraGreyboxSettings& Settings, float HudScale)
{
	const double Scale = (Settings.HudReferenceHeight > 0.0f ? Viewport.Y / Settings.HudReferenceHeight : 1.0) * HudScale;
	FVeyraChatFrame Frame;
	Frame.Width = Settings.ChatWidth * Scale;
	Frame.InputSize = FVector2D(Frame.Width, Settings.ChatInputHeight * Scale);
	Frame.InputTopLeft = FVector2D(Settings.HudMargin * Scale, Viewport.Y - Settings.ChatBottomOffset * Scale - Frame.InputSize.Y);
	Frame.LogBottomLeft = FVector2D(Frame.InputTopLeft.X, Frame.InputTopLeft.Y - Settings.DeckGap * Scale);
	return Frame;
}
}

#undef LOCTEXT_NAMESPACE
