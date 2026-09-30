// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Pings/VeyraPingSubsystem.h"

#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Pings/VeyraPingRules.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "VeyraGameState.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"

EVeyraPingRefusal UVeyraPingSubsystem::Ping(const AVeyraPlayerState& Sender, const FVector& Point, EVeyraPingKind Kind)
{
	const EVeyraTeam Team = Sender.GetVeyraTeam();
	if (Sender.IsABot() || (Team != EVeyraTeam::A && Team != EVeyraTeam::B))
	{
		return EVeyraPingRefusal::NotAPlayer;
	}
	const AVeyraGameState* GameState = GetWorld()->GetGameState<AVeyraGameState>();
	const EVeyraMatchPhase Phase = GameState ? GameState->GetPhase() : EVeyraMatchPhase::Loading;
	// A pause stops play, not talk (Match Flow Bible §10.2).
	if (Phase != EVeyraMatchPhase::Preparation && Phase != EVeyraMatchPhase::Live)
	{
		return EVeyraPingRefusal::NotNow;
	}
	if (!VeyraPings::Allow(SentAt.FindOrAdd(Sender.GetPlayerId()), FPlatformTime::Seconds(), UVeyraMatchTuningSubsystem::Get().Pings))
	{
		return EVeyraPingRefusal::TooMany;
	}
	FVeyraPing Sent;
	Sent.Point = Point;
	Sent.Kind = Kind;
	Sent.SenderId = Sender.GetPlayerId();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AVeyraPlayerController* Controller = Cast<AVeyraPlayerController>(It->Get());
		const AVeyraPlayerState* Member = Controller ? Controller->GetPlayerState<AVeyraPlayerState>() : nullptr;
		if (Member && Member->GetVeyraTeam() == Team)
		{
			Controller->DeliverPing(Sent);
		}
	}
	return EVeyraPingRefusal::None;
}
