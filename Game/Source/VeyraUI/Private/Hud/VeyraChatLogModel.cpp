// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Hud/VeyraChatLogModel.h"

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
		}
		return FText::GetEmpty();
	}

	/** 1 while Line is fresh, falling to 0 as it fades. */
	double OpacityOf(const FVeyraReceivedChat& Line, double Now, bool bComposing, const FVeyraChatLogPreferences& Preferences)
	{
		if (bComposing)
		{
			return 1.0;
		}
		const double Past = Now - Line.ReceivedAt - Preferences.FadeSeconds;
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
	TArray<FVeyraChatLine> Lines;
	const int32 First = FMath::Max(0, Chat.Num() - FMath::Max(0, Preferences.Lines));
	for (int32 Index = First; Index < Chat.Num(); ++Index)
	{
		const FVeyraReceivedChat& Received = Chat[Index];
		const double Opacity = OpacityOf(Received, Now, bComposing, Preferences);
		if (Opacity <= 0.0)
		{
			continue;
		}
		FVeyraChatLine& Line = Lines.AddDefaulted_GetRef();
		Line.Opacity = Opacity;
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
		return FText::Format(LOCTEXT("UnknownCommand", "{0} is not a command. Try /all, /mute or /unmute."), Subject);
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
