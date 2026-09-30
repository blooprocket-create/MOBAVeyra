// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Chat/VeyraChatSubsystem.h"

#include "Chat/VeyraChatRules.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "VeyraGameState.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"

EVeyraChatRefusal UVeyraChatSubsystem::Send(const AVeyraPlayerState& Sender, EVeyraChatChannel Channel, const FString& Text)
{
	const EVeyraTeam Side = Sender.GetVeyraTeam();
	if (Sender.IsABot() || (Side != EVeyraTeam::A && Side != EVeyraTeam::B))
	{
		return EVeyraChatRefusal::NotAPlayer;
	}
	const AVeyraGameState* GameState = GetWorld()->GetGameState<AVeyraGameState>();
	const EVeyraMatchPhase Phase = GameState ? GameState->GetPhase() : EVeyraMatchPhase::Loading;
	// A pause stops play, not talk (Match Flow Bible §10.2); after the match, post-match chat is another space (§5).
	if (Phase != EVeyraMatchPhase::Preparation && Phase != EVeyraMatchPhase::Live)
	{
		return EVeyraChatRefusal::NotNow;
	}
	const FVeyraChatTuning& Tuning = UVeyraMatchTuningSubsystem::Get().Chat;
	const FString Cleaned = VeyraChat::Clean(Text);
	if (const EVeyraChatRefusal Refusal = VeyraChat::CheckText(Cleaned, Tuning); Refusal != EVeyraChatRefusal::None)
	{
		return Refusal;
	}
	// With All Chat off, a player neither sends to it nor receives it (§2).
	if (Channel == EVeyraChatChannel::All && !IsAllChatOn(Sender.GetPlayerId()))
	{
		return EVeyraChatRefusal::AllChatOff;
	}
	if (!VeyraChat::Allow(SentAt.FindOrAdd(Sender.GetPlayerId()), FPlatformTime::Seconds(), Tuning))
	{
		return EVeyraChatRefusal::TooMany;
	}
	FVeyraChatMessage Sent;
	Sent.SenderId = Sender.GetPlayerId();
	Sent.SenderName = Sender.GetPlayerName();
	Sent.SenderTeam = Side;
	Sent.Channel = Channel;
	Sent.Text = Cleaned;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AVeyraPlayerController* Controller = Cast<AVeyraPlayerController>(It->Get());
		const AVeyraPlayerState* Reader = Controller ? Controller->GetPlayerState<AVeyraPlayerState>() : nullptr;
		if (Reader && !Reader->IsABot()
			&& VeyraChat::Receives(Channel, Side, Reader->GetVeyraTeam(), IsAllChatOn(Reader->GetPlayerId()), IsMuted(Reader->GetPlayerId(), Sent.SenderId)))
		{
			Controller->DeliverChat(Sent);
		}
	}
	return EVeyraChatRefusal::None;
}

void UVeyraChatSubsystem::SetMuted(const AVeyraPlayerState& Muter, int32 MutedId, bool bMuted)
{
	TSet<int32>& Muted = Mutes.FindOrAdd(Muter.GetPlayerId());
	if (bMuted && MutedId != Muter.GetPlayerId())
	{
		Muted.Add(MutedId);
	}
	else
	{
		Muted.Remove(MutedId);
	}
}

void UVeyraChatSubsystem::SetAllChat(const AVeyraPlayerState& Player, bool bOn)
{
	if (bOn)
	{
		AllChatOff.Remove(Player.GetPlayerId());
	}
	else
	{
		AllChatOff.Add(Player.GetPlayerId());
	}
}

bool UVeyraChatSubsystem::IsMuted(int32 MuterId, int32 MutedId) const
{
	const TSet<int32>* Muted = Mutes.Find(MuterId);
	return Muted && Muted->Contains(MutedId);
}
