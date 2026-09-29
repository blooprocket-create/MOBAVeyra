// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraBotSubsystem.h"

#include "Bots/VeyraMatchEvents.h"
#include "Brain/VeyraBotBrainComponent.h"
#include "Engine/World.h"
#include "Tuning/VeyraBotsTuningSubsystem.h"
#include "VeyraBotsLog.h"
#include "VeyraGameMode.h"
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
	const TArray<FVeyraBotSeatTuning>& Seats = UVeyraBotsTuningSubsystem::Get().Seats;
	if (!Controller || Seats.IsEmpty())
	{
		return;
	}
	// Later seats than the list wrap around it.
	const FVeyraBotSeatTuning& Place = Seats[Seat.Seat % Seats.Num()];
	const EVeyraBotRole Role = Place.Role;
	// It takes its seat's Flux Spells, as a player takes theirs from champion select (ADR-015 §8).
	if (AVeyraGameMode* GameMode = GetWorld()->GetAuthGameMode<AVeyraGameMode>())
	{
		GameMode->EquipStartingFluxSpells(Bot, Place.FluxSpells);
	}
	UVeyraBotBrainComponent* Brain = NewObject<UVeyraBotBrainComponent>(Controller);
	Brain->Configure(Bot, Role, Seat.Difficulty, static_cast<int32>(HashCombine(GetTypeHash(Bot.GetPlayerId()), GetTypeHash(Seat.Seat))));
	Brain->RegisterComponent();
	UE_LOG(LogVeyraBots, Log, TEXT("%s plays %s as a %s bot."), *Bot.GetPlayerName(), *StaticEnum<EVeyraBotRole>()->GetNameStringByValue(static_cast<int64>(Role)), LexToString(Seat.Difficulty));
}
