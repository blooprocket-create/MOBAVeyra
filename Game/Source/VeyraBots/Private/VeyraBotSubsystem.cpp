// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraBotSubsystem.h"

#include "Bots/VeyraMatchEvents.h"
#include "Brain/VeyraBotBrainComponent.h"
#include "Tuning/VeyraBotsTuningSubsystem.h"
#include "VeyraBotsLog.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardController.h"

void UVeyraBotSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UVeyraMatchEvents* Events = Collection.InitializeDependency<UVeyraMatchEvents>();
	if (Events)
	{
		BotAddedHandle = Events->OnBotAdded.AddUObject(this, &UVeyraBotSubsystem::OnBotAdded);
	}
}

void UVeyraBotSubsystem::Deinitialize()
{
	if (UVeyraMatchEvents* Events = GetWorld() ? GetWorld()->GetSubsystem<UVeyraMatchEvents>() : nullptr)
	{
		Events->OnBotAdded.Remove(BotAddedHandle);
	}
	Super::Deinitialize();
}

void UVeyraBotSubsystem::OnBotAdded(AVeyraPlayerState& Bot, const FVeyraBotSeat& Seat)
{
	AVeyraVanguardController* Controller = Bot.GetVanguardController();
	const TArray<EVeyraBotRole>& Roles = UVeyraBotsTuningSubsystem::Get().Roles;
	if (!Controller || Roles.IsEmpty())
	{
		return;
	}
	// Later seats than the roles list wrap around it.
	const EVeyraBotRole Role = Roles[Seat.Seat % Roles.Num()];
	UVeyraBotBrainComponent* Brain = NewObject<UVeyraBotBrainComponent>(Controller);
	Brain->Configure(Bot, Role, Seat.Difficulty, static_cast<int32>(HashCombine(GetTypeHash(Bot.GetPlayerId()), GetTypeHash(Seat.Seat))));
	Brain->RegisterComponent();
	UE_LOG(LogVeyraBots, Log, TEXT("%s plays %s as a %s bot."), *Bot.GetPlayerName(), *StaticEnum<EVeyraBotRole>()->GetNameStringByValue(static_cast<int64>(Role)), LexToString(Seat.Difficulty));
}
